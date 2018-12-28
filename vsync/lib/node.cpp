#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

#include "ns3/simulator.h"
#include "ns3/nstime.h"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

/* Constants */
/* No. of times same sync interest will be sent */
static int kSyncInterestMax = 3;
/* No. of times same data interest will be sent */
static int kInterestTransmissionTime = 3;
/* Lifetime of sync interests */
static time::milliseconds kSendOutInterestLifetime = time::milliseconds(100);
/* Timeout for sync interests */
static time::milliseconds kInterestWT = time::milliseconds(100);
/* Distributions for multi-hop */
std::uniform_int_distribution<> mhop_dist(0, 10000);
static int pMultihopForwardSyncInterest1 = 3000;
static int pMultihopForwardSyncInterest2 = 7000;
static int pMultihopForwardDataInterest = 5000;


/* RNGs */
/* Delay for sending everything to avoid collision */
std::uniform_int_distribution<> dt_dist(0, 5000);
/* Delay for sending ACK when local vector is not newer */
std::uniform_int_distribution<> ack_dist(50000, 100000);


/* Public */
Node::Node(Face &face, Scheduler &scheduler, Keychain &key_chain, const NodeID &nid,
           const Name &prefix, DataCb on_data) : 
           face_(face), scheduler_(scheduler),key_chain_(key_chain), nid_(nid),
           prefix_(prefix), data_cb_(std::move(on_data)), rengine_(rdevice_()) {
  
  /* Initialize statistics */
  data_num = 0;
  retx_sync_interest = 0;
  retx_data_interest = 0;
  retx_bundled_interest = 0;

  /* Set interest filters */
  face_.setInterestFilter(
    Name(kSyncNotifyPrefix), std::bind(&Node::OnSyncInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register syncNotify prefix: " << reason); 
      throw Error("Failed to register syncNotify prefix: " + reason);
  });
  face_.setInterestFilter(
    Name(kSyncDataPrefix), std::bind(&Node::onDataInterest, this, _2),
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

  /* Initiate node states */
  generate_data = true;     
  pending_sync_notify = Name("/");
  waiting_sync_notify = Name("/");
  vv_updated = true;
  version_vector_[nid_] = 0;

  /* Initiate event scheduling */
  /* 2s: Start simulation */
  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });

  /* 400s: Stop data generation */

  /* 1195s: Print NFD statistics */
  scheduler_.scheduleEvent(time::seconds(1195), [this] {
    // std::cout << "node(" << nid_ << ") outInterest = " << out_interest_num << std::endl;
    // std::cout << "node(" << nid_ << ") average time to meet a new node = " << total_time / (double)count << std::endl;
    std::cout << "node(" << nid_ << ") retx_sync_interest = " << retx_sync_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_data_interest = " << retx_data_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_bundled_interest = " << retx_bundled_interest << std::endl;
    PrintNDNTraffic();
  });

  /* 1196s: Print Node statistics */
  scheduler_.scheduleEvent(time::seconds(1196), [this] {
    uint64_t seq_sum = 0;
    for (auto entry: version_vector_) {
      seq_sum += entry.second;
    }
    std::cout << "node(" << nid_ << ") seq sum: " << seq_sum << std::endl;
    // std::cout << "node(" << nid_ << ") seq = " << version_vector_[nid_] << std::endl;
  });
}

void Node::PublishData(const std::string& content, uint32_t type) {
  if (!generate_data) {
    return;
  }
  /* Make data name */
  auto n = MakeDataName(nid_, version_vector_[nid_]);
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setFreshnessPeriod(time::seconds(3600));
  /* Set data content */
  proto::Content content_proto;
  EncodeVV(version_vector_, content_proto.mutable_vv());
  content_proto.set_content(content);
  const std::string& content_proto_str = content_proto.SerializeAsString();
  data -> setContent(reinterpret_cast<const uint8_t*>(content_proto_str.data()),
                   content_proto_str.size());
  data -> setContentType(type);
  key_chain_.sign(*data, signingWithSha256());

  data_store_[n] = data;

  /* Print that both state and data have been stored */
  logDataStore(n);
  logStateStore(nid_, version_vector_[nid_]);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Publish Data: d.name=" << n.toUri() );

  /* Schedule next publish with same data */
  if (generate_data) {
    scheduler_.scheduleEvent(time::milliseconds(data_generation_dist(rengine_)),
                             [this, content] { PublishData(content); });
  } else {
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Stopped data generation");
  }

  /* Send sync interest. No delay because callee already delayed randomly */
  SendSyncInterest();
}


/* Helper functions */
void Node::StartSimulation() {
  /* Init the first data publishing */
  std::string content = std::string(100, '*');
  scheduler_.scheduleEvent(time::milliseconds(10 * nid_),
                           [this, content] { PublishData(content); });
}

void Node::PrintNDNTraffic() {
  /* Send a packet to trigger NFD to print */
  Interest i(kGetNDNTraffic, time::milliseconds(5));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
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


/* Packet processing pipeline */
/* 1. Sync packet processing */
void Node::SendSyncInterest() {
  std::string encoded_vv = EncodeVVToName(version_vector_);
  auto cur_time = ns3::Simulator::Now().GetMicroSeconds();
  // Don't know why append time stamp, but easier to keep it than remove
  auto sync_notify_interest_name = MakeSyncNotifyName(nid_, encoded_vv, cur_time);

  pending_sync_notify = sync_notify_interest_name;
  notify_time = kSyncInterestMax;     /* Reset retx counter */
  vv_updated = false;                 
  
  scheduler_.cancelEvent(dt_notify);
  scheduler_.cancelEvent(wt_notify);
  int delay = dt_dist(rengine_);
  dt_notify = scheduler_.scheduleEvent(time::microseconds(delay), 
                                       [this] { OnNotifyDTTimeout(); });
}

void Node::OnSyncInterest(const Interest &interest) {
  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest" );
  const auto& n = interest.getName();
  NodeID node_id = ExtractNodeID(n);
  auto other_vv = DecodeVVFromName(ExtractEncodedVV(n));

  /* Prepare ACK packet */
  ack = std::make_shared<Data>(n);
  VersionVector difference;
  for (auto entry: version_vector_) {
    auto node_id = entry.first;
    auto seq = entry.second;
    if (other_vv.find(node_id) == other_vv.end() || other_vv[node_id] < seq) {
      difference[node_id] = seq;
    }
  }
  // proto::AckContent content_proto;
  // EncodeVV(difference, content_proto.mutable_vv());
  // const std::string& content_proto_str = content_proto.SerializeAsString();
  // ack->setContent(reinterpret_cast<const uint8_t*>(content_proto_str.data()),
  //                 content_proto_str.size());
  // key_chain_.sign(*ack, signingWithSha256());

  /* Send ACK */
  if (!difference.empty()) {
    /* If local vector contains newer state, send ACK immediately */
    int delay = dt_dist(rengine_);
    // scheduler_.scheduleEvent(time::microseconds(delay), 
    //                          [this] { face_.put(*ack); });
    scheduler_.scheduleEvent(time::microseconds(delay),
                             [this, n] { SendSyncAck(n); });
    VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK immediately" );
  } else {
    /* If local vector outdated or equal, send ACK after some delay */
    int delay = ack_dist(rengine_);
    // scheduler_.scheduleEvent(time::microseconds(delay), 
    //                          [this] { face_.put(*ack); });
    scheduler_.scheduleEvent(time::microseconds(delay),
                             [this, n] { SendSyncAck(n); });
    VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK with delay" );
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
}

/* Append vector to name just before sending out ACK for freshness */
void Node::SendSyncAck(const Name &n) {
  std::shared_ptr<Data> ack = std::make_shared<Data>(n);
  proto::AckContent content_proto;
  EncodeVV(version_vector_, content_proto.mutable_vv());
  const std::string& content_proto_str = content_proto.SerializeAsString();
  ack->setContent(reinterpret_cast<const uint8_t*>(content_proto_str.data()),
                  content_proto_str.size());
  key_chain_.sign(*ack, signingWithSha256());
  
  face_.put(*ack);
}

void Node::onSyncAck(const Data &data) {
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

  /* Extract difference */
  const auto& content = ack.getContent();
  proto::AckContent content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data AckContent format: nid=" << nid_);
    assert(false);
  }
  auto vector_other = DecodeVV(content_proto.vv());
  std::queue<Name> q;
  for (auto entry : vector_other) {
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

void Node::OnNotifyDTTimeout() {
  Interest i(pending_sync_notify, kSendOutInterestLifetime);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Notify Interest: i.name=" << pending_sync_notify.toUri());
  face_.expressInterest(i, std::bind(&Node::onSyncAck, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  
  if (notify_time < kSyncNotifyMax) { 
    /* This is a retx (scheduled by Node::OnNotifyWTTimeout()) */
    retx_sync_interest++;
  }
  waiting_sync_notify = pending_sync_notify;
  scheduler_.cancelEvent(wt_notify);
  wt_notify = scheduler_.scheduleEvent(kInterestWT, 
                                       [this] { OnNotifyWTTimeout(); });
}

void Node::OnNotifyWTTimeout() {
  waiting_sync_notify = Name("/");
  notify_time--;
  if (notify_time == 0) {
    /* Stop retx attempt for this sync interest */
    return;
  }
  /* Schedule next retx with random delay */
  int delay = dt_dist(rengine_);
  dt_notify = scheduler_.scheduleEvent(time::microseconds(delay), 
                                       [this] { OnNotifyDTTimeout(); });
}


/* 2. Data packet processing */
void Node::SendDataInterest() {
  /* Stop scheduling itself if empty. Will be rescheduled by onSyncAck() */
  if (pending_interest.empty()) {
    return;
  }

  /* Drop non-responding data interest */
  if (left_retx_count == 0) {
    /* Remove entire queue, because remaining data are also unlikely to receive reply */
    pending_interest.pop();
    if (pending_interest.empty()) {
      /* Stop scheduling itself */
      return;
    }
    /* Reset counter for the next data interest */
    left_retx_count = kInterestTransmissionTime;
    if (vv_update) {
      SendSyncInterest();
    }
    return SendDataInterest();  /* Recursion */
  }

  /* Remove falsy pending interests */
  if (!pending_interest.front().empty() && pending_interest.front().front().compare(0, 2, kSyncDataPrefix) == 0) {
    while (!pending_interest.front().empty()) {
      if (data_store_.find(pending_interest.front().front()) != data_store_.end()) {
        pending_interest.front().pop();
      } else {
        break;
      }
    }
  }
  if (pending_interest.front.empty()) {
    pending_interest.pop();
    if (vv_update) {
      SendSyncNotify();
    }
    return SendDataInterest();  /* Recursion */
  }

  /* Send first data interest in the first queue */
  auto n = pending_interest.front().front();
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
    Interest i(n, kSendOutInterestLifetime);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Interest: i.name=" << n.getPrefix(4).toUri());
    if (n.compare(0, 2, kSyncDataPrefix) == 0) {
      face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      if (left_retx_count != kInterestTransmissionTime) {
        retx_data_interest++;
      }
    }
    else if (n.compare(0, 2, kBundledDataPrefix) == 0) {
      face_.expressInterest(i, std::bind(&Node::OnBundledData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      if (left_retx_count != kInterestTransmissionTime) {
        retx_bundled_interest++; 
      }
    }
    left_retx_count--;
    waiting_data = n;
    wt_data_interest = scheduler_.scheduleEvent(kInterestWT, [this] { 
      SendDataInterest(); 
    });
  });
}

void Node::OnDataInterest(const Interest &interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv Data Interest: i.name=" << n.toUri());

  auto node_id = ExtractNodeID(n);
  auto seq = ExtractSequence(n);

  auto iter = data_store_.find(n);
  if (iter != data_store_.end()) {
    /* If I have this data, send it */
    VSYNC_LOG_TRACE( "node(" << nid_ << ") sends the data name = " << iter->second->getName());
    int delay = dt_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(delay), [this, iter] {  /* data_store_ entires are immutable */
      face_.put(*iter->second);
    });
  } else {
    /* Otherwise add to my PIT, but send probabilistically */
    int p = mhop_dist(rengine_);
    if (p < pMultihopForwardDataInterest) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Forward Interest: i.name=" << n.toUri());
      int delay = dt_dist(rengine_);
      scheduler_.scheduleEvent(time::microseconds(delay), [this, interest] {  
        face_.expressInterest(interest, std::bind(&Node::OnRemoteData, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
      });
    } else {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Suppress Interest: i.name=" << n.toUri());
      Interest interest_suppress(interest);
      interest_suppress.setInterestLifetime(kAddToPitInterestLifetime);
      /* No need to add delay timer because data wasn't actually sent */
      face_.expressInterest(interest, std::bind(&Node::OnRemoteData, this, _2),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
    }
  }
}

void Node::SendDataReply() {
  /* Nothing */
}

void Node::onDataReply(const Data &data) {
  const auto& n = data.getName();
  // assert(n.compare(waiting_data) == 0); // TODO
  assert(data_store_.find(n) == data_store_.end());
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data: name=" << n.toUri());
  auto node_id = ExtractNodeID(n);
  auto node_seq = ExtractSequence(n);

  /* Save data */
  data_store_[n] = data.shared_from_this();
  logDataStore(n);
  recv_window[node_id].Insert(node_seq);
  
  /* Inferred state store: Update vector based on data reply instead of sync ack */
  auto last_ack = recv_window[node_id].LastAckedData();
  assert(last_ack != 0);
  if (last_ack != version_vector_[node_id]) {
    vv_update++;
    for (auto seq = version_vector_[node_id] + 1; seq <= last_ack; ++seq) {
      logStateStore(node_id, seq);
    }
  }
  version_vector_[node_id] = last_ack;

  /* Trigger sending data interest */
  scheduler_.cancelEvent(wt_data_interest);
  left_retx_count = kInterestTransmissionTime;
  SendDataInterest();

  /* Broadcast the received data for multi-hop */
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, data] {
    face_.put(data);
  });
}


/* 3. Bundled data packet processing */
void Node::SendBundledDataInterest() {

}

void Node::OnBundledDataInterest(const Interest &interest) {

}

void Node::SendBundledDataReply() {

}

void Node::onBundledDataReply(const Data &data) {

}


/* 4. Beacons */
void Node::SendBeacon() {

}

void Node::onBeacon(const Interest &beacon) {

}


} // namespace vsync
} // namespace ndn