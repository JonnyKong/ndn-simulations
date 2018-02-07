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

static int kInterestTransmissionTime = 3;

static int kInterestDT = 5000;
static time::milliseconds kInterestWT = time::milliseconds(50);
static time::milliseconds kSendOutInterestLifetime = time::milliseconds(50);
static time::milliseconds kAddToPitInterestLifetime = time::milliseconds(54);

static const int kSnapshotNum = 90;
static time::milliseconds kSnapshotInterval = time::milliseconds(2000);
static const std::string availabilityFileName = "availability.txt";

static const int data_generation_rate_mean = 20000;
static const int sync_timer_mean = 50;

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
std::poisson_distribution<> sync_timer_dist(sync_timer_mean);
std::uniform_int_distribution<> dt_dist(0, kInterestDT);

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, Node::DataCb on_data)
           : face_(face),
             key_chain_(key_chain),
             nid_(nid),
             scheduler_(scheduler),
             prefix_(prefix),
             data_cb_(std::move(on_data)),
             rengine_(rdevice_()) {
  collision_num = 0;
  suppression_num = 0;
  out_interest_num = 0;
  in_dt = false;
  data_num = 0;

  face_.setInterestFilter(
      Name(kSyncPrefix), std::bind(&Node::OnSyncInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register vsync prefix: " << reason); 
        throw Error("Failed to register vsync prefix: " + reason);
      });


  face_.setInterestFilter(
      Name(kSyncDataPrefix), std::bind(&Node::OnDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register data prefix: " << reason); 
        throw Error("Failed to register data prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
}

void Node::StartSimulation() {
  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });

  std::string content = "Hello from " + to_string(nid_);
  scheduler_.scheduleEvent(time::milliseconds(4000 * nid_),
                           [this, content] { PublishData(content); });
}

void Node::PublishData(const std::string& content, uint32_t type) {
  data_num++;
  version_vector_[nid_]++;

  // make data dame
  auto n = MakeDataName(nid_, version_vector_[nid_]);
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setFreshnessPeriod(time::seconds(3600));
  // set data content
  data->setContent(reinterpret_cast<const uint8_t*>(content.data()),
                   content.size());
  data->setContentType(type);
  key_chain_.sign(*data, signingWithSha256());

  data_store_[n] = data;
  recv_window[nid_].Insert(version_vector_[nid_]);

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Publish Data: d.name=" << n.toUri() << " d.type=" << type << " d.content=" << content);

  scheduler_.scheduleEvent(time::milliseconds(data_generation_dist(rengine_)),
                           [this, content] { PublishData(content); });

  if (data_num == 1) {
    data_num = 0;
    SyncData();
  }
}

/****************************************************************/
/* pipeline for sync-requester node                             */
/* The sync-requester node will retransmit sync interest for    */
/* most three times if it doesn't receive any data              */
/****************************************************************/

void Node::SyncData() {
  std::string vv_encode = EncodeVV(version_vector_);
  auto sync_interest_name = MakeSyncInterestName(nid_, vv_encode);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Sync Interest: name = " << sync_interest_name.toUri() );

  // SendSyncInterest(sync_interest_name, 0);
  // currently just derictly send out the sync interest
  Interest i(sync_interest_name, kSendOutInterestLifetime);
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

/*
void Node::SendSyncInterest(const Name& sync_interest_name, const uint32_t& sync_interest_time) {
  scheduler_.cancelEvent(inst_dt);
  // scheduler_.cancelEvent(inst_wt);
  // wt_name = Name("/");

  if (sync_interest_time == 3) {
    // should start the pending_list
    in_dt = true;
    SendDataInterest();
    return;
  }
  std::uniform_real_distribution<> rdist_(0, kInterestDT);
  inst_dt = scheduler_.scheduleEvent(time::milliseconds(dt_dist(rengine_)),
    [this, sync_interest_name, sync_interest_time] {
      VSYNC_LOG_TRACE("node(" << nid_ << ") Send Sync Interest: i.name=" << sync_interest_name.toUri());
      Interest i(sync_interest_name, kSendOutInterestLifetime);
      face_.expressInterest(i, std::bind(&Node::OnSyncACK(), this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      inst_wt = scheduler_.scheduleEvent(kInterestWT, [this] { SendSyncInterest(sync_interest_name, sync_interest_time + 1); });
      wt_name = sync_interest_name;
    });
}

void Node::OnSyncACK(const Data& data) {
  scheduler_.cancelEvent(inst_dt);
  // scheduler_.cancelEvent(inst_wt);
  // wt_name = Name("/");

  in_dt = true;
  SendDataInterest();
}
*/

/****************************************************************/
/* pipeline for sync-responder                         
/* 1. receive sync interest, is_syncing = true, generate missing_data list
/* most three times if it doesn't receive any data interests or 
/* SyncACK interest                                             
/****************************************************************/
void Node::OnSyncInterest(const Interest& interest) {
  const auto& n = interest.getName();
  auto sync_requester = ExtractNodeID(n);
  auto other_vv_str = ExtractEncodedVV(n);

  VersionVector other_vv = DecodeVV(other_vv_str);
  VSYNC_LOG_TRACE("node(" << nid_ << ") Recv Sync Interest: i.version_vector=" << VersionVectorToString(other_vv));

  for (auto entry: other_vv) {
    NodeID node_id = entry.first;
    uint64_t other_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
      // current vv don't have the node_id
      version_vector_[node_id] = other_seq;
      ReceiveWindow::SeqNumIntervalSet missing_interval = recv_window[node_id].CheckForMissingData(version_vector_[node_id]);
      if (missing_interval.empty()) continue;
      auto it = missing_interval.begin();
      while (it != missing_interval.end()) {
        for (uint64_t seq = it->lower(); seq <= it->upper(); ++seq) {
          //missing_data.push_back(MissingData(i, seq));
          Name data_interest_name = MakeDataName(node_id, seq);
          if (pending_interest.find(data_interest_name) != pending_interest.end() &&
            pending_interest[data_interest_name] != 0) continue;
          pending_interest[data_interest_name] = kInterestTransmissionTime;
        }
        it++;
      }
    }
  }

  // send back data
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setFreshnessPeriod(time::seconds(3600));
  key_chain_.sign(*data, signingWithSha256());
  face_.put(*data);

  // print the pending interest
  std::string pending_list = "";
  for (auto entry: pending_interest) {
    pending_list += entry.first.toUri() + "\n";
  }
  VSYNC_LOG_TRACE( "(node" << nid_ << ") pending interest list = :\n" + pending_list);
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
}

void Node::SendDataInterest() {
  // actually no need to cancel the timers again here, but to guarantee
  assert(in_dt == true);
  scheduler_.cancelEvent(inst_dt);
  // scheduler_.cancelEvent(inst_wt);
  // wt_name = Name("/");
  
  while (!pending_interest.empty() && (data_store_.find(pending_interest.begin()->first) != data_store_.end() || pending_interest.begin()->second == 0)) {
    if (data_store_.find(pending_interest.begin()->first) != data_store_.end()) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") already has the data: name = " << pending_interest.begin()->first.toUri() );
    }
    else {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") has already retransmitted the data for three times: data name = " << pending_interest.begin()->first.toUri() );
    }
    pending_interest.erase(pending_interest.begin());
  }
  if (pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    // scheduler_.cancelEvent(inst_wt);
    in_dt = false;
    return;
  }

  inst_dt = scheduler_.scheduleEvent(time::microseconds(dt_dist(rengine_)), [this] { OnDTTimeout(); });
}

void Node::OnDTTimeout() {
  while (!pending_interest.empty() && (data_store_.find(pending_interest.begin()->first) != data_store_.end() || pending_interest.begin()->second == 0)) {
    if (data_store_.find(pending_interest.begin()->first) != data_store_.end()) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") already has the data: name = " << pending_interest.begin()->first.toUri() );
    }
    else {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") has already retransmitted the data for three times: data name = " << pending_interest.begin()->first.toUri() );
    }
    pending_interest.erase(pending_interest.begin());
  }
  if (pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    // scheduler_.cancelEvent(inst_wt);
    in_dt = false;
    return;
  }
  auto n = pending_interest.begin()->first;
  int cur_transmission_time = pending_interest.begin()->second;

  if (cur_transmission_time != kInterestTransmissionTime) {
    // add the collision_num (retransmission num)
    collision_num++;
  }
  // pending_interest.begin()->second--;
  Interest i(n, kSendOutInterestLifetime);

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.toUri());

  face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  scheduler_.cancelEvent(inst_dt);
  // scheduler_.cancelEvent(inst_wt);
  out_interest_num++;
  VSYNC_LOG_TRACE("node(" << nid_ << ") Set WT " );
  assert(wt_list.find(n) == wt_list.end());
  wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time - 1); });
  
  pending_interest.erase(n);
  if (pending_interest.empty()) {
    in_dt = false;
    return;
  }
  inst_dt = scheduler_.scheduleEvent(time::microseconds(dt_dist(rengine_)), [this] { OnDTTimeout(); });
}

void Node::OnWTTimeout(const Name& name, int cur_transmission_time) {
  wt_list.erase(name);
  if (pending_interest.find(name) != pending_interest.end()) {
    assert(in_dt == true);
    return;
  }
  else {
    pending_interest[name] = cur_transmission_time;
    if (in_dt == false) {
      assert(pending_interest.size() == 1);
      in_dt = true;
      SendDataInterest();
    }
    else assert(pending_interest.size() > 1);
  }
}

void Node::OnDataInterest(const Interest& interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Process Data Interest: i.name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  auto iter = data_store_.find(n);
  if (iter != data_store_.end()) {
    face_.put(*iter->second);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") sends the data name = " << iter->second->getName());
  }
  else if (pending_interest.find(n) != pending_interest.end()) {
    suppression_num++;
    Interest i(n, kAddToPitInterestLifetime);
    // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send: i.name=" << interest_name.toUri());

    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    int cur_transmission_time = pending_interest.begin()->second;
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time - 1); });
    
    pending_interest.erase(n);
    if (pending_interest.empty()) {
      in_dt = false;
      return;
    }
  }
  else {
    // even if you don't need to fetch this data, you can add the corresponding pit
    Interest i(n, kAddToPitInterestLifetime);
    // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send: i.name=" << interest_name.toUri());

    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
  }
}

void Node::OnRemoteData(const Data& data) {
  const auto& n = data.getName();

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data: name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  if (data_store_.find(n) == data_store_.end()) {
    // update the version_vector, data_store_ and recv_window
    data_store_[n] = data.shared_from_this();
    recv_window[node_id].Insert(seq);

    pending_interest.erase(n);
    if (pending_interest.empty()) in_dt = false;
    if (wt_list.find(n) != wt_list.end()) {
      scheduler_.cancelEvent(wt_list[n]);
      wt_list.erase(n);
    }
  }
}

// print the vector clock every 5 seconds
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  vv_snapshots.push_back(version_vector_);
  rw_snapshots.push_back(recv_window);
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
}
  
}  // namespace vsync
}  // namespace ndn
