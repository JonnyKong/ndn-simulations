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

static const int data_generation_rate_mean = 40000;

static time::seconds kDetectIsolationTimer = time::seconds(20);
static time::seconds kHeartbeatTimer = time::seconds(2);

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
std::uniform_int_distribution<> dt_dist(0, kInterestDT);

static const bool kSyncNotifyBeacon = false;
static time::seconds kSyncNotifyBeaconTimer = time::seconds(6);

static const bool kSyncNotifyRetx = true;
static int kSyncNotifyMax = 3;
static time::seconds kSyncNotifyRetxTimer = time::seconds(3);

static const bool kBeaconing = true;

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
  // for heartbeat
  recv_count = 0;
  is_isolated = false;
  sync_notify_time = 0;
  latest_data = Name("/");
  pending_sync_notify = Name("/");

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

  face_.setInterestFilter(
      Name(kHeartbeatPrefix), std::bind(&Node::OnHeartbeat, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register heartbeat prefix: " << reason); 
        throw Error("Failed to register heartbeat prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kIncomingPacketNotify), std::bind(&Node::OnIncomingPacketNotify, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register incomingPacketNotify: " << reason); 
        throw Error("Failed to register incomingPacketNotify: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
  scheduler_.scheduleEvent(time::seconds(kSimulationRecordingTime), [this] { PrintNDNTraffic(); });
  // scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
  if (kSyncNotifyBeacon == true) {
    sync_notify_beacon_scheduler = scheduler_.scheduleEvent(kSyncNotifyBeaconTimer, [this] { OnSyncNotifyBeaconTimeout(); });
  }
}

void Node::OnIncomingPacketNotify(const Interest& interest) {
  /*
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Recv Packet From Others" );
  if (!is_isolated) {
    recv_count++;
  }
  else {
    scheduler_.cancelEvent(heartbeatScheduler);
    scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
    assert(recv_count == 0);
    is_isolated = false;
    // find the most update data
    auto latest_data_name = MakeDataName(nid_, version_vector_[nid_]);
    SyncData(latest_data_name);
  }
  */
}

void Node::OnHeartbeat(const Interest& interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Recv Heartbeat: " << n.toUri() );
  SendSyncNotify();
}

void Node::DetectIsolation() {
  if (recv_count == 0) {
    VSYNC_LOG_TRACE ( "node(" << nid_ << ") Detect Isolation, Start to Heartbest" );
    is_isolated = true;
    heartbeatScheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
    // cancel the sync_notify_scheduler
    scheduler_.cancelEvent(sync_notify_retx_scheduler);
    sync_notify_time = 0;
    return;
  }
  else {
    VSYNC_LOG_TRACE ( "node(" << nid_ << ") Detect No Isolation" );
    assert(is_isolated == false);
    recv_count = 0;
    scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
  }
}

void Node::SendHeartbeat() {
  // broadcast heartbeat. Notification! Heartbeat is only broadcast by one hop!!
  // TBD, do we need delay for heartbeat?? Otherwise collision
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Send Heartbest" );
  assert(is_isolated == true);
  auto heartbeat_name = MakeHeartbeatName(nid_);
  Interest i(heartbeat_name);
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  heartbeatScheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
}

void Node::PrintNDNTraffic() {
  Interest i(kGetNDNTraffic, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnSyncNotifyRetxTimeout() {
  VSYNC_LOG_TRACE ("node(" << nid_ << ") SyncNotifyRetxTimeout" );
  sync_notify_time++;
  SendSyncNotify();
}

void Node::OnSyncNotifyBeaconTimeout() {
  VSYNC_LOG_TRACE ("node(" << nid_ << ") SyncNotifyBeaconTimeout" );
  SendSyncNotify();
  sync_notify_beacon_scheduler = scheduler_.scheduleEvent(kSyncNotifyBeaconTimer, [this] { OnSyncNotifyBeaconTimeout(); });
}

void Node::StartSimulation() {
  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });

  std::string content = "Hello From " + to_string(nid_);
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
  data->setContentType(type);
  key_chain_.sign(*data, signingWithSha256());

  data_store_[n] = data;
  logDataStore(n);
  recv_window[nid_].Insert(version_vector_[nid_]);
  latest_data = n;

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Publish Data: d.name=" << n.toUri() << " d.type=" << type << " d.content=" << content);

  scheduler_.scheduleEvent(time::milliseconds(data_generation_dist(rengine_)),
                           [this, content] { PublishData(content); });

  if (!is_isolated) {
    if (data_num >= 1) {
      data_num = 0;
      SendSyncNotify();

      if (kSyncNotifyRetx == true) {
        // when we update the vv, we need to send out the latest state vector for three times
        sync_notify_time = 0;
        scheduler_.cancelEvent(sync_notify_retx_scheduler);
      }
    }
  }
}

/****************************************************************/
/* pipeline for sync-requester node                             */
/* The sync-requester node will retransmit sync interest for    */
/* most three times if it doesn't receive any data              */
/****************************************************************/

void Node::SendSyncNotify() {
  // std::string vv_encode = EncodeVV(version_vector_);
  if (latest_data.compare(Name("/")) == 0) return;
  std::string encoded_vv = EncodeVVToName(version_vector_);
  auto data_block = data_store_[latest_data]->wireEncode();
  auto sync_notify_interest_name = MakeSyncNotifyName(encoded_vv, data_block);

  pending_sync_notify = sync_notify_interest_name;
  // pending_interest[sync_notify_interest_name] = 1;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
  /*
  Interest sync_notify(sync_notify_interest_name);
  face_.expressInterest(sync_notify, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  */
}

/****************************************************************************/
/* pipeline for sync-responder                                              */
/* 1. receive sync interest, is_syncing = true, generate missing_data list  */
/* most three times if it doesn't receive any data interests or             */
/* SyncACK interest                                                         */
/****************************************************************************/
void Node::OnSyncNotify(const Interest& interest) {
  const auto& n = interest.getName();

  // extract data
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
  if (data_store_.find(data_name) == data_store_.end()) {
    data_store_[data_name] = carried_data;
    logDataStore(data_name);

    auto seq_owner = ExtractNodeID(data_name);
    auto seq = ExtractSequence(data_name);
    recv_window[seq_owner].Insert(seq);
    latest_data = data_name;
  }

  std::vector<NodeID> missing_data;
  bool updated = false;
  for (auto entry: other_vv) {
    NodeID node_id = entry.first;
    uint64_t other_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
      // current vv don't have the node_id
      updated = true;
      version_vector_[node_id] = other_seq;
      missing_data.push_back(node_id);
    }
  }
  if (updated == false) return;

  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a new syncNotify Interest" );

  // send out the notify, add it into the pending_interest list. There is no retransmission for notify
  // the state vector in the notify should be the merged one
  auto new_sync_notify_name = MakeSyncNotifyName(EncodeVVToName(version_vector_), carried_data->wireEncode());
  // pending_interest[new_sync_notify_name] = 1;
  pending_sync_notify = new_sync_notify_name;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }

  if (kSyncNotifyRetx == true) {
    // when we update the vv, we need to send out the latest state vector for three times
    sync_notify_time = 0;
    scheduler_.cancelEvent(sync_notify_retx_scheduler);
  }

  FetchMissingData(missing_data);
}

void Node::FetchMissingData(const std::vector<NodeID>& missing_data) {
  for (auto node_id: missing_data) {
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

  // print the pending interest
  std::string pending_list = pending_sync_notify.getPrefix(4).toUri() + "\n";
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
  if (pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    // scheduler_.cancelEvent(inst_wt);
    in_dt = false;
    return;
  }

  int delay = dt_dist(rengine_);
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
}

void Node::OnDTTimeout() {
  // if the node currently is isolated, should stop sending data interests anymore
  if (is_isolated) {
    in_dt = false;
    scheduler_.cancelEvent(inst_dt);
    return;
  }

  while (!pending_interest.empty() && (data_store_.find(pending_interest.begin()->first) != data_store_.end() || pending_interest.begin()->second == 0)) {
    pending_interest.erase(pending_interest.begin());
  }
  if (pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    in_dt = false;
    return;
  }

  // choose an interest to send out. SyncNotify has higher priority
  if (pending_sync_notify.compare(Name("/")) != 0) {
    auto n = pending_sync_notify;
    pending_sync_notify = Name("/");

    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    if (kSyncNotifyRetx == true) {
      if (sync_notify_time == kSyncNotifyMax) {
        sync_notify_time = 0;
      }
      else {
        sync_notify_retx_scheduler = scheduler_.scheduleEvent(kSyncNotifyRetxTimer, [this] { OnSyncNotifyRetxTimeout(); });
      }
    }
    if (kSyncNotifyBeacon == true) {
      scheduler_.cancelEvent(sync_notify_beacon_scheduler);
      sync_notify_beacon_scheduler = scheduler_.scheduleEvent(kSyncNotifyBeaconTimer, [this] { OnSyncNotifyBeaconTimeout(); });
    }
  }
  else {
    auto n = pending_interest.begin()->first;
    int cur_transmission_time = pending_interest.begin()->second;
    if (cur_transmission_time != kInterestTransmissionTime) {
      // add the collision_num (retransmission num)
      collision_num++;
    }

    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());
    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    // set the wt
    assert(wt_list.find(n) == wt_list.end());
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time - 1); });
    pending_interest.erase(n);
  }

  scheduler_.cancelEvent(inst_dt);
  out_interest_num++;  
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
  assert(pending_interest.find(name) == pending_interest.end());
  assert(data_store_.find(name) == data_store_.end());
  pending_interest[name] = cur_transmission_time;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
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
    if (pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
      in_dt = false;
      return;
    }
  }
  else if (wt_list.find(n) != wt_list.end()) return;
  else {
    // even if you don't need to fetch this data, you can add the corresponding pit
    // you should directly add the interest into the pending list
    /*
    Interest i(n, kAddToPitInterestLifetime);
    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    // we also need to set a corresponding WT
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n] { OnWTTimeout(n, kInterestTransmissionTime); });
    */
    Interest i(n, kSendOutInterestLifetime);
    pending_interest[n] = kInterestTransmissionTime;
    if (in_dt == false) {
      in_dt = true;
      SendDataInterest();
    }
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
    latest_data = n;

    if (pending_interest.find(n) != pending_interest.end()) {
      pending_interest.erase(n);
      if (pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
        scheduler_.cancelEvent(inst_dt);
        in_dt = false;
      }
    }
    else if (wt_list.find(n) != wt_list.end()) {
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

    // find the missing_data
    std::vector<NodeID> missing_data;
    for (auto entry: other_vv) {
      NodeID node_id = entry.first;
      uint64_t other_seq = entry.second;
      if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
        // current vv don't have the node_id
        version_vector_[node_id] = other_seq;
        missing_data.push_back(node_id);
      }
    }
    FetchMissingData(missing_data);
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
