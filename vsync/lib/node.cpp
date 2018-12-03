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
// static time::milliseconds kAddToPitInterestLifetime = time::milliseconds(54);

/*
static const int kSimulationRecordingTime = 180;
static const int kSnapshotNum = 90;
static time::milliseconds kSnapshotInterval = time::milliseconds(2000);
static const std::string availabilityFileName = "availability.txt";
*/
static const int data_generation_rate_mean = 40000;

std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
std::uniform_int_distribution<> dt_dist(0, kInterestDT);

static int kSyncNotifyMax = 3;

// normal: 1s - 2s, lifetime = 4s
std::uniform_int_distribution<> beacon_dist(2000000, 3000000);
std::uniform_int_distribution<> ack_dist(100000, 200000);
static time::seconds kBeaconLifetime = time::seconds(6);
// std::uniform_int_distribution<> heartbeat_dist(2000000, 3000000);
// static time::seconds kHeartbeatLifetime = time::seconds(6);
static time::seconds kRetxTimer = time::seconds(2);
// static time::seconds kBeaconFloodLifetime = time::seconds(6);

/* Distribution for multi-hop */
std::uniform_int_distribution<> mhop_dist(0, 10000);
static int pMultihopForwardSyncInterest1 = 3000;
static int pMultihopForwardSyncInterest2 = 7000;
static int pMultihopForwardDataInterest = 5000;

static int kMaxDataContent = 4000;
static const int kMissingDataThreshold = 10;

Node::Node(Face& face, 
           Scheduler& scheduler, 
           KeyChain& key_chain,
           const NodeID& nid, 
           const Name& prefix, 
           Node::DataCb on_data, 
           Node::GetCurrentPos getCurrentPos,
          //  bool useHeartbeat, 
          //  bool useHeartbeatFlood, 
           bool useBeacon, 
          //  bool useBeaconSuppression, 
          //  bool useBeaconFlood,
           bool useRetx) 
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
  data_num = 0;
  pending_sync_notify = Name("/");
  notify_time = 0;
  // pending_bundled_interest = Name("/");
  // initialize the version_vector_ and heartbeat_vector_
  version_vector_[nid_] = 0;
  // heartbeat_vector_[nid_] = 0;
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

  // kHeartbeat = useHeartbeat;
  // kHeartbeatFlood = useHeartbeatFlood;
  kBeacon = useBeacon;
  // kBeaconSuppression = useBeaconSuppression;
  kRetx = useRetx;
  // kBeaconFlood = useBeaconFlood;

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

  face_.setInterestFilter(
      Name(kBeaconPrefix), std::bind(&Node::OnBeacon, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register BeaconInterest prefix: " << reason); 
        throw Error("Failed to register BeaconInterest prefix: " + reason);
      });

  // face_.setInterestFilter(
  //     Name(kHeartbeatPrefix), std::bind(&Node::OnHeartbeat, this, _2),
  //     [this](const Name&, const std::string& reason) {
  //       VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register HeartbeatInterest prefix: " << reason); 
  //       throw Error("Failed to register HeartbeatInterest prefix: " + reason);
  //     });  

  // face_.setInterestFilter(
  //     Name(kBeaconFloodPrefix), std::bind(&Node::OnBeaconFlood, this, _2),
  //     [this](const Name&, const std::string& reason) {
  //       VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register BeaconFloodInterest prefix: " << reason); 
  //       throw Error("Failed to register BeaconFloodInterest prefix: " + reason);
  //     });

  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });
  // scheduler_.scheduleEvent(time::seconds(kSimulationRecordingTime), [this] { PrintNDNTraffic(); });
  scheduler_.scheduleEvent(time::seconds(400), [this] {
    generate_data = false;
  });

  // record the ndn traffic
  scheduler_.scheduleEvent(time::seconds(1195), [this] {
    // std::cout << "node(" << nid_ << ") outInterest = " << out_interest_num << std::endl;
    // std::cout << "node(" << nid_ << ") average time to meet a new node = " << total_time / (double)count << std::endl;
    std::cout << "node(" << nid_ << ") retx_notify_interest = " << retx_notify_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_data_interest = " << retx_data_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_bundled_interest = " << retx_bundled_interest << std::endl;
    // PrintNDNTraffic();
  });

  scheduler_.scheduleEvent(time::seconds(1196), [this] {
    uint64_t seq_sum = 0;
    for (auto entry: version_vector_) {
      seq_sum += entry.second;
    }
    std::cout << "node(" << nid_ << ") seq sum: " << seq_sum << std::endl;
    // std::cout << "node(" << nid_ << ") seq = " << version_vector_[nid_] << std::endl;
  });
}

void Node::SendBeacon() {
  auto n = MakeBeaconName(nid_);
  Interest i(n, time::milliseconds(1));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  int next_beacon = beacon_dist(rengine_);
  beacon_event = scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeacon(); });
}

void Node::OnBeacon(const Interest& beacon) {
  auto n = beacon.getName();
  auto node_id = ExtractNodeID(n);
  if (one_hop.find(node_id) == one_hop.end()) {
    // fast resync, send notify Interest
    std::string one_hop_list = to_string(node_id);
    for (auto entry: one_hop) one_hop_list += ", " + to_string(entry.first);
    // VSYNC_LOG_TRACE ("node(" << nid_ << ") detect a new one-hop node: " << node_id << ", the current one-hop list: " << one_hop_list);
    SendSyncNotify();
  }
  // update the one_hop info
  scheduler_.cancelEvent(one_hop[node_id]);
  one_hop[node_id] = scheduler_.scheduleEvent(kBeaconLifetime, [this, node_id] {
    one_hop.erase(node_id);
  });

  // if use suppression for beacon
  if (kBeaconSuppression) {
    scheduler_.cancelEvent(beacon_event);
    int next_beacon = beacon_dist(rengine_);
    beacon_event = scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeacon(); });
  }
}

// void Node::SendBeaconFlood() {
//   beacon_vector_[nid_]++;
//   auto n = MakeBeaconFloodName(nid_, nid_, beacon_vector_[nid_]);
//   // VSYNC_LOG_TRACE ( "node(" << nid_ << ") SEND a new beaconflood interest: " << n.toUri() );

//   Interest i(n, time::milliseconds(1));
//   face_.expressInterest(i, [](const Interest&, const Data&) {},
//                         [](const Interest&, const lp::Nack&) {},
//                         [](const Interest&) {});
//   int next_beacon = beacon_dist(rengine_);
//   beacon_flood_event = scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeaconFlood(); });
// }

// void Node::OnBeaconFlood(const Interest& beacon) {
//   auto n = beacon.getName();
//   // VSYNC_LOG_TRACE ( "node(" << nid_ << ") RECV a new beaconflood interest");
//   auto sender = ExtractBeaconSender(n);
//   auto initializer = ExtractBeaconInitializer(n);
//   auto seq = ExtractBeaconSeq(n);
//   if (beacon_vector_[initializer] >= seq) return;

//   if (connected_group.find(sender) == connected_group.end()) {
//     // fast resync, send notify Interest
//     std::string connected_list = to_string(sender);
//     for (auto entry: connected_group) connected_list += ", " + to_string(entry.first);
//     VSYNC_LOG_TRACE ("node(" << nid_ << ") detect a new connected node: " << sender << ", the current connected list: " << connected_list);
    
//     SendSyncNotify();
//   }

//   beacon_vector_[initializer] = seq;
//   scheduler_.cancelEvent(connected_group[sender]);
//   scheduler_.cancelEvent(connected_group[initializer]);
//   connected_group[sender] = scheduler_.scheduleEvent(kBeaconFloodLifetime, [this, sender] {
//     connected_group.erase(sender);
//   });
//   connected_group[initializer] = scheduler_.scheduleEvent(kBeaconFloodLifetime, [this, initializer] {
//     connected_group.erase(initializer);
//   });

//   // flood the current beacon
//   auto flood_beacon_name = MakeBeaconFloodName(nid_, initializer, seq);
//   int delay = dt_dist(rengine_);
//   scheduler_.scheduleEvent(time::microseconds(delay), [this, flood_beacon_name] {
//     Interest i(flood_beacon_name, time::milliseconds(1));
//     face_.expressInterest(i, [](const Interest&, const Data&) {},
//                         [](const Interest&, const lp::Nack&) {},
//                         [](const Interest&) {});
//   });
// }

// void Node::SendHeartbeat() {
//   // SendSyncNotify();
//   // heartbeat_scheduler = scheduler_.scheduleEvent(kHeartbeatTimer, [this] { SendHeartbeat(); });
//   heartbeat_vector_[nid_]++;
//   std::string encoded_hv = EncodeVVToName(heartbeat_vector_);
//   std::string tag = to_string(nid_) + "-" + to_string(heartbeat_vector_[nid_]);
//   auto n = MakeHeartbeatName(nid_, encoded_hv, tag);
//   Interest i(n, time::milliseconds(1));
//   face_.expressInterest(i, [](const Interest&, const Data&) {},
//                         [](const Interest&, const lp::Nack&) {},
//                         [](const Interest&) {});
//   int next_heartbeat = heartbeat_dist(rengine_);
//   heartbeat_event = scheduler_.scheduleEvent(time::microseconds(next_heartbeat), [this] { SendHeartbeat(); });
// }

// void Node::OnHeartbeat(const Interest& heartbeat) {
//   // ignore the heartbeat whose tag is old
//   auto n = heartbeat.getName();
//   auto node_id = ExtractNodeID(n);
//   /*
//   std::string tag = ExtractTag(n);
//   size_t dash = tag.find("-");
//   auto tag_id = std::stoull(tag.substr(0, dash));
//   uint64_t tag_seq = std::stoull(tag.substr(dash + 1));
//   if (heartbeat_vector_[tag_id] >= tag_seq) return;
//   */

//   if (partition_group.find(node_id) == partition_group.end()) {
//     std::string partition_list = to_string(node_id);
//     for (auto entry: partition_group) partition_list += ", " + to_string(entry.first);
//     // VSYNC_LOG_TRACE ("node(" << nid_ << ") detect a new partition-group node: " << node_id << ", the current partition list: " << partition_list);
//     SendSyncNotify();
//   }
//   // update the partition_group info
//   auto other_hv = DecodeVVFromName(ExtractEncodedHV(n));
//   for (auto entry: other_hv) {
//     auto entry_id = entry.first;
//     auto entry_seq = entry.second;
//     if (heartbeat_vector_.find(entry_id) == heartbeat_vector_.end() || heartbeat_vector_[entry_id] < entry_seq) {
//       heartbeat_vector_[entry_id] = entry_seq;
//       scheduler_.cancelEvent(partition_group[entry_id]);
//       partition_group[entry_id] = scheduler_.scheduleEvent(kHeartbeatLifetime, [this, entry_id] {
//         partition_group.erase(entry_id);
//       });
//     }
//   }

//   /*
//   if (kHeartbeatFlood) {
//     if (update == true) {
//       // need to forward the current heartbeat.
//       scheduler_.cancelEvent(heartbeat_event);
//       auto tag = ExtractTag(n);
//       heartbeat_vector_[nid_]++;

//     }
//   }
//   */
// }

/**
 * Keep scheduling SendSyncNotify() events.
 */
void Node::RetxSyncNotify() {
  SendSyncNotify();
  retx_event = scheduler_.scheduleEvent(kRetxTimer, [this] { RetxSyncNotify(); });
}

/**
 * Send an interest for getting NDN traffic.
 */
// void Node::PrintNDNTraffic() {
//   Interest i(kGetNDNTraffic, time::milliseconds(5));
//   face_.expressInterest(i, [](const Interest&, const Data&) {},
//                         [](const Interest&, const lp::Nack&) {},
//                         [](const Interest&) {});
// }

/**
 * Init necessary event scheduling, and then schedule the first publishData()
 *  event.
 */
void Node::StartSimulation() {
  // scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });
  // scheduler_.scheduleEvent(kDetectIsolationTimer, [this] { DetectIsolation(); });
  // if (kHeartbeat == true) {
  //   int next_heartbeat = heartbeat_dist(rengine_);
  //   scheduler_.scheduleEvent(time::microseconds(next_heartbeat), [this] { SendHeartbeat(); });
  // }
  if (kBeacon == true) {
    int next_beacon = beacon_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeacon(); });
  }
  if (kRetx == true) {
    retx_event = scheduler_.scheduleEvent(kRetxTimer, [this] { RetxSyncNotify(); });
  }
  // if (kBeaconFlood == true) {
  //   int next_beacon = beacon_dist(rengine_);
  //   beacon_flood_event = scheduler_.scheduleEvent(time::microseconds(next_beacon), [this] { SendBeaconFlood(); });
  // }

  std::string content = std::string(100, '*');
  last = ns3::Simulator::Now().GetMicroSeconds();
  scheduler_.scheduleEvent(time::milliseconds(10 * nid_),
                           [this, content] { PublishData(content); });
}

/**
 * Create data packet of given content, and insert into data_store_ and update
 *  version vector. Send sync notify.
 * Schedule next PublishData() event. 
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
  logStateStore(nid_, version_vector_[nid_]);
  recv_window[nid_].Insert(version_vector_[nid_]);

  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Publish Data: d.name=" << n.toUri() );

  scheduler_.scheduleEvent(time::milliseconds(data_generation_dist(rengine_)),
                           [this, content] { PublishData(content); });

  if (data_num >= 1) {
    data_num = 0;
    SendSyncNotify();
    if (kRetx) {
      scheduler_.cancelEvent(retx_event);
      retx_event = scheduler_.scheduleEvent(kRetxTimer, [this] { RetxSyncNotify(); });
    }
    // scheduler_.scheduleEvent(time::seconds(5), [this] { SendSyncNotify(); });
    // scheduler_.scheduleEvent(time::seconds(10), [this] { SendSyncNotify(); });
  }

  /* In case an ACK is scheduled to send, send the updated vector */

}

/****************************************************************/
/* pipeline for Notify Interest                                 */
/****************************************************************/

/**
 * Put version vector in name and send out with delay timer. This function sends
 *  the entire vector.
 * Set timeout callback.
 */
void Node::SendSyncNotify() {
  std::string encoded_vv = EncodeVVToName(version_vector_);
  // auto data_block = data_store_[latest_data]->wireEncode();
  auto cur_time = ns3::Simulator::Now().GetMicroSeconds();
  auto sync_notify_interest_name = MakeSyncNotifyName(nid_, encoded_vv, cur_time);

  pending_sync_notify = sync_notify_interest_name;
  notify_time = kSyncNotifyMax;
  vv_update = 0;

  // will send out a new notify interest
  scheduler_.cancelEvent(dt_notify);
  scheduler_.cancelEvent(wt_notify);
  int delay = dt_dist(rengine_);
  // std::cout << "will send out notify interest" << pending_sync_notify.toUri() << " after " << delay << std::endl;
  dt_notify = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnNotifyDTTimeout(); });
}

/**
 * Callback for sending sync interest with delay timer.
 * Async mutual recursion with OnNotifyWTTimeout().
 */
void Node::OnNotifyDTTimeout() {
  Interest i(pending_sync_notify, kSendOutInterestLifetime);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Notify Interest: i.name=" << pending_sync_notify.toUri());
  face_.expressInterest(i, std::bind(&Node::onNotifyACK, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  if (notify_time != kSyncNotifyMax) retx_notify_interest++;
  waiting_sync_notify = pending_sync_notify;
  scheduler_.cancelEvent(wt_notify);
  wt_notify = scheduler_.scheduleEvent(kInterestWT, [this] { OnNotifyWTTimeout(); });
}

/**
 * Callback for sync reply timeout. Just send again with random delay timer.
 * Async mutual recursion with OnNotifyDTTimeout().
 */
void Node::OnNotifyWTTimeout() {
  notify_time--;
  waiting_sync_notify = Name("/");
  if (notify_time == 0) return;
  // retransmit current notify interest. Current there must be no dt or wt
  int delay = dt_dist(rengine_);
  dt_notify = scheduler_.scheduleEvent(time::microseconds(delay), [this] { OnNotifyDTTimeout(); });
}

/**
 * Callback for receiving sync reply. Resolve all vector differences, and append 
 *  missing data names to a queue, then add this queue to pending_interest array.
 */
void Node::onNotifyACK(const Data& ack) {
  const auto& n = ack.getName();
  /* If reply is for outstanding sync notify, cancel timeout timer */
  if (n.compare(waiting_sync_notify) == 0) {
    scheduler_.cancelEvent(wt_notify);
    VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV NotifyACK: " << ack.getName().toUri());
  }
  else {
    VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV outdate NotifyACK: " << ack.getName().toUri());
  }

  /* Do a broadcast for multi-hop */
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, ack] {
    face_.put(ack);
  });

  // process the difference in ack
  const auto& content = ack.getContent();
  proto::AckContent content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data AckContent format: nid=" << nid_);
    assert(false);
  }
  auto difference = DecodeVV(content_proto.vv());
  std::queue<Name> q;
  for (auto entry: difference) {
    auto node_id = entry.first;
    auto node_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || version_vector_[node_id] < node_seq) {
      auto start_seq = version_vector_.find(node_id) == version_vector_.end() ? 1: version_vector_[node_id] + 1;
      for (auto seq = start_seq; seq <= node_seq; ++seq) {
        auto n = MakeDataName(node_id, seq);
        q.push(n);
      }
    }
  }
  if (!q.empty()) {
    pending_interest.push(q);
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  }
}

/**
 * Thunk for sending ack. 
 */
void Node::sendAck() {
  face_.put(*ack);
}


/****************************************************************************/
/* pipeline for Data Interest                                               */
/****************************************************************************/
/**
 * Listen for sync notify interests. When receiving a sync interest, compare
 *  the received vector with my own vector, and send out immediately only the 
 *  differences (i.e. nodes or sync numbers that I have but he doesn't).
 * Opportunistiscally forward sync interest.
 * After sending out vector differences, also check if I'm missing any data. If
 *  there is, fetch these data interests.
 */
void Node::OnSyncNotify(const Interest& interest) {
  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest" );
  const auto& n = interest.getName();

  NodeID node_id = ExtractNodeID(n);
  auto other_vv = DecodeVVFromName(ExtractEncodedVV(n));

  // send back ack
  // std::shared_ptr<Data> ack = std::make_shared<Data>(n);
  ack = std::make_shared<Data>(n);
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
  // face_.put(*ack);

  /* Send ACK */
  if (!difference.empty()) {
    /* If local vector contains newer state, send ACK immediately */
    sendAck();
    VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK immediately" );
  } else {
    /* If local vector outdated or equal, send ACK after some delay */
    int next_ack = ack_dist(rengine_);
    dt_ack = scheduler_.scheduleEvent(time::microseconds(next_ack), [this] {
      sendAck();
      VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK with delay" );
    });
  }

  /**
   * Forward sync interest probabilistically for multi-hop.
   * Default: Forward with probability p1.
   * Overhear interest with same name:  Forward with probability p2. (p1 < p2)
   */
  int p = mhop_dist(rengine_);
  bool forward = true;
  if (other_vv != version_vector_ && p > pMultihopForwardSyncInterest1) {
    forward = false;
  } else if (p > pMultihopForwardSyncInterest2) {
    forward = false;
  }
  /* Forward interest (with random delay) */
  if (forward) {
    int delay = dt_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(delay), [this, interest] {
      face_.expressInterest(interest, std::bind(&Node::onNotifyACK, this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
    });
  }

  /* Pipeline for missing data fetch and vector merge */
  std::queue<Name> q;
  int missing_data = 0;
  VersionVector mv;
  for (auto entry: other_vv) {
    auto entry_id = entry.first;
    auto entry_seq = entry.second;
    /* Merge state vector */
    if (version_vector_.find(entry_id) == version_vector_.end() || version_vector_[entry_id] < entry_seq) {
      auto start_seq = version_vector_.find(entry_id) == version_vector_.end() ? 1: version_vector_[entry_id] + 1;
      mv[entry_id] = start_seq;
      for (auto seq = start_seq; seq <= entry_seq; ++seq) {
        auto n = MakeDataName(entry_id, seq);
        missing_data++;
        q.push(n);
      }
    }
  }

  if (missing_data >= kMissingDataThreshold) {
    assert(!mv.empty());
    auto n = MakeBundledDataName(node_id, EncodeVVToName(mv));
    std::queue<Name> bundle_queue;
    bundle_queue.push(n);
    pending_interest.push(bundle_queue);
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  }
  else if (!q.empty()) {
    pending_interest.push(q);
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  }

  /*
  if (!q.empty()) {
    pending_interest.push(q);
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  }
  */
}

/**
 * Send interest for missing data, with random delay timer. 
 * Also set a timeout, if data is not received.
 */
void Node::SendDataInterest() {
  if (pending_interest.empty()) {
    return;
  }

  if (left_retx_count == 0) {
    pending_interest.pop();
    left_retx_count = kInterestTransmissionTime;
    if (vv_update != 0) {
      SendSyncNotify();
    }
    if (pending_interest.empty()) return;
  }

  // find a queue too start to send out interests
  if (!pending_interest.front().empty() && pending_interest.front().front().compare(0, 2, kSyncDataPrefix) == 0) {
    while (!pending_interest.front().empty()) {
      /* Remove falsy pending interests */
      if (data_store_.find(pending_interest.front().front()) != data_store_.end()) {
        pending_interest.front().pop();
      }
      else break;
    }
  }
  if (pending_interest.front().empty()) {
    pending_interest.pop();
    // std::cout << "pending interest empty queue!" << std::endl;
    if (vv_update != 0) {
      SendSyncNotify();
    }
    SendDataInterest();
    return;
  }

  auto n = pending_interest.front().front();
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());
    if (n.compare(0, 2, kSyncDataPrefix) == 0) {
      face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      if (left_retx_count != kInterestTransmissionTime) retx_data_interest++;
    }
    else if (n.compare(0, 2, kBundledDataPrefix) == 0) {
      face_.expressInterest(i, std::bind(&Node::OnBundledData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      if (left_retx_count != kInterestTransmissionTime) retx_bundled_interest++; 
    }
    left_retx_count--;
    waiting_data = n;
    wt_data_interest = scheduler_.scheduleEvent(kInterestWT, [this] { SendDataInterest(); });
  });
}

/**
 * Callback for when requested data returns. If I don't have this data, save it
 *  in data_store_. 
 * Cancel timeout event.
 */
void Node::OnRemoteData(const Data& data) {
  const auto& n = data.getName();
  assert(n.compare(waiting_data) == 0);
  assert(data_store_.find(n) == data_store_.end());
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data: name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto node_seq = ExtractSequence(n);
  if (data_store_.find(n) == data_store_.end()) {
    // update the version_vector, data_store_ and recv_window
    data_store_[n] = data.shared_from_this();
    logDataStore(n);
    recv_window[node_id].Insert(node_seq);
    auto last_ack = recv_window[node_id].LastAckedData();
    assert(last_ack != 0);
    if (last_ack != version_vector_[node_id]) {
      vv_update++;
      for (auto seq = version_vector_[node_id] + 1; seq <= last_ack; ++seq) {
        logStateStore(node_id, seq);
      }
    }
    version_vector_[node_id] = last_ack;
  }
  // cancel the wt timer
  scheduler_.cancelEvent(wt_data_interest);
  left_retx_count = kInterestTransmissionTime;
  SendDataInterest();
  
  /* Broadcast the received data for multi-hop */
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, data] {
    face_.put(data);
  });
}

/**
 * Listen for interest for data. If I have data with same name in data_store_, 
 *  reply with this data packet.
 */
void Node::OnDataInterest(const Interest& interest) {
  // no forwarding now!
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Data Interest: i.name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  auto iter = data_store_.find(n);
  if (iter != data_store_.end()) {
    /* If I have this data, send it */
    face_.put(*iter->second);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") sends the data name = " << iter->second->getName());
  } else {
    /* Otherwise add to my PIT, but send probabilistically */
    int p = mhop_dist(rengine_);
    if (p < pMultihopForwardDataInterest) {
      face_.expressInterest(interest, std::bind(&Node::OnRemoteData, this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
    } else {
      face_.addToPit(interest, std::bind(&Node::OnRemoteData, this, _2),
                    [](const Interest&, const lp::Nack&) {},
                    [](const Interest&) {});
    }
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Suppress Interest: i.name=" << n.toUri());
  }
}

/**
 * Callback for when requested bundled data returns. 
 * Cancel timeout event.
 */
void Node::OnBundledData(const Data& data) {
  const auto& n = data.getName();
  assert(n.compare(waiting_data) == 0);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv bundled data: name=" << n.toUri());

  const auto& content = data.getContent();
  proto::PackData pack_data_proto;
  if (!pack_data_proto.ParseFromArray(content.value(), content.value_size())) {
    // VSYNC_LOG_WARN( "Invalid syncNotifyNotify reply content format" );
    return;
  }

  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Bundled Data" );
  for (int i = 0; i < pack_data_proto.entry_size(); ++i) {
    const auto& entry = pack_data_proto.entry(i);
    auto data_name = Name(entry.name());
    if (data_store_.find(data_name) != data_store_.end()) return;
    std::string data_content = entry.content();
    std::shared_ptr<Data> data = std::make_shared<Data>(data_name);
    data->setContent(reinterpret_cast<const uint8_t*>(data_content.data()),
                     data_content.size());
    key_chain_.sign(*data, signingWithSha256());

    data_store_[data_name] = data;
    auto data_nid = ExtractNodeID(data_name);
    auto data_seq = ExtractSequence(data_name);
    recv_window[data_nid].Insert(data_seq);
    logDataStore(data_name);
  }

  // update the version vector
  for (auto entry: recv_window) {
    auto node_id = entry.first;
    auto node_rw = entry.second;
    auto last_ack = node_rw.LastAckedData();
    assert(last_ack != 0);
    if (last_ack != version_vector_[node_id]) {
      vv_update++;
      for (auto seq = version_vector_[node_id] + 1; seq <= last_ack; ++seq) {
        logStateStore(node_id, seq);
      }
    }
    version_vector_[node_id] = last_ack;
  }

  auto recv_nid = ExtractNodeID(n);
  auto next_vv = DecodeVV(pack_data_proto.nextvv());
  auto next_bundle = MakeBundledDataName(recv_nid, EncodeVVToName(next_vv));
  assert(pending_interest.front().size() == 1);
  pending_interest.front().pop();
  if (!next_vv.empty()) {
    std::cout << "node(" << nid_ << ") sends next bundled interest: " << next_bundle.toUri() << std::endl;
    pending_interest.front().push(next_bundle);
  }
  else {
    std::cout << "node(" << nid_ << ") has no next bundled interest" << std::endl;
  }
  // cancel the wt timer
  scheduler_.cancelEvent(wt_data_interest);
  left_retx_count = kInterestTransmissionTime;
  SendDataInterest();
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

void Node::logDataStore(const Name& name) {
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Store New Data: " << name.toUri() << std::endl;
}

void Node::logStateStore(const NodeID& nid, int64_t seq) {
  std::string state_tag = to_string(nid) + "-" + to_string(seq);
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Update New Seq: " << state_tag << std::endl;
}

/*
// bundle
void Node::SendBundledDataInterest(const NodeID& recv_id, VersionVector mv) {
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
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Bundled Data Interest: " << n.toUri() );

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

  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Back Packed Data" );
  const std::string& pack_data = pack_data_proto.SerializeAsString();
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setContent(reinterpret_cast<const uint8_t*>(pack_data.data()),
                   pack_data.size());
  key_chain_.sign(*data);
  face_.put(*data);
}


void Node::OnBundledData(const Data& data) {
  auto n = data.getName();
  if (wt_list.find(n) != wt_list.end()) {
    scheduler_.cancelEvent(wt_list[n]);
    wt_list.erase(n);
  }

  const auto& content = data.getContent();
  proto::PackData pack_data_proto;
  if (!pack_data_proto.ParseFromArray(content.value(), content.value_size())) {
    // VSYNC_LOG_WARN( "Invalid syncNotifyNotify reply content format" );
    return;
  }
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Bundled Data" );
  for (int i = 0; i < pack_data_proto.entry_size(); ++i) {
    const auto& entry = pack_data_proto.entry(i);
    auto data_name = Name(entry.name());
    if (data_store_.find(data_name) != data_store_.end()) return;
    std::string data_content = entry.content();
    std::shared_ptr<Data> data = std::make_shared<Data>(data_name);
    data->setContent(reinterpret_cast<const uint8_t*>(data_content.data()),
                     data_content.size());
    key_chain_.sign(*data, signingWithSha256());

    data_store_[data_name] = data;

    auto data_nid = ExtractNodeID(data_name);
    auto data_seq = ExtractSequence(data_name);
    recv_window[data_nid].Insert(data_seq);
    if (wt_list.find(data_name) != wt_list.end()) {
      scheduler_.cancelEvent(wt_list[data_name]);
      wt_list.erase(data_name);
    }
    if (pending_interest.find(data_name) != pending_interest.end()) pending_interest.erase(data_name);
    logDataStore(data_name);
  }

  auto recv_nid = ExtractNodeID(n);
  auto next_vv = DecodeVV(pack_data_proto.nextvv());
  SendBundledDataInterest(recv_nid, next_vv);
}
*/

// print the vector clock every 5 seconds
/*
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  vv_snapshots.push_back(version_vector_);
  rw_snapshots.push_back(recv_window);
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
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
  // VSYNC_LOG_TRACE( "(node" << nid_ << ") pending interest list = :\n" + pending_list);
  if (in_dt == false) {
    in_dt = true;
    SendDataInterest();
  }
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

  // choose an interest to send out. BundledInterest has highest priority, then SyncNotify
  if (!pending_bundled_interest.empty()) {
    auto n = pending_bundled_interest.begin()->first;
    auto cur_transmission_time = pending_bundled_interest.begin()->second;
    assert(cur_transmission_time != 0);
    if (cur_transmission_time != kInterestTransmissionTime) retx_bundled_interest++;

    Interest i(n, kSendOutInterestLifetime);
    // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send BundledDataInterest: i.name=" << n.getPrefix(4).toUri());
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
    // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(5).toUri());
    face_.expressInterest(i, std::bind(&Node::onNotifyACK, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});

    if (notify_time != kSyncNotifyMax) retx_notify_interest++;
    waiting_sync_notify = n;
    scheduler_.cancelEvent(wt_notify);
    wt_notify = scheduler_.scheduleEvent(kInterestWT, [this, n] { OnWTTimeout(n, notify_time - 1); });
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
    // VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());
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
    // VSYNC_LOG_TRACE ("node(" << nid_ << ") WT time out for syncNotify: " << name.toUri());
    if (pending_sync_notify.compare(Name("/")) == 0) {
      // VSYNC_LOG_TRACE ("node(" << nid_ << ") set up the retx syncNotify");
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
*/
  
}  // namespace vsync
}  // namespace ndn
