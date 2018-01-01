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

static time::milliseconds kReplyWaitingTime = time::milliseconds(1000);
static time::milliseconds kInterval = time::milliseconds(9 * 4 * 1000);

static time::milliseconds kWaitACKforSyncInterestInterval = time::milliseconds(55);
static time::milliseconds kWaitRemoteDataInterval = time::milliseconds(55);
static time::milliseconds kSnapshotInterval = time::milliseconds(10000);

static int kDataInterestDelay = 50;
static int kSyncACKDelay = 20;
static int kProbeIntermediate = 5;

static int kSendOutInterestLifetime = 55;
static int kAddToPitInterestLifetime = 54;
static const std::string availabilityFileName = "availability.txt";
static const int kActiveInGroup = 2;
static const int kSnapshotNum = 15;

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, const GroupID& gid,
           const uint64_t group_size_, Node::DataCb on_data)
           : face_(face),
             scheduler_(scheduler),
             key_chain_(key_chain),
             nid_(nid),
             group_size(group_size_),       
             prefix_(prefix),
             gid_(name::Component(gid).toUri()),
             data_cb_(std::move(on_data)),
             rengine_(rdevice_()),
             rdist_(3000, 10000) {
  version_vector_ = VersionVector(group_size, 0);
  data_store_ = std::vector<std::vector<std::shared_ptr<Data>>>(group_size, std::vector<std::shared_ptr<Data>>(0));
  node_state = kActive;
  energy_consumption = 0.0;
  sleeping_time = 0.0;
  current_sync_sender = -1;
  receive_ack_for_sync_interest = false;
  index = -1;
  is_syncing = false;

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
      Name(kProbePrefix).append(gid_), std::bind(&Node::OnProbeInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register probe interest prefix: " << reason); 
        throw Error("Failed to register probe interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kReplyPrefix).append(gid_), std::bind(&Node::OnReplyInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register reply interest prefix: " << reason); 
        throw Error("Failed to register reply interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kSleepCommandPrefix).append(gid_), std::bind(&Node::OnSleepCommandInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register sleep command interest prefix: " << reason); 
        throw Error("Failed to register sleep command interest prefix: " + reason);
      });  

  face_.setInterestFilter(
      Name(kSyncACKPrefix).append(gid_), std::bind(&Node::OnSyncACKInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register sync ack interest prefix: " << reason); 
        throw Error("Failed to register sync ack interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kProbeIntermediatePrefix).append(gid_), std::bind(&Node::OnProbeIntermediateInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register probeIntermediate interest prefix: " << reason); 
        throw Error("Failed to register probeIntermediate interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kStartRequestDataPrefix).append(gid_), std::bind(&Node::OnStartRequestDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register startRequestData interest prefix: " << reason); 
        throw Error("Failed to register startRequestData interest prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(4 * nid_ * 1000),
                           [this] { SendProbeInterest(); });

  scheduler_.scheduleEvent(time::milliseconds(0), [this] { CheckOneWakeupNode(); });

  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });
}

void Node::CheckOneWakeupNode() {
  scheduler_.scheduleEvent(time::milliseconds(4 * 1000), [this] { CheckOneWakeupNode(); });
  index++;
  if (index == group_size) index = 0;
  if (index == nid_) {
    return;
  }
  else {
    // all other nodes should not be kIntermediate
    if (node_state == kIntermediate) {
      // make this node go to sleep
      scheduler_.cancelEvent(sync_interest_scheduler);
      Interest i(kLocalhostSleepingCommand);
      face_.expressInterest(i, [](const Interest&, const Data&) {},
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      scheduler_.cancelEvent(sync_interest_scheduler);
      node_state = kSleeping;
      sleep_start = time::system_clock::now();
      std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
    }
    else if (node_state == kActive) {
      std::cout << "active node(" << gid_ << " " << nid_ << ") stop syncing the data!" << std::endl;
      // cancel all of the scheduling events
      missing_data.clear();
      is_syncing = false;
      scheduler_.cancelEvent(syncACK_delay_scheduler);
      scheduler_.cancelEvent(syncACK_scheduler);
      scheduler_.cancelEvent(data_interest_delay_scheduler);
      scheduler_.cancelEvent(data_interest_scheduler);
    }
  }
}

//OK
void Node::PublishData(const std::string& content, uint32_t type) {
  if (node_state != kActive) return;
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

  data_store_[nid_].push_back(data);

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Publish Data: d.name=" << n.toUri() << " d.type=" << type << " d.content=" << content);
}

/****************************************************************/
/* pipeline for sync-initializer node                           */
/* The sync-initializer node will retransmit sync interest for  */
/* most three times if it doesn't receive any data interests or */
/* SyncACK interest                                             */
/****************************************************************/
void Node::SyncData() {
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Start to Sync Data");
  receive_ack_for_sync_interest = false;

  std::string vv_encode = EncodeVV(version_vector_);
  auto sync_interest_name = MakeSyncInterestName(gid_, nid_, vv_encode);
  SendSyncInterest(sync_interest_name, 0);

  sync_interest_scheduler = scheduler_.scheduleEvent(kWaitACKforSyncInterestInterval, [this, sync_interest_name] {
    this->SyncInterestTimeout(sync_interest_name, 1);
  });
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
    // actually do nothing. the current node will still try to sync up data
    return;
  }
  else if (node_state == kIntermediate) {
    if (syncACK_receiver != nid_) {
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Receive wrong SyncACKInterest: i.name=" << n.toUri());
      return;
    }

    VSYNC_LOG_TRACE( "sync-initializer (" << gid_ << " " << nid_ << ") Receive SyncACKInterest: i.name=" << n.toUri());
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);

    Interest i(kLocalhostSleepingCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    scheduler_.cancelEvent(sync_interest_scheduler);
    node_state = kSleeping;
    sleep_start = time::system_clock::now();
  }
}

/****************************************************************/
/* pipeline for sync-interest-receivers                         
/* 1. receive sync interest, is_syncing = true, generate missing_data list
/* most three times if it doesn't receive any data interests or 
/* SyncACK interest                                             
/****************************************************************/
void Node::OnStartRequestDataInterest(const Interest& interest) {
  if (node_state == kSleeping || node_state == kIntermediate) return;
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv StartRequestData Interest");
  RequestMissingData();
}

void Node::OnSyncInterest(const Interest& interest) {
  if (node_state != kActive) return;
  if (is_syncing == true) return;

  const auto& n = interest.getName();
  auto group_id = ExtractGroupID(n);
  auto sync_sender = ExtractNodeID(n);
  auto other_vv_str = ExtractEncodedVV(n);

  current_sync_sender = sync_sender;
  is_syncing = true;
  VersionVector other_vv = DecodeVV(other_vv_str);
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv Sync Interest: i.version_vector=" << VersionVectorToString(other_vv));
  if (other_vv.size() != version_vector_.size()) {
    VSYNC_LOG_TRACE("Different Version Vector Size in Group: " << gid_);
    return;
  }

  // Process version vector
  VersionVector old_vv = version_vector_;
  for (NodeID i = 0; i < version_vector_.size(); ++i) {
    uint64_t other_seq = other_vv[i];
    uint64_t my_seq = version_vector_[i];
    if (other_seq > my_seq) {
      for (uint64_t seq = my_seq + 1; seq <= other_seq; ++seq) {
        missing_data.push_back(MissingData(i, seq));
      }
    }
  }
  RequestMissingData();
}


void Node::RequestMissingData() {
  if (is_syncing == false) return;
  if (missing_data.empty()) {
    is_syncing = false;
    // TBD
    // cancel all of the schedulers: 
    scheduler_.cancelEvent(data_interest_scheduler);
    scheduler_.cancelEvent(data_interest_delay_scheduler);
    Name n = MakeSyncACKInterestName(gid_, current_sync_sender);
    SendSyncACKInterest(0, n);
    return;
  }
  MissingData request_data = missing_data[0];
  // check if the request_data has been received and stored
  if (version_vector_[request_data.node_id] >= request_data.seq) {
    missing_data.erase(missing_data.begin());
    RequestMissingData();
    return;
  }
  SendDataInterest(request_data.node_id, request_data.seq);
}

void Node::SendDataInterest(const NodeID& node_id, uint64_t seq) {
  auto data_interest_name = MakeDataName(gid_, node_id, seq);

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") schedule to send DATA Interest: i.name=" << data_interest_name.toUri());
  std::uniform_int_distribution<> rdist2_(0, kDataInterestDelay);
  data_interest_delay_scheduler = scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
    [this, data_interest_name] {
      Interest i(data_interest_name, time::milliseconds(kSendOutInterestLifetime));
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send DATA Interest: i.name=" << data_interest_name.toUri());

      face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      data_interest_scheduler = scheduler_.scheduleEvent(kWaitRemoteDataInterval, [this] { OnDataInterestTimeout(); });
    });
}

void Node::OnDataInterestTimeout() {
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") OnDataInterestTimeout");
  MissingData request_data = missing_data[0];
  missing_data.erase(missing_data.begin());
  missing_data.push_back(request_data);
  RequestMissingData();
}

void Node::OnDataInterest(const Interest& interest) {
  // if node_state == kIntermediate, you should also process the interest!
  if (node_state == kSleeping) return;

  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv Data Interest: i.name=" << n.toUri());

  auto group_id = ExtractGroupID(n);
  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  if(group_id != gid_) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Ignore data interest from different group: " << group_id);
    return;
  }

  if (node_state == kIntermediate) {
    receive_ack_for_sync_interest = true;
    if (seq > version_vector_[node_id]) {
      VSYNC_LOG_TRACE( "should not happen! node(" << gid_ << " " << nid_ << ") doesn't contain the requesting data based on Version Vector");
      return;
    }
    else if (seq > data_store_[node_id].size()) {
      VSYNC_LOG_TRACE( "should not happen! node(" << gid_ << " " << nid_ << ") doesn't contain the requesting data based on Data Store");
      return;
    }
    else {
      // make the data packet
      std::shared_ptr<Data> data = data_store_[node_id][seq - 1];
      std::shared_ptr<Data> outdata = std::make_shared<Data>(data->getName());
      outdata->setFreshnessPeriod(time::seconds(3600));
      outdata->setContent(data->getContent());
      outdata->setContentType(data->getContentType());
      key_chain_.sign(*outdata, signingWithSha256());
      face_.put(*outdata);
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") sends the data name = " << outdata->getName());
    }
  }
  else if (node_state == kActive) {
    // cancel the data_interest timers:
    scheduler_.cancelEvent(data_interest_scheduler);
    scheduler_.cancelEvent(data_interest_delay_scheduler);
    // iterate the missing_data, to see if there is the same missing data
    for (auto data: missing_data) {
      if (data.node_id == node_id && data.seq == seq) {
        Interest i(interest.getName(), time::milliseconds(kAddToPitInterestLifetime));
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send: i.name=" << n.toUri());

        face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
        break;
      }
    }
  }
}

void Node::OnRemoteData(const Data& data) {
  if (node_state == kSleeping || node_state == kIntermediate) return;
  const auto& n = data.getName();

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv data: name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  // Store a local copy of received data
  assert(version_vector_[node_id] >= seq - 1);
  if (version_vector_[node_id] == seq - 1) {
    std::shared_ptr<Data> new_data = std::make_shared<Data>(n);
    new_data->setFreshnessPeriod(time::seconds(3600));
    // set data content
    new_data->setContent(data.getContent());
    new_data->setContentType(data.getContentType());
    key_chain_.sign(*new_data, signingWithSha256());

    data_store_[node_id].push_back(new_data);
    assert(data_store_[node_id].size() == seq);
    version_vector_[node_id] = seq;
    data_cb_(version_vector_);
      
    std::vector<MissingData>::iterator it = missing_data.begin();
    while (it != missing_data.end()) {
      if (it->node_id == node_id && it->seq == seq) {
        missing_data.erase(it);
        break;
      }
      it++;
    }
    scheduler_.cancelEvent(data_interest_scheduler);
  }
  RequestMissingData();
}

// at most will send syncACK for three times
void Node::SendSyncACKInterest(uint32_t syncACK_time, const Name& syncACK_name) {
  if (syncACK_time == 3) return;

  Interest i(syncACK_name, time::milliseconds(5));

  std::uniform_int_distribution<> rdist2_(0, kSyncACKDelay);
  syncACK_delay_scheduler = scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
    [this, syncACK_time, syncACK_name] { 
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send SyncACKInterest: i.name=" << syncACK_name.toUri());
      face_.expressInterest(syncACK_name, [this](const Interest&, const Data&) {
        scheduler_.cancelEvent(syncACK_scheduler);
      }, 
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      syncACK_scheduler = scheduler_.scheduleEvent(time::milliseconds(5), [this, syncACK_time, syncACK_name] {
        SendSyncACKInterest(syncACK_time + 1, syncACK_name);
      });
    });
}

/***************************
 sleeping mechanisms
****************************/
void Node::OnProbeIntermediateInterest(const Interest& interest) {
  if (node_state == kIntermediate) {
    auto n = interest.getName();
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);
  }
}

void Node::SendProbeInterest() {
  lastState = node_state;
  if (node_state == kIntermediate) {
    // could not happen
    std::cout << "node(" << gid_ << " " << nid_ << ") continue to go to intermediate twice, could not happen!" << std::endl;
    return;
  }
  else if (node_state == kActive) {
    // cancel the current timers
    std::cout << "active node(" << gid_ << " " << nid_ << ") stop syncing the data!" << std::endl;
    // cancel all of the scheduling events
    scheduler_.cancelEvent(syncACK_scheduler);
    scheduler_.cancelEvent(syncACK_delay_scheduler);
    scheduler_.cancelEvent(data_interest_scheduler);
    scheduler_.cancelEvent(data_interest_delay_scheduler);
  }
  else if (node_state == kSleeping) {
    time::system_clock::time_point sleep_end = time::system_clock::now();
    std::time_t time_span = time::system_clock::to_time_t(sleep_end) - time::system_clock::to_time_t(sleep_start);
    std::cout << "node(" << gid_ << " " << nid_ << ") slept for " << time_span << " seconds." << std::endl;
    sleeping_time += time_span;
  }

  // initialize missing_data and is_syncup_node
  missing_data.clear();
  is_syncing = false;

  scheduler_.scheduleEvent(kInterval, [this] { SendProbeInterest(); });
  node_state = kIntermediate;
  // make the nfd work
  Interest wakeupNFD(kLocalhostWakeupCommand);
  face_.expressInterest(wakeupNFD, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  node_state = kIntermediate;
  auto n = MakeProbeInterestName(gid_);
  Interest i(n, time::milliseconds(1000));
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send ProbeInterest: i.name=" << n.toUri());


  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  scheduler_.scheduleEvent(kReplyWaitingTime, [this] { CalculateReply(); });
}

void Node::CalculateReply() {
  /* we should guarantee that there should be at least two active nodes in the group. if so, the current 
  node can continue to go to sleep. Otherwise it should start to work.
  */
  size_t active_in_group = received_reply.size();
  std::string received_nodes = "";
  for (auto entry: received_reply) {
    received_nodes += to_string(entry.first) + ",";
  }
  std::cout << "node(" << gid_ << " " << nid_ << ") receives " << active_in_group << " replies: " <<  received_nodes << std::endl;

  if (active_in_group >= kActiveInGroup) {
    // we should choose the node with smallest sleeping time to go to sleep
    NodeID sleep_node = nid_;
    uint64_t smallest_sleeping_time = sleeping_time;
    for (auto entry: received_reply) {
      if (entry.second < smallest_sleeping_time) {
        sleep_node = entry.first;
        smallest_sleeping_time = entry.second;
      }
    }
    if (sleep_node == nid_) {
      if (lastState == kSleeping) {
        // continue to go to sleep, no need to sync up data because no new data
        std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
        Interest i(kLocalhostSleepingCommand);
        face_.expressInterest(i, [](const Interest&, const Data&) {},
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
        node_state = kSleeping;
        sleep_start = time::system_clock::now();
      }
      else SyncData();
    }
    else {
      std::cout << "node(" << gid_ << " " << nid_ << ") will send sleep command to " << "node(" << gid_ << " " << sleep_node << ")" << std::endl;
      auto sleepCommandName = MakeSleepCommandName(gid_, sleep_node);
      Interest i(sleepCommandName);
      face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      node_state = kActive;
    }
  }
  else {
    // start to work
    // we have waken up the nfd in SendProbeInterest(), so no need to do again here
    /*
    Interest i(kLocalhostWakeupCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    */
    std::cout << "node(" << gid_ << " " << nid_ << ") will start to work" << std::endl;
    node_state = kActive;
  }
  received_reply.clear();
}

void Node::OnProbeInterest(const Interest& interest) {
  if (node_state == kActive) {
    const auto& n = interest.getName();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv probe interest: name=" << n.toUri());
    std::uniform_int_distribution<> rdist2_(0, 50);
    scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
                             [this] { SendReplyInterest(); });
  }
}

void Node::SendReplyInterest() {
  if (node_state == kActive) {
    auto n = MakeReplyInterestName(gid_, nid_, sleeping_time);
    Interest i(n, time::milliseconds(5));
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send ReplyInterest: i.name=" << n.toUri());

    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
  }
}

void Node::OnReplyInterest(const Interest& interest) {
  if (node_state == kIntermediate) {
    const auto& n = interest.getName();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv reply interest: name=" << n.toUri());
    auto reply_node = ExtractNodeID(n);
    auto sleeping_time = ExtractSleepingTime(n);
    received_reply[reply_node] = sleeping_time;
  }
}

void Node::OnSleepCommandInterest(const Interest& interest) {
  const auto& n = interest.getName();
  NodeID sleep_node = ExtractNodeID(n);
  if (sleep_node != nid_) return;
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv sleep command interest: name=" << n.toUri());
  if (node_state != kActive) {
    std::cout << "problem 0 here!" << std::endl;
  }
  node_state = kIntermediate;
  // go to sleep
  SyncData();
  // sleeping_time += kSleepInterval; // currently sleeping time is fixed, = 10 seconds
}

// print the vector clock every 5 seconds
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  if (node_state != kSleeping) { 
    vv_snapshots.push_back(version_vector_);
  }
  else vv_snapshots.push_back(VersionVector(version_vector_.size(), 0));
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
}
  
}  // namespace vsync
}  // namespace ndn
