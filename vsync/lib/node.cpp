/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <random>
#include <fstream>

#include <boost/log/trivial.hpp>

#include <ndn-cxx/util/digest.hpp>

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

static const int kActiveInGroup = 3;
static int kSyncDelay = 4000;
static int kInterestTransmissionTime = 3;

static time::milliseconds kSyncDuration = time::milliseconds(150);
static int kInterestDT = 20;
static time::milliseconds kWaitACKforSyncInterestInterval = time::milliseconds(kInterestDT + 3);
static time::milliseconds kInterestWT = time::milliseconds(kInterestDT + 3);
static int kSendOutInterestLifetime = kInterestDT + 3;
static int kAddToPitInterestLifetime = 54;

static const int kSnapshotNum = 150;
static time::milliseconds kSnapshotInterval = time::milliseconds(8000);
static const std::string availabilityFileName = "availability.txt";

static const int data_rate_lower_bound = 1000;
static const int data_rate_upper_bound = 8000;

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, const GroupID& gid,
           const uint64_t group_size_, Node::DataCb on_data)
           : face_(face),
             key_chain_(key_chain),
             nid_(nid),
             scheduler_(scheduler),
             prefix_(prefix),
             gid_(name::Component(gid).toUri()),
             group_size(group_size_),
             data_cb_(std::move(on_data)),
             rengine_(rdevice_()),
             rdist_(3000, 10000) {
  version_vector_ = VersionVector(group_size, 0);
  recv_window = std::vector<ReceiveWindow>(group_size);
  // data_store_ = std::vector<std::vector<std::shared_ptr<Data>>>(group_size, std::vector<std::shared_ptr<Data>>(0));
  node_state = kActive;
  energy_consumption = 0.0;
  sleeping_time = 0.0;
  receive_ack_for_sync_interest = false;
  time_slot = -1;
  sync_num = 0;
  sync_requester = false;
  outVsyncInfo = "";
  collision_num = 0;
  suppression_num = 0;
  out_interest_num = 0;
  working_time = 0.0;

  face_.setInterestFilter(
      Name(kSyncPrefix).append(gid_), std::bind(&Node::OnSyncInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register vsync prefix: " << reason); 
        throw Error("Failed to register vsync prefix: " + reason);
      });


  face_.setInterestFilter(
      Name(kSyncDataPrefix).append(gid_), std::bind(&Node::OnDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register data prefix: " << reason); 
        throw Error("Failed to register data prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kSyncACKPrefix).append(gid_), std::bind(&Node::OnSyncACKInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register sync ack interest prefix: " << reason); 
        throw Error("Failed to register sync ack interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kIncomingDataPrefix), std::bind(&Node::OnIncomingData, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register incomingData interest prefix: " << reason); 
        throw Error("Failed to register incomingData interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kIncomingInterestPrefix), std::bind(&Node::OnIncomingInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register incomingInterest interest prefix: " << reason); 
        throw Error("Failed to register incomingInterest interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kIncomignSyncACKPrefix).append(gid_), std::bind(&Node::OnIncomingSyncACKInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register incomingSyncACK interest prefix: " << reason); 
        throw Error("Failed to register incomingSyncACK interest prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
}

void Node::StartSimulation() {
  // at first, node(0) enter intermediate, and there are only other 2 active nodes.
  // if the kActiveInGroup = 3, node(1, 2) are active now. node(3) are waking up
  if (nid_ >= kActiveInGroup) {
    // node should go to sleep
    Interest i(kLocalhostSleepingCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    node_state = kSleeping;
    sleep_start = time::system_clock::now();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") go to sleep" );
  }
  else {
    node_state = kActive;
    wakeup = time::system_clock::now();
  }

  CheckState();

  scheduler_.scheduleEvent(time::milliseconds(kSyncDelay * nid_),
                           [this] { EnterIntermediateState(); });

  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });

  scheduler_.scheduleEvent(time::seconds(1200), [this] { SendGetOutVsyncInfoInterest(); });

  PublishData("Hello from " + to_string(nid_));
}

void Node::SendGetOutVsyncInfoInterest() {
  Interest i(kGetOutVsyncInfoCommand, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::PublishData(const std::string& content, uint32_t type) {
  if (node_state == kActive) {
    // sequence number increases from 1, not 0
    version_vector_[nid_]++;

    // make data dame
    auto n = MakeDataName(gid_, nid_, version_vector_[nid_]);
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    // set data content
    data->setContent(reinterpret_cast<const uint8_t*>(content.data()),
                     content.size());
    data->setContentType(type);
    key_chain_.sign(*data, signingWithSha256());

    // data_store_[nid_].push_back(data);
    data_store_[n] = data;
    recv_window[nid_].Insert(version_vector_[nid_]);

    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Publish Data: d.name=" << n.toUri() << " d.type=" << type << " d.content=" << content);
  }

  std::uniform_int_distribution<> data_rdist(data_rate_lower_bound, data_rate_upper_bound);
  scheduler_.scheduleEvent(time::milliseconds(data_rdist(rengine_)),
                           [this, content] { PublishData(content); });
}

/****************************************************************/
/* pipeline for sleeping scheduling                             */
/****************************************************************/

void Node::EnterIntermediateState() {
  assert(node_state == kActive);
  scheduler_.scheduleEvent(time::milliseconds(group_size * kSyncDelay), [this] { EnterIntermediateState(); });
  // assert -> the NFD is working now!
  Reset();
  node_state = kIntermediate;
  SyncData();
}

void Node::CheckState() {
  scheduler_.scheduleEvent(time::milliseconds(kSyncDelay), [this] { CheckState(); });
  time_slot++;
  if (time_slot == group_size) time_slot = 0;
  if (time_slot == nid_) {
    // the node currently will enter intermediate state
    return;
  }
  else if ((time_slot + kActiveInGroup) % group_size == nid_) {
    // need to wake up
    assert(node_state == kSleeping);
    Interest i(kLocalhostWakeupCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    node_state = kActive;
    auto cur_timepoint = time::system_clock::now();
    sleeping_time += time::toUnixTimestamp(cur_timepoint).count() - time::toUnixTimestamp(sleep_start).count();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") wakeup" );
    Reset();
    wakeup = time::system_clock::now();
  }
  else {
    bool isActive = false;
    for (int i = 1; i < kActiveInGroup; ++i) {
      if ((time_slot + i) % group_size == nid_) {
        assert(node_state == kActive);
        Reset();
        isActive = true;
        break;
      }
    }
    if (isActive == false) {
      if (node_state != kSleeping) {
        // force the node who doesn't finish the syncing to go to sleep
        Interest i(kLocalhostSleepingCommand);
        face_.expressInterest(i, [](const Interest&, const Data&) {},
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
        scheduler_.cancelEvent(sync_interest_scheduler);
        scheduler_.cancelEvent(sync_duration_scheduler);
        node_state = kSleeping;
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") go to sleep" );
        sleep_start = time::system_clock::now();

        time::system_clock::time_point cur_time = time::system_clock::now();
        auto cur_sync_delay = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(send_sync_interest_time).count();
        sync_delay.push_back(cur_sync_delay);
        auto active_time = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(wakeup).count();
        working_time += active_time;
      }
    }
  }
}

void Node::Reset() {
  pending_interest.clear();
  scheduler_.cancelEvent(sync_interest_scheduler);
  scheduler_.cancelEvent(sync_duration_scheduler);
  scheduler_.cancelEvent(inst_dt);
  scheduler_.cancelEvent(inst_wt);
  sync_requester = false;
  sync_responder_success = false;
  receive_sync_interest = false;
}

/****************************************************************/
/* pipeline for sync-requester node                             */
/* The sync-requester node will retransmit sync interest for    */
/* most three times if it doesn't receive any data interests or */
/* SyncACK interest                                             */
/****************************************************************/

// record the syncACK delay, used for experiments, not the design part
void Node::OnIncomingSyncACKInterest(const Interest& interest) {
  if (sync_requester == false) return;
  const auto& n = interest.getName();
  auto syncACK_receiver = n.get(-2).toNumber();
  if (syncACK_receiver != nid_) return;

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") receives incomingSyncACK Interest: name = " << n.toUri());

  // extract the information from sign
  const auto& sign = n.get(-1).toUri();
  size_t sep1 = sign.find("-");
  size_t sep2 = sign.find("-", sep1 + 1);
  NodeID syncACK_responder = std::stoll(sign.substr(0, sep1));
  uint64_t sync_index = std::stoll(sign.substr(sep1 + 1, sep2 - sep1 - 1));
  size_t pending_list_size = std::stoll(sign.substr(sep2 + 1));
  // std::cout << n.toUri() << ": " << syncACK_responder << " " << sync_index << " " << pending_list_size << std::endl;

  assert(sync_index == sync_num);

  if (receive_syncACK_responder.find(syncACK_responder) != receive_syncACK_responder.end()) return;
  receive_syncACK_responder.insert(syncACK_responder);
  if (receive_syncACK_responder.size() == 1) {
    time::system_clock::time_point receive_first_syncACK_time = time::system_clock::now();
    auto sync_ack_delay = time::toUnixTimestamp(receive_first_syncACK_time).count() - time::toUnixTimestamp(send_sync_interest_time).count();
    receive_first_syncACK_delay.push_back(std::pair<double, int>(sync_ack_delay, pending_list_size));
    receive_last_syncACK_delay.push_back(std::pair<double, int>(sync_ack_delay, pending_list_size));
  }
  else {
    time::system_clock::time_point receive_cur_syncACK_time = time::system_clock::now();
    auto sync_ack_delay = time::toUnixTimestamp(receive_cur_syncACK_time).count() - time::toUnixTimestamp(send_sync_interest_time).count();
    receive_last_syncACK_delay.back() = std::pair<double, int>(sync_ack_delay, pending_list_size);
  }
}

void Node::SyncData() {
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Start to Sync Data");
  sync_requester = true;
  receive_ack_for_sync_interest = false;
  receive_syncACK_responder.clear();
  send_sync_interest_time = time::system_clock::now();
  sync_num++;

  std::string vv_encode = EncodeVV(version_vector_);
  auto sync_interest_name = MakeSyncInterestName(gid_, nid_, vv_encode, sync_num);

  // set a timer for syncing-state
  sync_duration_scheduler = scheduler_.scheduleEvent(kSyncDuration, [this] { OnSyncDurationTimeOut(); });

  SendSyncInterest(sync_interest_name, 0);

  sync_interest_scheduler = scheduler_.scheduleEvent(kWaitACKforSyncInterestInterval, [this, sync_interest_name] {
    this->SyncInterestTimeout(sync_interest_name, 1);
  });
}

void Node::OnSyncDurationTimeOut() {
  // go to sleep
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") will go to sleep");
  Interest i(kLocalhostSleepingCommand);
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  scheduler_.cancelEvent(sync_interest_scheduler);
  node_state = kSleeping;
  sleep_start = time::system_clock::now();

  time::system_clock::time_point cur_time = time::system_clock::now();
  auto cur_sync_delay = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(send_sync_interest_time).count();
  sync_delay.push_back(cur_sync_delay);
  auto active_time = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(wakeup).count();
  working_time += active_time;
}

void Node::SendSyncInterest(const Name& sync_interest_name, const uint32_t& sync_interest_time) {
  // make the sync interest name
  if (sync_interest_time == 3) {
    // at most will only send the sync interest for three times!
    receive_ack_for_sync_interest = true;
    return;
  }

  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Send Sync Interest: i.name=" << sync_interest_name.toUri());

  Interest i(sync_interest_name, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  // if (sync_interest_size == 0) sync_interest_size = i.wireEncode().size();
}

void Node::SyncInterestTimeout(const Name& sync_interest_name, const uint32_t& sync_interest_time) {
  if (receive_ack_for_sync_interest == false) {
    SendSyncInterest(sync_interest_name, sync_interest_time);
    sync_interest_scheduler = scheduler_.scheduleEvent(kWaitACKforSyncInterestInterval, [this, sync_interest_name, sync_interest_time] {
      this->SyncInterestTimeout(sync_interest_name, sync_interest_time + 1);
    });
  }
  else {
    scheduler_.cancelEvent(sync_interest_scheduler);
    sync_interest_scheduler = scheduler_.scheduleEvent(kWaitACKforSyncInterestInterval, [this, sync_interest_name] {
      this->SyncInterestTimeout(sync_interest_name, 0);
    });
  }
}

void Node::OnSyncACKInterest(const Interest& interest) {
  const auto& n = interest.getName();
  auto syncACK_receiver = ExtractNodeID(n);

  if (node_state == kSleeping) return;
  else if (node_state == kActive) {
    if (sync_responder_success == true) {
      // send back the data to other sync_responder, because i have finished syncup data successfully
      std::shared_ptr<Data> data = std::make_shared<Data>(n);
      data->setFreshnessPeriod(time::seconds(3600));
      key_chain_.sign(*data, signingWithSha256());
      face_.put(*data);
    }
    return;
  }
  else if (node_state == kIntermediate) {
    // current one group
    assert(syncACK_receiver == nid_);
    // VSYNC_LOG_TRACE( "sync-initializer (" << gid_ << " " << nid_ << ") Receive SyncACKInterest: i.name=" << n.toUri() );
    const auto& sign = n.get(-1).toUri();
    size_t sep1 = sign.find("-");
    size_t sep2 = sign.find("-", sep1 + 1);
    NodeID syncACK_responder = std::stoll(sign.substr(0, sep1));
    uint64_t sync_index = std::stoll(sign.substr(sep1 + 1, sep2 - sep1 - 1));
    size_t pending_list_size = std::stoll(sign.substr(sep2 + 1));
    assert(sync_index == sync_num);

    VSYNC_LOG_TRACE( "sync-initializer (" << gid_ << " " << nid_ << ") Receive SyncACKInterest: i.name=" << n.toUri() << " from node " << syncACK_responder );
    assert(receive_syncACK_responder.find(syncACK_responder) != receive_syncACK_responder.end());

    // send back an empty data to ack the syncACK_sender
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);
    /*
    if (receive_syncACK_responder.size() == 1) {
      std::shared_ptr<Data> data = std::make_shared<Data>(n);
      data->setFreshnessPeriod(time::seconds(3600));
      key_chain_.sign(*data, signingWithSha256());
      face_.put(*data);

      Interest i(kLocalhostSleepingCommand);
      face_.expressInterest(i, [](const Interest&, const Data&) {},
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      scheduler_.cancelEvent(sync_interest_scheduler);
      scheduler_.cancelEvent(sync_duration_scheduler);
      node_state = kSleeping;
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") go to sleep" );
      sleep_start = time::system_clock::now();

      time::system_clock::time_point cur_time = time::system_clock::now();
      auto cur_sync_delay = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(send_sync_interest_time).count();
      sync_delay.push_back(cur_sync_delay);
      auto active_time = time::toUnixTimestamp(cur_time).count() - time::toUnixTimestamp(wakeup).count();
      working_time += active_time;
    }
    */
  }
}

/****************************************************************/
/* pipeline for sync-responder                         
/* 1. receive sync interest, is_syncing = true, generate missing_data list
/* most three times if it doesn't receive any data interests or 
/* SyncACK interest                                             
/****************************************************************/
void Node::OnIncomingData(const Interest& interest) {
  if (node_state == kSleeping || node_state == kIntermediate) return;
  else if (pending_interest.empty()) return;
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Schedule to send next interest");
  // cancel all interest timers:
  scheduler_.cancelEvent(inst_dt);
  scheduler_.cancelEvent(inst_wt);
  // send next pending interest
  SendInterest();
}

void Node::SendInterest() {
  // actually no need to cancel the timers again here, but to guarantee
  scheduler_.cancelEvent(inst_dt);
  scheduler_.cancelEvent(inst_wt);
  
  std::uniform_int_distribution<> rdist2_(0, kInterestDT);
  inst_dt = scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
    [this] {
      assert(!pending_interest.empty());
      while (!pending_interest.empty() && (data_store_.find(pending_interest.front().first) != data_store_.end() || pending_interest.front().second == 0)) {
        if (data_store_.find(pending_interest.front().first) != data_store_.end()) {
          VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") already has the data: name = " << pending_interest.front().first.toUri() );
        }
        else {
          VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") has already retransmitted the data for three times: data name = " << pending_interest.front().first.toUri() );
        }
        pending_interest.erase(pending_interest.begin());
      }
      if (pending_interest.empty()) {
        scheduler_.cancelEvent(inst_dt);
        scheduler_.cancelEvent(inst_wt);
        sync_responder_success = true;
        return;
      }
      auto n = pending_interest[0].first;
      if (pending_interest[0].second != kInterestTransmissionTime) {
        // add the collision_num (retransmission num)
        collision_num++;
      }
      pending_interest[0].second--;
      Interest i(n, time::milliseconds(kSendOutInterestLifetime));

      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send Interest: i.name=" << n.toUri());

      if (n.compare(0, 2, kSyncDataPrefix) == 0) {
        face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
      }
      else if (n.compare(0, 2, kSyncACKPrefix) == 0) {
        face_.expressInterest(i, std::bind(&Node::OnDataForSyncack, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
      }
      else assert(false);

      scheduler_.cancelEvent(inst_dt);
      scheduler_.cancelEvent(inst_wt);
      out_interest_num++;
      VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Reset WT " );
      inst_wt = scheduler_.scheduleEvent(kInterestWT, [this] { SendInterest(); });
    });
}

void Node::OnIncomingInterest(const Interest& interest) {
  if (node_state == kSleeping) return;
  else if (node_state == kIntermediate) {
    receive_ack_for_sync_interest = true;
    return;
  }
  else if (pending_interest.empty()) return;
  // cancel all interest timers:
  scheduler_.cancelEvent(inst_dt);
  scheduler_.cancelEvent(inst_wt);

  Name incoming_interest_name = Name("/ndn");
  incoming_interest_name.append(interest.getName().getSubName(2));
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv incomingInterest: name = " << incoming_interest_name.toUri() );
  // check if there exists the same pending interests
  for (auto entry: pending_interest) {
    auto interest_name = entry.first;
    if (interest_name.compare(incoming_interest_name) == 0) {
      suppression_num++;
      Interest i(interest_name, time::milliseconds(kAddToPitInterestLifetime));
      // VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send: i.name=" << interest_name.toUri());

      face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      break;
    }
  }
  // reset wt
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Reset WT " );
  inst_wt = scheduler_.scheduleEvent(kInterestWT, [this] { SendInterest(); });
}

void Node::OnSyncInterest(const Interest& interest) {
  if (node_state != kActive) return;
  if (receive_sync_interest == true) return;
  receive_sync_interest = true;

  const auto& n = interest.getName();
  auto sync_index = ExtractSyncIndex(n);
  auto sync_requester = ExtractNodeID(n);
  auto other_vv_str = ExtractEncodedVV(n);

  VersionVector other_vv = DecodeVV(other_vv_str);
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv Sync Interest: i.version_vector=" << VersionVectorToString(other_vv));
  if (other_vv.size() != version_vector_.size()) {
    VSYNC_LOG_TRACE("Different Version Vector Size in Group: " << gid_);
    return;
  }

  for (NodeID i = 0; i < version_vector_.size(); ++i) {
    uint64_t other_seq = other_vv[i];
    // update vv
    if (other_seq > version_vector_[i]) version_vector_[i] = other_seq;
    ReceiveWindow::SeqNumIntervalSet missing_interval = recv_window[i].CheckForMissingData(version_vector_[i]);
    if (missing_interval.empty()) continue;
    auto it = missing_interval.begin();
    while (it != missing_interval.end()) {
      for (uint64_t seq = it->lower(); seq <= it->upper(); ++seq) {
        //missing_data.push_back(MissingData(i, seq));
        pending_interest.push_back(std::pair<Name, int>(MakeDataName(gid_, i, seq), kInterestTransmissionTime));
      }
      it++;
    }
  }

  // add the syncACK interest to the last of the pending list
  size_t pending_list_size = pending_interest.size();
  pending_interest.push_back(std::pair<Name, int>(MakeSyncACKInterestName(gid_, sync_requester, nid_, sync_index, pending_list_size), 3));
  // print the pending interest
  std::string pending_list = "";
  for (auto entry: pending_interest) {
    pending_list += entry.first.toUri() + "\n";
  }
  VSYNC_LOG_TRACE( "(node" << gid_ << ", " << nid_ << ") pending interest list = :\n" + pending_list);
  SendInterest();
}

void Node::OnDataInterest(const Interest& interest) {
  // if node_state == kIntermediate, you should also process the interest!
  if (node_state == kSleeping) return;

  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Process Data Interest: i.name=" << n.toUri());

  auto group_id = ExtractGroupID(n);
  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  if(group_id != gid_) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Ignore data interest from different group: " << group_id);
    return;
  }

  if (node_state == kActive) {
    auto iter = data_store_.find(n);
    if (iter != data_store_.end()) {
      face_.put(*iter->second);
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") sends the data name = " << iter->second->getName());
    }
  }
  else if (node_state == kIntermediate) {
    receive_ack_for_sync_interest = true;
    auto iter = data_store_.find(n);
    // assert(iter != data_store_.end());
    if (iter != data_store_.end()) {
      face_.put(*iter->second);
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") sends the data name = " << iter->second->getName());
    }
  }
}

void Node::OnRemoteData(const Data& data) {
  if (node_state == kSleeping || node_state == kIntermediate) return;
  const auto& n = data.getName();

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv data: name=" << n.toUri());

  // if (data_size == 0) data_size = data.wireEncode().size();

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  if (data_store_.find(n) == data_store_.end()) {
    // update the version_vector, data_store_ and recv_window
    data_store_[n] = data.shared_from_this();
    recv_window[node_id].Insert(seq);

    std::vector<std::pair<Name, int>>::iterator it = pending_interest.begin();
    while (it != pending_interest.end()) {
      if (it->first.compare(data.getName()) == 0) {
        pending_interest.erase(it);
        break;
      }
      it++;
    }
  }
}

void Node::OnDataForSyncack(const Data& data) {
  // cancel dt & wt timers
  if (sync_responder_success == true) return;
  if (pending_interest.size() == 1 && pending_interest.back().first.compare(data.getName()) == 0) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv data for SyncACK, Stop Syncing" );
    scheduler_.cancelEvent(inst_dt);
    scheduler_.cancelEvent(inst_wt);
    sync_responder_success = true;
    pending_interest.clear();
  }
}

// print the vector clock every 5 seconds
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  if (node_state != kSleeping) { 
    vv_snapshots.push_back(version_vector_);
    rw_snapshots.push_back(recv_window);
    active_record.push_back(1);
  }
  else {
    vv_snapshots.push_back(VersionVector(version_vector_.size(), 0));
    rw_snapshots.push_back(std::vector<ReceiveWindow>(version_vector_.size()));
    active_record.push_back(0);
  }
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
}
  
}  // namespace vsync
}  // namespace ndn
