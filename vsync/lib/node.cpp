#include <random>

#undef NDEBUG

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

#include "ns3/simulator.h"
#include "ns3/nstime.h"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

/* Constants */
/* No. of times same data interest will be sent */
static const int kInterestTransmissionTime = 1;
/* Lifetime */
static const time::milliseconds kSendOutInterestLifetime = time::milliseconds(50);
static const time::seconds kBeaconLifetime = time::seconds(6);
static const time::milliseconds kAddToPitInterestLifetime = time::milliseconds(444);
/* Timeout for sync interests */
// static const time::milliseconds kInterestWT = time::milliseconds(50);
static const time::milliseconds kInterestWT = time::milliseconds(200);
/* Distributions for multi-hop */
std::uniform_int_distribution<> mhop_dist(0, 10000);
static const int pMultihopForwardDataInterest = 5000;
/* Distribution for data generation */
static const int data_generation_rate_mean = 40000;
std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
/* Threshold for bundled data fetching */
static const int kMissingDataThreshold = 0x7fffffff;
/* MTU */
static const int kMaxDataContent = 4000;


/* Delay timers */
/* Delay for sending everything to avoid collision */
std::uniform_int_distribution<> dt_dist(0, 5000);
/* Delay for sending ACK when local vector is not newer */
std::uniform_int_distribution<> ack_dist(5000, 10000);
/* Delay for sync interest retx */
// static time::seconds kRetxTimer = time::seconds(2);
std::uniform_int_distribution<> retx_dist(2000000, 10000000);
/* Delay for beacon frequency */
std::uniform_int_distribution<> beacon_dist(2000000, 3000000);


/* Options */
const static bool kBeacon =   false;  /* Use beacon? */
/*const*/ static bool kRetx =     true;   /* Use sync interest retx? */
const static bool kMultihopSync = true;  /* Use multihop for sync? */
const static bool kMultihopData = true;   /* Use multihop for data? */ 
const static bool kSyncAckSuppression = false;


/* Public */
Node::Node(Face &face, Scheduler &scheduler, KeyChain &key_chain, const NodeID &nid,
           const Name &prefix, DataCb on_data) : 
           face_(face), scheduler_(scheduler),key_chain_(key_chain), nid_(nid),
           prefix_(prefix), data_cb_(std::move(on_data)), 
           rengine_(rdevice_()) 
          // rengine_(0)
  {
  
  /* Initialize statistics */
  // data_num = 0;
  retx_sync_interest = 0;
  retx_data_interest = 0;
  retx_bundled_interest = 0;

  /* Set interest filters */
  face_.setInterestFilter(
    Name(kSyncNotifyPrefix), std::bind(&Node::OnSyncInterest, this, _2),
    [this](const Name&, const std::string& reason) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register sync prefix: " << reason); 
      throw Error("Failed to register sync interest prefix: " + reason);
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
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register bundled data prefix: " << reason); 
      throw Error("Failed to register bundled data prefix: " + reason);
  }); 
  face_.setInterestFilter(
    Name(kBeaconPrefix), std::bind(&Node::OnBeacon, this, _2),
    [this](const Name&, const std::string& reason) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Failed to register beacon prefix: " << reason); 
      throw Error("Failed to register beacon prefix: " + reason);
  });

  /* Initiate node states */
  generate_data = true;     
  version_vector_[nid_] = 0;
  
  /* Initiate event scheduling */
  /* 2s: Start simulation */
  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });

  /* 400s: Stop data generation */
  scheduler_.scheduleEvent(time::seconds(400), [this] {
    generate_data = false;
  });

  /* 1195s: Print NFD statistics */
  scheduler_.scheduleEvent(time::seconds(1195), [this] {
    // std::cout << "node(" << nid_ << ") outInterest = " << out_interest_num << std::endl;
    // std::cout << "node(" << nid_ << ") average time to meet a new node = " << total_time / (double)count << std::endl;
    std::cout << "node(" << nid_ << ") retx_sync_interest = " << retx_sync_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_data_interest = " << retx_data_interest << std::endl;
    std::cout << "node(" << nid_ << ") retx_bundled_interest = " << retx_bundled_interest << std::endl;
    // TODO: Remove
    int remaining = 0;
    while(!pending_interest.empty()) {
      while(!pending_interest.front().empty()) {
        ++remaining;
        pending_interest.front().pop();
      }
      pending_interest.pop();
    }
    std::cout << "node(" << nid_ << ") remaining pending interest = " << remaining << std::endl;

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
  version_vector_[nid_]++;
  // data_num++;

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
  
  if (kRetx) {
    int delay = retx_dist(rengine_);
    retx_event = scheduler_.scheduleEvent(time::microseconds(delay), [this] { 
      RetxSyncInterest(); 
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Retx sync interest" );
    });
  }
  if (kBeacon) {
    int next_beacon = beacon_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(next_beacon), 
                             [this] { SendBeacon(); });
  }
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
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this] {
    std::string encoded_vv = EncodeVVToName(version_vector_);
    // Don't know why append time stamp, but easier to keep it than remove
    auto cur_time = ns3::Simulator::Now().GetMicroSeconds();
    auto pending_sync_notify = MakeSyncNotifyName(nid_, encoded_vv, cur_time);

    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send sync interest: i.name=" << pending_sync_notify.toUri());
    face_.expressInterest(pending_sync_notify, std::bind(&Node::OnSyncAck, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
  });
}

void Node::OnSyncInterest(const Interest &interest) {
  const auto& n = interest.getName();
  NodeID node_id = ExtractNodeID(n);
  auto other_vv = DecodeVVFromName(ExtractEncodedVV(n));

  VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest:" << n.toUri() );

  /* Merge state vector, add missing data to pending_interests */
  bool otherVectorNew = false;
  std::queue<Name> q;
  int missing_data = 0;
  VersionVector mv;     /* For encoding bundled interest */
  for (auto entry : other_vv) {
    auto node_id = entry.first;
    auto seq_other = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || 
        version_vector_[node_id] < seq_other) {
      otherVectorNew = true;
      auto start_seq = version_vector_.find(node_id) == version_vector_.end() ? 
                       1: version_vector_[node_id] + 1;
      mv[node_id] = start_seq;
      for (auto seq = start_seq; seq <= seq_other; ++seq) {
        logStateStore(node_id, seq);
        auto n = MakeDataName(node_id, seq);
        missing_data++;
        q.push(n);
      }
      version_vector_[node_id] = seq_other;  // Will be set later
    }
  }
  if (missing_data > kMissingDataThreshold) {
    auto n = MakeBundledDataName(node_id, EncodeVVToName(mv));
    std::queue<Name> bundle_queue;
    bundle_queue.push(n);
    pending_interest.push(bundle_queue);
    // TODO: Should bundled data be same as non-bundled?
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  } else if (missing_data > 0) {
    pending_interest.push(q);
    if (pending_interest.size() == 1) {
      left_retx_count = kInterestTransmissionTime;
      SendDataInterest();
    }
  }

  /* If incoming state not newer, reset timer to delay sending next sync interest */
  if (!otherVectorNew) {
    scheduler_.cancelEvent(retx_event);
    int delay = retx_dist(rengine_);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Reset sync interest retx timer" );
    retx_event = scheduler_.scheduleEvent(time::microseconds(delay), [this] { 
      RetxSyncInterest(); 
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Retx sync interest" );
    });
  }

  /* Do I have newer state? */
  bool myVectorNew = false;
  for (auto entry: version_vector_) {
    auto node_id = entry.first;
    auto seq = entry.second;
    if (other_vv.find(node_id) == other_vv.end() || 
        other_vv[node_id] < seq) {
      myVectorNew = true;
      break;
    }
  }

  /* Send ACK based on whether I have new state */
  if (myVectorNew) {
    /* If local vector contains newer state, send ACK immediately */
    int delay = dt_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(delay), [this, n] { 
      VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK immediately:" << n.toUri() );
      SendSyncAck(n); 
    });
  } else {
    if (kSyncAckSuppression) {
      if (overheard_sync_interest.find(n) != overheard_sync_interest.end()) {
        return;
      }

      int delay = dt_dist(rengine_);
      // scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
        Interest interest_overhear(n, kAddToPitInterestLifetime);
        face_.expressInterest(interest_overhear, std::bind(&Node::OnSyncAck, this, _2),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
      // });

      delay += ack_dist(rengine_);
      overheard_sync_interest[n] = scheduler_.scheduleEvent(
        time::microseconds(delay), [this, n] {
          /* Remove entry and send ack */
          overheard_sync_interest.erase(n);
          VSYNC_LOG_TRACE ("node(" << nid_ << ") finally reply ACK with delay: " << n.toUri() );
          SendSyncAck(n);
        }
      );
      VSYNC_LOG_TRACE ("node(" << nid_ << ") starting overhear sync interest: " << n.toUri() );
    } else {
      int delay = ack_dist(rengine_);
      scheduler_.scheduleEvent(time::microseconds(delay), [this, n] { 
        VSYNC_LOG_TRACE ("node(" << nid_ << ") reply ACK immediately:" << n.toUri() );
        SendSyncAck(n); 
      });
    }
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

  VSYNC_LOG_TRACE ("node(" << nid_ << ") replying ACK:" << n.toUri() );
  face_.put(*ack);
}

void Node::OnSyncAck(const Data &ack) {
  const auto& n = ack.getName();

  if (kSyncAckSuppression && 
      overheard_sync_interest.find(n) != overheard_sync_interest.end()) {
    scheduler_.cancelEvent(overheard_sync_interest[n]);
    overheard_sync_interest.erase(n);
    VSYNC_LOG_TRACE ("node(" << nid_ << ") Overhear sync ack, suppress: " << ack.getName().toUri());
    return;
  } else {
    VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV sync ack: " << ack.getName().toUri());
  }

  /* Extract difference and add to pending_interest */
  const auto& content = ack.getContent();
  proto::AckContent content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data AckContent format: nid=" << nid_);
    assert(false);
  }
  auto vector_other = DecodeVV(content_proto.vv());
  bool otherVectorNew = false;    /* Set if remote vector has newer state */
  for (auto entry : vector_other) {
    std::queue<Name> q;
    auto node_id = entry.first;
    auto node_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || 
        version_vector_[node_id] < node_seq) {
      otherVectorNew = true;
      auto start_seq = version_vector_.find(node_id) == version_vector_.end() ? 
                       1: version_vector_[node_id] + 1;
      for (auto seq = start_seq; seq <= node_seq; ++seq) {
        logStateStore(node_id, seq);
        auto n = MakeDataName(node_id, seq);
        q.push(n);
      }
      version_vector_[node_id] = node_seq;
      pending_interest.push(q);
      if (pending_interest.size() == 1) {
        left_retx_count = kInterestTransmissionTime;
        SendDataInterest();
      }
    }
  }

  /**
   * If remote vector has newer state, send another sync interest immediately,
   *  because there may be sync reply got stuck in the network due to 
   *  "one-interest-one-reply policy".
   */
  //  if (otherVectorNew) {
  //    VSYNC_LOG_TRACE( "node(" << nid_ << ") Send another sync interest immediately");
  //    SendSyncInterest();
  //  }
}


/* 2. Data packet processing */
void Node::SendDataInterest() {
  /* Stop scheduling itself if empty. Will be rescheduled by OnSyncAck() */
  if (pending_interest.empty()) {
    return;
  }

  /* Drop non-responding data interest */
  if (left_retx_count == 0) {
    /* Remove entire queue, because remaining data are also unlikely to receive reply */
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Drop data interest");
    pending_interest.push(pending_interest.front());
    pending_interest.pop();
    if (pending_interest.empty()) {
      /* Stop scheduling itself */
      return;
    }
    /* Reset counter for the next data interest */
    left_retx_count = kInterestTransmissionTime;
    return SendDataInterest();  /* Recursion */
  }

  /* Remove falsy pending interests for non-bundled interest */
  if (!pending_interest.front().empty() && 
      pending_interest.front().front().compare(0, 2, kSyncDataPrefix) == 0) {
    while (!pending_interest.front().empty()) {
      if (data_store_.find(pending_interest.front().front()) != data_store_.end()) {
        pending_interest.front().pop();
      } else {
        break;
      }
    }
  } 
  if (pending_interest.front().empty()) {
    pending_interest.pop();
    left_retx_count = kInterestTransmissionTime;
    return SendDataInterest();  /* Recursion */
  }

  /* Send first data interest in the first queue */
  auto n = pending_interest.front().front();
  assert(data_store_.find(n) == data_store_.end());
  int delay = dt_dist(rengine_);
  scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
    Interest i(n, kSendOutInterestLifetime);
    if (n.compare(0, 2, kSyncDataPrefix) == 0) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Send data interest: i.name=" << n.getPrefix(4).toUri());
      face_.expressInterest(i, std::bind(&Node::OnDataReply, this, _2, kOriginal),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      if (left_retx_count != kInterestTransmissionTime) {
        retx_data_interest++;
      }
    }
    else if (n.compare(0, 2, kBundledDataPrefix) == 0) {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Send bundled data interest: i.name=" << n.getPrefix(4).toUri());
      face_.expressInterest(i, std::bind(&Node::OnBundledDataReply, this, _2),
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
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data interest: i.name=" << n.toUri());

  auto iter = data_store_.find(n);
  if (iter != data_store_.end()) {
    /* If I have this data, send it */
    int delay = dt_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(delay), [this, iter] {
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Send data = " << iter->second->getName());
      face_.put(*iter->second);
    });
  } else if (kMultihopData) {
    /* Otherwise add to my PIT, but send probabilistically */
    int p = mhop_dist(rengine_);
    if (p < pMultihopForwardDataInterest) {
      int delay = dt_dist(rengine_);
      scheduler_.scheduleEvent(time::microseconds(delay), [this, interest, n] {  
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Forward data interest: i.name=" << n.toUri());
        Interest interest_forward(n, kSendOutInterestLifetime);
        face_.expressInterest(interest_forward, std::bind(&Node::OnDataReply, this, _2, kForwarded),
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
      });
    } else {
      int delay = dt_dist(rengine_);
      // scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Suppress data interest: i.name=" << n.toUri());
        Interest interest_suppress(n, kAddToPitInterestLifetime);
        face_.expressInterest(interest_suppress, std::bind(&Node::OnDataReply, this, _2, kSuppressed),
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      // });
    }
  }
}

void Node::SendDataReply() {
  /* Nothing */
  /* See Node::OnDataInterest() */
}

void Node::OnDataReply(const Data &data, SourceType sourceType) {
  const auto& n = data.getName();
  // assert(n.compare(waiting_data) == 0);              // TODO: no longer true
  // assert(data_store_.find(n) == data_store_.end());  // TODO: no longer true
  if (data_store_.find(n) != data_store_.end()) {
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Drops duplicate data: name=" << n.toUri());
    return;
  }

  /* Print based on source */
  if (sourceType == kOriginal) {
    /* Only 1 outstanding interest from pending_interest queue */
    // assert(n.compare(waiting_data) == 0);
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data reply: name=" << n.toUri());
  } else if (sourceType == kForwarded) {
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv forwarded data reply: name=" << n.toUri());
  } else if (sourceType == kSuppressed) {
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv suppressed data reply: name=" << n.toUri());
  } else {
    assert(0);
  }

  /* Save data */
  data_store_[n] = data.shared_from_this();
  logDataStore(n);

  // /* Trigger sending data interest */  // TODO: What's this?
  // scheduler_.cancelEvent(wt_data_interest);
  // left_retx_count = kInterestTransmissionTime;
  // SendDataInterest();

  /* Broadcast the received data for multi-hop */
  if (kMultihopData) {
    int delay = dt_dist(rengine_);
    scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
      face_.put(*data_store_[n]);
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Re-broadcast data: name=" << n.toUri());
    });
  }
}



/* 3. Bundled data packet processing */
void Node::SendBundledDataInterest() {
  /* Do nothing */
  /* See Node::SendDataInterest() */
}

void Node::OnBundledDataInterest(const Interest &interest) {
  auto n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv bundled data interest: " << n.toUri() );

  auto missing_data = DecodeVVFromName(ExtractEncodedMV(n));
  proto::PackData pack_data_proto;
  VersionVector next_vv = missing_data;
  for (auto item: missing_data) {
    auto node_id = item.first;
    auto start_seq = item.second;
    bool exceed_max_size = false;   /* MTU */

    /* Because bundled interest/data are unicast */
    assert(version_vector_.find(node_id) != version_vector_.end());

    for (auto seq = start_seq; seq <= version_vector_[node_id]; ++seq) {
      Name data_name = MakeDataName(node_id, seq);
      if (data_store_.find(data_name) == data_store_.end()) {
        continue;
      }
      auto* entry = pack_data_proto.add_entry();
      entry->set_name(data_name.toUri());
      entry->set_content(data_store_[data_name]->getContent().value(), data_store_[data_name]->getContent().value_size());

      int cur_data_content_size = pack_data_proto.SerializeAsString().size();
      if (cur_data_content_size >= kMaxDataContent) {
        exceed_max_size = true;
        if (seq == version_vector_[node_id]) {
          next_vv.erase(node_id);
        } else {
          next_vv[node_id] = seq + 1;
        }
        break;
      }
    }

    /* Send data up to MTU, discard rest of the interest */
    if (exceed_max_size) {
      break;
    } else {
      next_vv.erase(node_id); 
    }
  }

  /* Encode remaining interest as tag */
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Send Back Packed Data" );
  EncodeVV(next_vv, pack_data_proto.mutable_nextvv());
  const std::string& pack_data = pack_data_proto.SerializeAsString();
  std::shared_ptr<Data> data = std::make_shared<Data>(n);
  data->setContent(reinterpret_cast<const uint8_t*>(pack_data.data()),
                   pack_data.size());
  key_chain_.sign(*data);
  face_.put(*data); // TODO: Add delay?
}

void Node::SendBundledDataReply() {
  /* Do nothing */
  /* See Node::SendDataInterest() */
}

void Node::OnBundledDataReply(const Data &data) {
  const auto& n = data.getName();
  assert(n.compare(waiting_data) == 0);
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv bundled data: name=" << n.toUri());

  const auto& content = data.getContent();
  proto::PackData pack_data_proto;
  if (!pack_data_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN( "Invalid syncNotifyNotify reply content format" );
    return;
  }

  for (int i = 0; i < pack_data_proto.entry_size(); ++i) {
    const auto& entry = pack_data_proto.entry(i);
    auto data_name = Name(entry.name());

    if (data_store_.find(data_name) != data_store_.end()) {
      continue;
    }

    std::string data_content = entry.content();
    std::shared_ptr<Data> data = std::make_shared<Data>(data_name);
    data->setContent(reinterpret_cast<const uint8_t*>(data_content.data()),
                     data_content.size());
    key_chain_.sign(*data, signingWithSha256());
    data_store_[data_name] = data;
    logDataStore(data_name);
  }

  /* If next_vv tag not empty, send another bundled interest */
  auto recv_nid = ExtractNodeID(n);
  auto next_vv = DecodeVV(pack_data_proto.nextvv());
  auto next_bundle = MakeBundledDataName(recv_nid, EncodeVVToName(next_vv));
  assert(pending_interest.front().size() == 1);   /* Is bundled interest */
  pending_interest.front().pop();
  if (!next_vv.empty()) {
    std::cout << "node(" << nid_ << ") sends next bundled interest: " 
              << next_bundle.toUri() << std::endl;
    pending_interest.front().push(next_bundle);
  } else {
    std::cout << "node(" << nid_ << ") has no next bundled interest" << std::endl;
  }

  // /* Re-schedule */
  // scheduler_.cancelEvent(wt_data_interest);
  // left_retx_count = kInterestTransmissionTime;
  // SendDataInterest();
}


/* 4. Pro-active events (beacons and sync interest retx) */
void Node::RetxSyncInterest() {
  SendSyncInterest();
  int delay = retx_dist(rengine_);
  retx_event = scheduler_.scheduleEvent(time::microseconds(delay), [this] { 
    RetxSyncInterest(); 
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Retx sync interest" );
  });
}

void Node::SendBeacon() {
  /* Notify presence of myself */
  auto n = MakeBeaconName(nid_);
  Interest i(n, time::milliseconds(1));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  int next_beacon = beacon_dist(rengine_);
  beacon_event = scheduler_.scheduleEvent(time::microseconds(next_beacon),
                                          [this] { SendBeacon(); });
}

void Node::OnBeacon(const Interest &beacon) {
  auto n = beacon.getName();
  auto node_id = ExtractNodeID(n);

  if (one_hop.find(node_id) == one_hop.end()) {
    /* New node enter one-hop distance,  */
    std::string one_hop_list = to_string(node_id);
    for (auto entry : one_hop) {
      one_hop_list += ", " + to_string(entry.first);
    }
    VSYNC_LOG_TRACE ("node(" << nid_ << ") detect a new one-hop node: " 
                     << node_id << ", the current one-hop list: " << one_hop_list);
    SendSyncInterest();

    /* Update one-hop info */
    scheduler_.cancelEvent(one_hop[node_id]);
    one_hop[node_id] = scheduler_.scheduleEvent(kBeaconLifetime, [this, node_id] {
      one_hop.erase(node_id);
    });
  }
}

} // namespace vsync
} // namespace ndn