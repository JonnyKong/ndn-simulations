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
static time::milliseconds kInterestWT = time::milliseconds(100);
static time::seconds kSyncDataTimer = time::seconds(10);
static time::milliseconds kSendOutInterestLifetime = time::milliseconds(100);
static time::milliseconds kAddToPitInterestLifetime = time::milliseconds(54);

/*
static const int kSimulationRecordingTime = 180;
static const int kSnapshotNum = 90;
static time::milliseconds kSnapshotInterval = time::milliseconds(2000);
static const std::string availabilityFileName = "availability.txt";
*/
static const time::milliseconds kLogPositionInterval = time::milliseconds(1000);

static const int data_generation_rate_mean = 40000;

static time::seconds kDetectIsolationTimer = time::seconds(20);
// static time::seconds kHeartbeatTimer = time::seconds(2);

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
std::uniform_int_distribution<> dt_dist(0, kInterestDT);

static const bool kSyncNotifyBeacon = false;
static time::seconds kSyncNotifyBeaconTimer = time::seconds(6);

static const bool kSyncNotifyRetx = false;
static int kSyncNotifyMax = 3;
static time::seconds kSyncNotifyRetxTimer = time::seconds(3);

static const bool kBeacon = false;
std::uniform_int_distribution<> beacon_dist(1000000, 2000000);

/*
static const bool kHeartbeat = true;
static time::seconds kHeartbeatTimer = time::seconds(5);
static time::seconds kDetectPartitionTimer = time::seconds(20);
static time::seconds kAdjustHeartbeatTimer = time::seconds(10);
*/

static int kMaxDataContent = 4000;

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, Node::DataCb on_data, Node::GetCurrentPos getCurrentPos,
           bool useHeartbeat, bool useFastResync, uint64_t heartbeatTimer, uint64_t detectPartitionTimer)
           : face_(face),
             key_chain_(key_chain),
             nid_(nid),
             prefix_(prefix),
             scheduler_(scheduler),
             data_cb_(std::move(on_data)),
             get_current_pos_(getCurrentPos),
             rengine_(rdevice_()) {
  collision_num = 0;
  suppression_num = 0;
  out_interest_num = 0;
  in_dt = false;
  data_num = 0;
  latest_data = Name("/");
  pending_sync_notify = Name("/");
  notify_time = 0;
  // pending_bundled_interest = Name("/");
  // initialize the version_vector_ and heartbeat_vector_
  version_vector_[nid_] = 0;
  heartbeat_vector_[nid_] = 0;
  // initialize the data_store_: put Name("/") into data_store_, because sometimes when we send syncNotify because of heartbearts
  // there are no data in the data_store_. So we put an emtry data to Name("/")
  auto n = Name("/");
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setFreshnessPeriod(time::seconds(3600));
  key_chain_.sign(*data, signingWithSha256());
  data_store_[Name("/")] = data;
  generate_data = true;
  retx_notify_interest = 0;
  retx_data_interest = 0;
  retx_bundled_interest = 0;

  kHeartbeat = useHeartbeat;
  kFastResync = useFastResync;
  kHeartbeatTimer = time::seconds(heartbeatTimer);
  kDetectPartitionTimer = time::seconds(detectPartitionTimer);

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
      Name(kBundledDataPrefix).appendNumber(nid_), std::bind(&Node::OnBundledDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register BundledDataInterest prefix: " << reason); 
        throw Error("Failed to register BundledDataInterest prefix: " + reason);
      });  

  /*
  face_.setInterestFilter(
      Name("/test"), std::bind(&Node::OnTestInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register BundledDataInterest prefix: " << reason); 
        throw Error("Failed to register BundledDataInterest prefix: " + reason);
      });
  */

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
  // scheduler_.scheduleEvent(time::seconds(kSimulationRecordingTime), [this] { PrintNDNTraffic(); });
  scheduler_.scheduleEvent(time::seconds(900), [this] {
    generate_data = false;
  });

  // record the ndn traffic
  scheduler_.scheduleEvent(time::seconds(2600), [this] {
    // std::cout << "node(" << nid_ << ") outInterest = " << out_interest_num << std::endl;
    // std::cout << "node(" << nid_ << ") average time to meet a new node = " << total_time / (double)count << std::endl;
    std::cout << "node(" << nid_ << ") retx_notify_interest = " << retx_notify_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_data_interest = " << retx_data_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_bundled_interest = " << retx_bundled_interest << std::endl;
    PrintNDNTraffic();
  });
}

/*
void Node::OnIncomingPacketNotify(const Interest& interest) {
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Recv Packet From Others" );
  if (!is_isolated) {
    recv_count++;
  }
  else {
    scheduler_.cancelEvent(heartbeatScheduler);
    scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
    assert(recv_count == 0);
    is_isolated = false;
    SendSyncNotify();
  }
}
*/

void Node::SendBeacon() {
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Send Beacon" );
  auto n = MakeBeaconName(nid_);
  Interest i(n, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  int next_beacon = beacon_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeacon(); });
}

void Node::UpdateHeartbeat() {
  VSYNC_LOG_TRACE ( "node(" << nid_ << ") Update Heartbeat Vector" );
  heartbeat_vector_[nid_]++;
  scheduler_.scheduleEvent(kHeartbeatTimer, [this] { UpdateHeartbeat(); });
}

void Node::SendHeartbeat() {
  SendSyncNotify();
  heartbeat_scheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
}

void Node::AdjustHeartbeatTimer() {
}

void Node::PrintNDNTraffic() {
  Interest i(kGetNDNTraffic, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnDetectPartitionTimeout(NodeID node_id) {
  VSYNC_LOG_TRACE( "node(" << nid_ << ") DetectPartitionTimeout for node(" << node_id << ")" );
  // erase the node_id from history infoemation
  scheduler_.cancelEvent(detect_partition_timer[node_id]);
  detect_partition_timer.erase(node_id);
}

void Node::StartSimulation() {
  // scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });
  // scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
  if (kHeartbeat == true) {
    scheduler_.scheduleEvent(kHeartbeatTimer, [this] { UpdateHeartbeat(); });
    heartbeat_scheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
    // adjust_heartbeat_scheduler = scheduler_.scheduleEvent(kAdjustHeartbeatTimer, [this] { AdjustHeartbeatTimer(); });
  }
  if (kBeacon == true) {
    int next_beacon = beacon_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeacon(); });
  }

  std::string content = std::string(100, '*');
  last = ns3::Simulator::Now().GetMicroSeconds();
  scheduler_.scheduleEvent(time::milliseconds(10 * nid_),
                           [this, content] { PublishData(content); });
}

/*
// for test the how many data pieces the DATA can hold
void Node::StartSimulation() {
  if (nid_ == 0) {
    Name test_name = Name("/test");
    Interest inst(test_name);
    std::bind(&Node::OnBundledData, this, _2),
    face_.expressInterest(inst, [this](const Interest&, const Data&) {
      std::cout << "node(" << nid_ << ") recv data" << std::endl;
    },
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
  }
}

void Node::OnTestInterest(const Interest& interest) {
  proto::PackData pack_data_proto;
  int cur_size = 0;
  int pkt_num = 0;
  while (cur_size < 6000) {
    auto* entry = pack_data_proto.add_entry();
    entry->set_name("/" + to_string(pkt_num));
    std::string content = "hello from test";
    entry->set_content(content);
    cur_size = pack_data_proto.SerializeAsString().size();
    pkt_num += 1;
  }
  std::cout << "number of pack data: " << pkt_num << std::endl;
  std::string data_content = pack_data_proto.SerializeAsString();
  std::shared_ptr<Data> data = std::make_shared<Data>(interest.getName());
  data->setContent(reinterpret_cast<const uint8_t*>(data_content.data()),
                   data_content.size());
  key_chain_.sign(*data, signingWithSha256());
  face_.put(*data);
  std::cout << "node(" << nid_ << ") send out data" << std::endl;
}
*/

void Node::PublishData(const std::string& content, uint32_t type) {
  if (generate_data == false) return;
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

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Publish Data: d.name=" << n.toUri() );

  scheduler_.scheduleEvent(time::milliseconds(data_generation_dist(rengine_)),
                           [this, content] { PublishData(content); });

  if (data_num >= 1) {
    data_num = 0;
    SendSyncNotify();
  }
}

/****************************************************************/
/* pipeline for sync-requester node                             */
/* The sync-requester node will retransmit sync interest for    */
/* most three times if it doesn't receive any data              */
/****************************************************************/

void Node::SendSyncNotify() {
  std::string encoded_vv = EncodeVVToName(version_vector_);
  std::string encoded_hv = EncodeVVToName(heartbeat_vector_);
  // auto data_block = data_store_[latest_data]->wireEncode();
  auto sync_notify_interest_name = MakeSyncNotifyName(nid_, encoded_vv, encoded_hv);

  pending_sync_notify = sync_notify_interest_name;
  notify_time = kSyncNotifyMax;

  // pending_interest[sync_notify_interest_name] = 1;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
}

/****************************************************************************/
/* pipeline for sync-responder                                              */
/* 1. receive sync interest, is_syncing = true, generate missing_data list  */
/* most three times if it doesn't receive any data interests or             */
/* SyncACK interest                                                         */
/****************************************************************************/
void Node::OnSyncNotify(const Interest& interest) {
  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest" );
  const auto& n = interest.getName();

  // extract node_id
  NodeID node_id = ExtractNodeID(n);
  // extract data
  // std::shared_ptr<Data> carried_data = std::make_shared<Data>(n.get(-1).blockFromValue());
  // extract the hv
  auto other_hv = DecodeVVFromName(ExtractEncodedHV(n));
  // extract the vv
  /*
  const auto& content = carried_data->getContent();
  proto::Content content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data content format: nid=" << nid_);
    assert(false);
  }
  */
  auto other_vv = DecodeVVFromName(ExtractEncodedVV(n));

  // send back ack
  std::shared_ptr<Data> ack = std::make_shared<Data>(n);
  VersionVector difference;
  for (auto entry: version_vector_) {
    auto node_id = entry.first;
    auto seq = entry.second;
    if (other_vv.find(node_id) == other_vv.end() || other_vv[node_id] < seq) difference[node_id] = seq;
  }
  proto::AckContent content_proto;
  EncodeVV(difference, content_proto.mutable_vv());
  const std::string& content_proto_str = content_proto.SerializeAsString();
  ack->setContent(reinterpret_cast<const uint8_t*>(content_proto_str.data()),
                  content_proto_str.size());
  key_chain_.sign(*ack, signingWithSha256());
  face_.put(*ack);

  // 1. store the current data
  /*
  auto data_name = carried_data->getName();
  if (data_store_.find(data_name) == data_store_.end()) {
    data_store_[data_name] = carried_data;
    logDataStore(data_name);

    auto seq_owner = ExtractNodeID(data_name);
    auto seq = ExtractSequence(data_name);
    recv_window[seq_owner].Insert(seq);
    latest_data = data_name;

    if (pending_interest.find(data_name) != pending_interest.end()) {
      pending_interest.erase(data_name);
      if (pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
        scheduler_.cancelEvent(inst_dt);
        in_dt = false;
      }
    }
    else if (wt_list.find(data_name) != wt_list.end()) {
      scheduler_.cancelEvent(wt_list[data_name]);
      wt_list.erase(data_name);
    }
  }
  */

  bool need_fast_resync = false;
  // 2. detect the new comer
  if (detect_partition_timer.find(node_id) == detect_partition_timer.end()) {
    // fast exchange of missing data
    /*
    if (nid_ == 9) {
      // print the mobile node's position for debugging
      if (node_id == 4) {
        VSYNC_LOG_TRACE( "node(9) Detect node(" << node_id << "), current pos = " << get_current_pos_() - 400.0 ); 
      }
      else if (node_id == 5) {
        VSYNC_LOG_TRACE( "node(9) Detect node(" << node_id << "), current pos = " << 1000.0 - get_current_pos_() ); 
      }
      else assert(false);
    }
    else if (nid_ == 4 && node_id == 9) {
      VSYNC_LOG_TRACE( "node(4) Detect node(9)" ); 
    }
    else if (nid_ == 5 && node_id == 9) {
      VSYNC_LOG_TRACE( "node(5) Detect node(9)" ); 
    }
    */
    // SendBundledDataInterest(node_id, other_vv);
    if (kFastResync) {
      VersionVector mv;
      for (auto entry: other_vv) {
        auto entry_id = entry.first;
        auto entry_seq = entry.second;
        if (entry_seq == 0) continue;
        if (version_vector_.find(entry_id) == version_vector_.end()) {
          mv[entry_id] = 1;
        }
        else if (version_vector_[entry_id] < entry_seq) {
          mv[entry_id] = version_vector_[entry_id] + 1;
        }
      }
      SendBundledDataInterest(node_id, mv);
    }

    // detect the average time to meet a new node
    /*
    std::cout << "node(" << nid_ << ") detect a new node(" << node_id << ")" << std::endl;
    count += 1;
    int64_t cur = ns3::Simulator::Now().GetMicroSeconds();
    total_time += (double)(cur - last) / 1000000;
    last = cur;
    */
  }

  // 3. update local version_vector_ and heartbeat_vector_
  // std::vector<NodeID> missing_data;
  bool updated = false;
  for (auto entry: other_vv) {
    NodeID node_id = entry.first;
    uint64_t other_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
      // current vv don't have the node_id
      updated = true;
      // missing_data.push_back(node_id);
      version_vector_[node_id] = other_seq;
      // update the corresponding detect_partition_timer
      scheduler_.cancelEvent(detect_partition_timer[node_id]);
      detect_partition_timer[node_id] = scheduler_.scheduleEvent(kDetectPartitionTimer,
        [this, node_id] { OnDetectPartitionTimeout(node_id); });
    }
  }
  for (auto entry: other_hv) {
    NodeID node_id = entry.first;
    uint64_t other_heartbeat = entry.second;
    if (heartbeat_vector_.find(node_id) == heartbeat_vector_.end() || heartbeat_vector_[node_id] < other_heartbeat) {
      heartbeat_vector_[node_id] = other_heartbeat;
      // detect the average time to meet a new node
      // updated = true;
      // update the corresponding detect_partition_timer
      scheduler_.cancelEvent(detect_partition_timer[node_id]);
      detect_partition_timer[node_id] = scheduler_.scheduleEvent(kDetectPartitionTimer,
        [this, node_id] { OnDetectPartitionTimeout(node_id); });
    }
  }

  if (updated == true) {
    VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a New State Vector" );
    // 4. re-send the merged vector
    // send out the notify, add it into the pending_interest list. There is no retransmission for notify
    // the state vector in the notify should be the merged one
    SendSyncNotify();
  }

  // 5. fetch missing data
  FetchMissingData();
}

void Node::SendBundledDataInterest(const NodeID& recv_id, VersionVector mv) {
  /*
  VersionVector mv;
  for (auto entry: other_vv) {
    auto nid = entry.first;
    auto seq = entry.second;
    if (seq == 0) continue;
    if (version_vector_.find(nid) == version_vector_.end()) {
      mv[nid] = 1;
    }
    else if (version_vector_[nid] < seq) {
      mv[nid] = version_vector_[nid] + 1;
    }
  }
  */
  if (mv.empty()) return;
  auto n = MakeBundledDataName(recv_id, EncodeVVToName(mv));
  if (pending_bundled_interest.find(n) != pending_bundled_interest.end() ||
    wt_list.find(n) != wt_list.end()) {
    return;
  }
  pending_bundled_interest[n] = kInterestTransmissionTime;
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
}

void Node::OnBundledDataInterest(const Interest& interest) {
  auto n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Bundled Data Interest: " << n.toUri() );

  auto missing_data = DecodeVVFromName(ExtractEncodedMV(n));
  proto::PackData pack_data_proto;
  VersionVector next_vv = missing_data;
  for (auto item: missing_data) {
    auto node_id = item.first;
    auto start_seq = item.second;
    assert(version_vector_.find(node_id) != version_vector_.end());
    bool exceed_max_size = false;

    for (auto seq = start_seq; seq <= version_vector_[node_id]; ++seq) {
      Name data_name = MakeDataName(node_id, seq);
      if (data_store_.find(data_name) == data_store_.end()) continue;
      auto* entry = pack_data_proto.add_entry();
      entry->set_name(data_name.toUri());
      // std::string content(data_store_[data_name]->getContent().value(), data_store_[data_name]->getContent().value() + data_store_[data_name]->getContent().value_size());
      entry->set_content(data_store_[data_name]->getContent().value(), data_store_[data_name]->getContent().value_size());

      int cur_data_content_size = pack_data_proto.SerializeAsString().size();
      if (cur_data_content_size >= kMaxDataContent) {
        exceed_max_size = true;
        if (seq == version_vector_[node_id]) next_vv.erase(node_id);
        else next_vv[node_id] = seq + 1;
        break;
      }
    }

    if (exceed_max_size == true) {
      break;
    }
    else {
      next_vv.erase(node_id);
    }
  }

  // add the next_vv tag
  EncodeVV(next_vv, pack_data_proto.mutable_nextvv());

  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Back Packed Data" );
  const std::string& pack_data = pack_data_proto.SerializeAsString();
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setContent(reinterpret_cast<const uint8_t*>(pack_data.data()),
                   pack_data.size());
  key_chain_.sign(*data);
  face_.put(*data);
}

void Node::FetchMissingData() {
  for (auto entry: version_vector_) {
    auto node_id = entry.first;
    auto node_seq = entry.second;
    ReceiveWindow::SeqNumIntervalSet missing_interval = recv_window[node_id].CheckForMissingData(node_seq);
    if (missing_interval.empty()) continue;
    auto it = missing_interval.begin();
    while (it != missing_interval.end()) {
      for (uint64_t seq = it->lower(); seq <= it->upper(); ++seq) {
        //missing_data.push_back(MissingData(i, seq));
        if (seq == 0) continue;
        Name data_interest_name = MakeDataName(node_id, seq);
        if (data_store_.find(data_interest_name) != data_store_.end()) {
          recv_window[node_id].Insert(seq);
          continue;
        }
        if (pending_interest.find(data_interest_name) != pending_interest.end() &&
          pending_interest[data_interest_name] != 0) continue;
        else if (wt_list.find(data_interest_name) != wt_list.end()) continue;
        pending_interest[data_interest_name] = kInterestTransmissionTime;
      }
      it++;
    }
  }

  // print the pending interest
  std::string pending_list = pending_sync_notify.getPrefix(5).toUri() + "\n";
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
  if (pending_bundled_interest.empty() && pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    // scheduler_.cancelEvent(inst_wt);
    in_dt = false;
    return;
  }

  int delay = dt_dist(rengine_);
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
}

void Node::OnDTTimeout() {
  // erase invalid pending_interest
  // VSYNC_LOG_TRACE("node(" << nid_ << ") OnDTTimeout");
  if (!pending_interest.empty()) {
    std::vector<Name> erase_name;
    auto it = pending_interest.begin();
    while (it != pending_interest.end()) {
      if (data_store_.find(it->first) != data_store_.end() || it->second == 0) {
        erase_name.push_back(it->first);
      }
      it++;
    }
    for (auto name: erase_name) pending_interest.erase(name);
  }
  // erase invalid pending_bundled_interest
  if (!pending_bundled_interest.empty()) {
    std::vector<Name> erase_name;
    auto it = pending_bundled_interest.begin();
    while (it != pending_bundled_interest.end()) {
      if (it->second == 0) {
        erase_name.push_back(it->first);
      }
      it++;
    }
    for (auto name: erase_name) pending_bundled_interest.erase(name);
  }
  // erase invalid notify_interest
  if (notify_time == 0) pending_sync_notify = Name("/");

  if (pending_bundled_interest.empty() && pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
    scheduler_.cancelEvent(inst_dt);
    in_dt = false;
    return;
  }
  /*
  if (nid_ == 9) {
    VSYNC_LOG_TRACE( "node(9) will send interest, current position: " << get_current_pos_() );
  }
  */
  // choose an interest to send out. BundledInterest has highest priority, then SyncNotify
  if (!pending_bundled_interest.empty()) {
    auto n = pending_bundled_interest.begin()->first;
    auto cur_transmission_time = pending_bundled_interest.begin()->second;
    assert(cur_transmission_time != 0);
    if (cur_transmission_time != kInterestTransmissionTime) retx_bundled_interest++;

    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send BundledDataInterest: i.name=" << n.getPrefix(4).toUri());
    face_.expressInterest(i, std::bind(&Node::OnBundledData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    // set the wt
    assert(wt_list.find(n) == wt_list.end());
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time - 1); });
    pending_bundled_interest.erase(n);
  }
  else if (pending_sync_notify.compare(Name("/")) != 0) {
    auto n = pending_sync_notify;
    pending_sync_notify = Name("/");
    assert(notify_time != 0);

    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(5).toUri());
    face_.expressInterest(i, std::bind(&Node::onNotifyACK, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    if (notify_time != kSyncNotifyMax) retx_notify_interest++;
    waiting_sync_notify = n;
    scheduler_.cancelEvent(wt_notify);
    wt_notify = scheduler_.scheduleEvent(kInterestWT, [this, n] { OnWTTimeout(n, notify_time - 1); });

    if (kHeartbeat == true) {
      scheduler_.cancelEvent(heartbeat_scheduler);
      heartbeat_scheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
    }
  }
  else {
    auto n = pending_interest.begin()->first;
    int cur_transmission_time = pending_interest.begin()->second;
    assert(cur_transmission_time != 0);
    if (cur_transmission_time != kInterestTransmissionTime) {
      // add the collision_num (retransmission num)
      retx_data_interest++;
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

  out_interest_num++;
  scheduler_.cancelEvent(inst_dt);  
  if (pending_bundled_interest.empty() && pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
    in_dt = false;
    return;
  }

  int delay = dt_dist(rengine_);
  inst_dt = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnDTTimeout(); });
}

void Node::OnWTTimeout(const Name& name, int cur_transmission_time) {
  // VSYNC_LOG_TRACE ("node(" << nid_ << ") WT time out for: " << name.toUri());
  if (name.compare(0, 2, kBundledDataPrefix) == 0) {
    wt_list.erase(name);
    // An interest cannot exist in WT and DT at the same time 
    assert(pending_bundled_interest.find(name) == pending_bundled_interest.end());
    pending_bundled_interest[name] = cur_transmission_time;
  }
  else if (name.compare(0, 2, kSyncNotifyPrefix) == 0) {
    VSYNC_LOG_TRACE ("node(" << nid_ << ") WT time out for syncNotify: " << name.toUri());
    if (pending_sync_notify.compare(Name("/")) == 0) {
      VSYNC_LOG_TRACE ("node(" << nid_ << ") set up the retx syncNotify");
      pending_sync_notify = name;
      notify_time = cur_transmission_time;
    }
  }
  else {
    wt_list.erase(name);
    // may have been cached by fast recover
    if (data_store_.find(name) != data_store_.end()) return;
    // An interest cannot exist in WT and DT at the same time
    assert(pending_interest.find(name) == pending_interest.end());
    pending_interest[name] = cur_transmission_time;
  }
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
    /*
    suppression_num++;
    Interest i(n, kAddToPitInterestLifetime);

    face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    int cur_transmission_time = pending_interest.begin()->second;
    wt_list[n] = scheduler_.scheduleEvent(kInterestWT, [this, n, cur_transmission_time] { OnWTTimeout(n, cur_transmission_time); });
    
    pending_interest.erase(n);
    if (pending_bundled_interest.compare(Name("/")) == 0 && pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
      in_dt = false;
      return;
    }
    */
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
      if (pending_bundled_interest.empty() && pending_sync_notify.compare(Name("/")) == 0 && pending_interest.empty()) {
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

    // fetch the missing_data
    for (auto entry: other_vv) {
      NodeID node_id = entry.first;
      uint64_t other_seq = entry.second;
      // ignore the 0 sequence
      if (other_seq == 0) continue;
      if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
        // current vv don't have the node_id
        version_vector_[node_id] = other_seq;
      }
    }
    FetchMissingData();
  }
}

void Node::OnBundledData(const Data& data) {
  const auto& content = data.getContent();
  proto::PackData pack_data_proto;
  if (!pack_data_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN( "Invalid syncNotifyNotify reply content format" );
    return;
  }
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Bundled Data" );
  for (int i = 0; i < pack_data_proto.entry_size(); ++i) {
    const auto& entry = pack_data_proto.entry(i);
    auto data_name = Name(entry.name());
    if (data_store_.find(data_name) != data_store_.end()) return;
    std::string data_content = entry.content();
    std::shared_ptr<Data> data = std::make_shared<Data>(data_name);
    data->setContent(reinterpret_cast<const uint8_t*>(data_content.data()),
                     data_content.size());
    key_chain_.sign(*data, signingWithSha256());
    /*
    std::string data_piece = pack_data_proto.data(i);
    auto data_block = Block(reinterpret_cast<const uint8_t*>(data_piece.data(), data_piece.size()), data_piece.size());
    std::shared_ptr<Data> data = std::make_shared<Data>(data_block);
    auto data_name = data->getName();
    */

    data_store_[data_name] = data;

    auto data_nid = ExtractNodeID(data_name);
    auto data_seq = ExtractSequence(data_name);
    recv_window[data_nid].Insert(data_seq);
    if (wt_list.find(data_name) != wt_list.end()) wt_list.erase(data_name);
    if (pending_interest.find(data_name) != pending_interest.end()) pending_interest.erase(data_name);
    logDataStore(data_name);
  }

  auto n = data.getName();
  auto recv_nid = ExtractNodeID(n);
  auto next_vv = DecodeVV(pack_data_proto.nextvv());
  SendBundledDataInterest(recv_nid, next_vv);
}

void Node::onNotifyACK(const Data& ack) {
  const auto& n = ack.getName();
  if (n.compare(waiting_sync_notify) == 0) {
    scheduler_.cancelEvent(wt_notify);
    VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV NotifyACK: " << ack.getName().toUri());
  }
  else {
    VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV outdate NotifyACK: " << ack.getName().toUri());
  }
  // process the difference in ack
  const auto& content = ack.getContent();
  proto::AckContent content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data AckContent format: nid=" << nid_);
    assert(false);
  }
  auto difference = DecodeVV(content_proto.vv());
  // fetch the missing_data
  for (auto entry: difference) {
    auto node_id = entry.first;
    auto other_seq = entry.second;
    // ignore the 0 sequence
    if (other_seq == 0) continue;
    if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < other_seq) {
      // current vv don't have the node_id
      version_vector_[node_id] = other_seq;
    }
  }
  FetchMissingData();
}

void Node::logDataStore(const Name& name) {
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Store New Data: " << name.toUri() << std::endl;
}

// print the vector clock every 5 seconds
/*
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  vv_snapshots.push_back(version_vector_);
  rw_snapshots.push_back(recv_window);
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
}
*/
  
}  // namespace vsync
}  // namespace ndn
