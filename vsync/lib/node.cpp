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

static const int kSnapshotNum = 18000;
static time::milliseconds kSnapshotInterval = time::milliseconds(10);
static const std::string availabilityFileName = "availability.txt";

static const int data_generation_rate_mean = 20000;

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
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
      Name(kIncomingSyncPrefix), std::bind(&Node::OnIncomingSyncInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register incomingSync prefix: " << reason); 
        throw Error("Failed to register incomingSync prefix: " + reason);
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
    SyncData(n);
  }
}

/****************************************************************/
/* pipeline for sync-requester node                             */
/* The sync-requester node will retransmit sync interest for    */
/* most three times if it doesn't receive any data              */
/****************************************************************/

void Node::SyncData(const Name& data_name) {
  std::string vv_encode = EncodeVV(version_vector_);
  auto sync_data_name = MakeSyncNotifyName(nid_, vv_encode);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Broadcast Sync Data: name = " << sync_data_name.toUri() );

  std::shared_ptr<Data> data = std::make_shared<Data>(sync_data_name);
  data->setFreshnessPeriod(time::seconds(3600));
  data->setContent(data_store_[data_name]->getContent());
  data->setContentType(data_store_[data_name]->getContentType());
  key_chain_.sign(*data, signingWithSha256());
  face_.put(*data);
}

/****************************************************************/
/* pipeline for sync-responder                         
/* 1. receive sync interest, is_syncing = true, generate missing_data list
/* most three times if it doesn't receive any data interests or 
/* SyncACK interest                                             
/****************************************************************/
void Node::OnIncomingSyncInterest(const Interest& interest) {
  const auto& n = interest.getName();
  auto node_id = ExtractNodeID(n);
  auto vv = ExtractEncodedVV(n);

  auto notify = MakeSyncNotifyName(node_id, vv);
  Interest i(notify, kSendOutInterestLifetime);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << notify.toUri());

  face_.expressInterest(i, std::bind(&Node::OnSyncNotify, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnSyncNotify(const Data& data) {
  const auto& n = data.getName();
  auto sender_id = ExtractNodeID(n);
  auto vv = ExtractEncodedVV(n);
  auto other_vv = DecodeVV(vv);

  // first store the current data
  auto data_name = MakeDataName(sender_id, other_vv[sender_id]);
  std::shared_ptr<Data> new_data = std::make_shared<Data>(data_name);
  new_data->setFreshnessPeriod(time::seconds(3600));
  new_data->setContent(data.getContent());
  new_data->setContentType(data.getContentType());
  key_chain_.sign(*new_data, signingWithSha256());
  data_store_[data_name] = new_data;
  recv_window[sender_id].Insert(other_vv[sender_id]);
  VSYNC_LOG_TRACE ("node(" << nid_ << ") store the data from SyncNotify: " << new_data->getName().toUri() );

  // update the vv, and fetch the other missing data
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
          if (data_store_.find(data_interest_name) != data_store_.end()) continue;
          if (pending_interest.find(data_interest_name) != pending_interest.end() &&
            pending_interest[data_interest_name] != 0) continue;
          else if (wt_list.find(data_interest_name) != wt_list.end()) continue;
          pending_interest[data_interest_name] = kInterestTransmissionTime;
        }
        it++;
      }
    }
  }

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

  int delay = dt_dist(rengine_);
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") schedule to send interest after " << delay << "nanoseconds" );
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") nanoseconds.count = " << time::nanoseconds(delay).count() );
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
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
  int delay = dt_dist(rengine_);
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") schedule to send interest after " << delay << "nanoseconds" );
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") nanoseconds.count = " << time::nanoseconds(delay).count() );
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
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
