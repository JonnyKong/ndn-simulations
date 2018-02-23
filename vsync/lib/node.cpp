/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <random>
#include <fstream>

#include <boost/log/trivial.hpp>

#include <ndn-cxx/util/digest.hpp>

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

#include "ns3/simulator.h"
#include "ns3/nstime.h"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

static int kInterestTransmissionTime = 3;

static int kInterestDT = 5000;
static time::milliseconds kInterestWT = time::milliseconds(50);
static time::seconds kSyncDataTimer = time::seconds(10);
static time::milliseconds kSendOutInterestLifetime = time::milliseconds(50);
static time::milliseconds kAddToPitInterestLifetime = time::milliseconds(54);

static const int kSimulationRecordingTime = 180;
static const int kSnapshotNum = 90;
static time::milliseconds kSnapshotInterval = time::milliseconds(2000);
static const std::string availabilityFileName = "availability.txt";

static const int data_generation_rate_mean = 20000;

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
std::uniform_int_distribution<> dt_dist(0, kInterestDT);

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, Node::DataCb on_data)
           : face_(face),
             key_chain_(key_chain),
             nid_(nid),
             prefix_(prefix),
             scheduler_(scheduler),
             data_cb_(std::move(on_data)),
             rengine_(rdevice_()) {
  collision_num = 0;
  suppression_num = 0;
  out_interest_num = 0;
  in_dt = false;
  data_num = 0;

  face_.setInterestFilter(
      Name(kSyncNotifyPrefix), std::bind(&Node::OnSyncNotify, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register syncNotify prefix: " << reason); 
        throw Error("Failed to register syncNotify prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kSyncDataPrefix), std::bind(&Node::OnDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register data prefix: " << reason); 
        throw Error("Failed to register data prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
  scheduler_.scheduleEvent(time::seconds(kSimulationRecordingTime), [this] {PrintNDNTraffic(); });
}

void Node::PrintNDNTraffic() {
  Interest i(kGetNDNTraffic, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::StartSimulation() {
  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });

  std::string content = "HelloFrom" + to_string(nid_);
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
  proto::Content content_proto;
  EncodeVV(version_vector_, content_proto.mutable_vv());
  content_proto.set_content(content);
  const std::string& content_proto_str = content_proto.SerializeAsString();
  data->setContent(reinterpret_cast<const uint8_t*>(content_proto_str.data()),
                   content_proto_str.size());

  // data->setContent(reinterpret_cast<const uint8_t*>(content.data()),
  //                  content.size());
  data->setContentType(type);
  key_chain_.sign(*data, signingWithSha256());

  data_store_[n] = data;
  logDataStore(n);
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
  // std::string vv_encode = EncodeVV(version_vector_);
  std::string encoded_vv = EncodeVVToName(version_vector_);
  auto data_block = data_store_[data_name]->wireEncode();
  auto sync_notify_interest_name = MakeSyncNotifyName(nid_, encoded_vv, data_block);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Broadcast syncNotify Interest" );

  Interest sync_notify(sync_notify_interest_name);
  face_.expressInterest(sync_notify, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  recv_sync_notify[sync_notify_interest_name] = 1;
}

/****************************************************************/
/* pipeline for sync-responder                         
/* 1. receive sync interest, is_syncing = true, generate missing_data list
/* most three times if it doesn't receive any data interests or 
/* SyncACK interest                                             
/****************************************************************/
void Node::OnSyncNotify(const Interest& interest) {
  const auto& n = interest.getName();
  // check if received this notify before
  if (recv_sync_notify.find(n) != recv_sync_notify.end()) return;
  recv_sync_notify[n] = 1;
  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv the syncNotify Interest" );
  // TBD, when we receive this interest for n times, we can do some tricks, like cancel the transmission of notify(if still in DT)

  auto sender_id = ExtractNodeID(n);
  std::shared_ptr<Data> carried_data = std::make_shared<Data>(n.get(-1).blockFromValue());

  // extract the vv
  const auto& content = carried_data->getContent();
  proto::Content content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data content format: nid=" << nid_);
    assert(false);
  }
  auto other_vv = DecodeVV(content_proto.vv());

  // first store the current data
  auto data_name = carried_data->getName();
  // assert(data_store_.find(data_name) == data_store_.end());
  if (data_store_.find(data_name) == data_store_.end()) {
    data_store_[data_name] = carried_data;
    logDataStore(data_name);

    auto sender_seq = other_vv[sender_id];
    recv_window[sender_id].Insert(sender_seq);
  }

  // re-broadcast the notify, add it into the pending_interest list. There is no retransmission for notify
  pending_interest[n] = 1;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }

  FindMissingData(other_vv);
}

void Node::FindMissingData(const VersionVector& other_vv) {
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
    pending_list += entry.first.getPrefix(4).toUri() + "\n";
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
      // VSYNC_LOG_TRACE( "node(" << nid_ << ") already has the data: name = " << pending_interest.begin()->first.toUri() );
    }
    else {
      // VSYNC_LOG_TRACE( "node(" << nid_ << ") has already retransmitted the interest for a pre-configured time" );
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
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
}

void Node::OnDTTimeout() {
  while (!pending_interest.empty() && (data_store_.find(pending_interest.begin()->first) != data_store_.end() || pending_interest.begin()->second == 0)) {
    if (data_store_.find(pending_interest.begin()->first) != data_store_.end()) {
      // VSYNC_LOG_TRACE( "node(" << nid_ << ") already has the data: name = " << pending_interest.begin()->first.toUri() );
    }
    else {
      // VSYNC_LOG_TRACE( "node(" << nid_ << ") has already retransmitted the interest for a pre-configured time" );
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

  if (cur_transmission_time != kInterestTransmissionTime && n.compare(0, 2, kSyncNotifyPrefix) != 0) {
    // add the collision_num (retransmission num)
    collision_num++;
  }
  // pending_interest.begin()->second--;
  Interest i(n, kSendOutInterestLifetime);

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());

  face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  scheduler_.cancelEvent(inst_dt);
  // scheduler_.cancelEvent(inst_wt);
  out_interest_num++;
  assert(wt_list.find(n) == wt_list.end());
  wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time - 1); });
  
  pending_interest.erase(n);
  if (pending_interest.empty()) {
    in_dt = false;
    return;
  }
  int delay = dt_dist(rengine_);
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
}

void Node::OnWTTimeout(const Name& name, int cur_transmission_time) {
  wt_list.erase(name);
  // An interest cannot exist in WT and DT at the same time
  /*
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
  */
  assert(pending_interest.find(name) == pending_interest.end());
  pending_interest[name] = cur_transmission_time;
  if (in_dt == false) {
    assert(pending_interest.size() == 1);
    in_dt = true;
    SendDataInterest();
  }
  else assert(pending_interest.size() > 1);
}

void Node::OnDataInterest(const Interest& interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Data Interest: i.name=" << n.toUri());

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

    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    int cur_transmission_time = pending_interest.begin()->second;
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time); });
    
    pending_interest.erase(n);
    if (pending_interest.empty()) {
      in_dt = false;
      return;
    }
  }
  else if (wt_list.find(n) != wt_list.end()) return;
  else {
    // even if you don't need to fetch this data, you can add the corresponding pit
    Interest i(n, kAddToPitInterestLifetime);
    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    // we also need to set a corresponding WT
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n] { OnWTTimeout(n, kInterestTransmissionTime); });
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
    logDataStore(n);
    recv_window[node_id].Insert(seq);

    pending_interest.erase(n);
    if (pending_interest.empty()) in_dt = false;
    if (wt_list.find(n) != wt_list.end()) {
      scheduler_.cancelEvent(wt_list[n]);
      wt_list.erase(n);
    }

    // check the state vector in data
    const auto& content = data.getContent();
    proto::Content content_proto;
    if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
      VSYNC_LOG_WARN("Invalid data content format: nid=" << nid_);
      assert(false);
    }
    auto other_vv = DecodeVV(content_proto.vv());
    FindMissingData(other_vv);
  }
}

void Node::logDataStore(const Name& name) {
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Store New Data: " << name.toUri() << std::endl;
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
