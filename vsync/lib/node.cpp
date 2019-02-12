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


/* Public */
Node::Node(Face &face, Scheduler &scheduler, KeyChain &key_chain, const NodeID &nid,
           const Name &prefix, DataCb on_data, GetCurrentPos getCurrentPos,
           GetCurrentPit getCurrentPit, GetNumSorroundingNodes getNumSorroundingNodes,
           GetFaceById getFaceById) : 
  face_(face), scheduler_(scheduler),key_chain_(key_chain), nid_(nid),
  prefix_(prefix), rengine_(rdevice_()), data_cb_(std::move(on_data)), 
  getCurrentPos_(getCurrentPos), getCurrentPit_(getCurrentPit), 
  getNumSorroundingNodes_(getNumSorroundingNodes), getFaceById_(getFaceById) {
  
  /* Initialize statistics */
  retx_sync_interest = 0;
  retx_data_interest = 0;
  retx_bundled_interest = 0;
  received_sync_interest = 0;
  suppressed_sync_interest = 0;
  should_receive_interest = 0;
  received_interest = 0;
  data_reply = 0;
  received_data_mobile = 0;
  received_data_mobile_from_repo = 0;

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
  isStatic = false;
  generate_data = true;     
  version_vector_[nid_] = 0;
  if (nid_ >= 20) {
    isStatic = true;
    generate_data = false;
  } 

  // face0 = getFaceById_(0);
  face1 = getFaceById_(1);
  // face2 = getFaceById_(2);

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
    std::cout << "node(" << nid_ << ") received_sync_interest = " << received_sync_interest << std::endl;
    std::cout << "node(" << nid_ << ") suppressed_sync_interest = " << suppressed_sync_interest << std::endl;
    std::cout << "node(" << nid_ << ") should_receive_interest = " << should_receive_interest << std::endl;
    std::cout << "node(" << nid_ << ") received_interest = " << received_interest << std::endl;
    std::cout << "node(" << nid_ << ") data_reply = " << data_reply << std::endl;
    std::cout << "node(" << nid_ << ") received_data_mobile = " << received_data_mobile << std::endl;
    std::cout << "node(" << nid_ << ") received_data_mobile_from_repo = " << received_data_mobile_from_repo << std::endl;
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
  scheduler_.scheduleEvent(time::milliseconds(100 * nid_),
                           [this, content] { PublishData(content); });

  /* Init async interest sending */
  // scheduler_.scheduleEvent(time::milliseconds(10 * nid_),
  //                          [this] { AsyncSendPacket(); });
  scheduler_.scheduleEvent(time::milliseconds(1 * nid_),
                           [this] { AsyncSendPacket(); });
  
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
  if (isStatic)
    return;
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Store New Data: " << name.toUri() << std::endl;
}

void Node::logStateStore(const NodeID& nid, int64_t seq) {
  if (isStatic)
    return;  
  std::string state_tag = to_string(nid) + "-" + to_string(seq);
  int64_t now = ns3::Simulator::Now().GetMicroSeconds();
  std::cout << now << " microseconds node(" << nid_ << ") Update New Seq: " << state_tag << std::endl;
}

void Node::AsyncSendPacket() {
  // VSYNC_LOG_TRACE ("node(" << nid_ << ") Queue size: " << pending_sync_interest.size() << ", " << pending_ack.size() );
  if (pending_sync_interest.size() > 0 || 
      pending_data_interest.size() > 0 || 
      pending_ack.size() > 0 || 
      pending_data_reply.size() > 0) {
    Name n; 
    Packet packet;
    // if (pending_sync_interest.size() > 0) {
    //   packet = pending_sync_interest.front();
    //   pending_sync_interest.pop_front();
    // } 
    // else if (pending_ack.size() > 0) {
    //   packet = pending_ack.front();
    //   pending_ack.pop_front();
    //   // pending_ack.clear();
    // } 
    if (pending_ack.size() > 0) {
      packet = pending_ack.front();
      pending_ack.pop_front();
      // pending_ack.clear();
    }
    else if (pending_data_reply.size() > 0) {
      packet = pending_data_reply.front();
      pending_data_reply.pop_front();
    } 
    else if (pending_sync_interest.size() > 0) {
      packet = pending_sync_interest.front();
      pending_sync_interest.pop_front();
    }
    else {
      packet = pending_data_interest.front();
      pending_data_interest.pop_front();
    }
    switch (packet.packet_type) {

      case Packet::INTEREST_TYPE:
        n = (packet.interest)->getName();
        if (n.compare(0, 2, kSyncDataPrefix) == 0) {            /* Data interest */
          /* Remove falsy data interest */
          if (data_store_.find(n) != data_store_.end()) {
            VSYNC_LOG_TRACE ("node(" << nid_ << ") Drop falsy data interest: i.name=" << n.toUri() );
            AsyncSendPacket();
            return;
          }
          /**
           * Check PIT to see if there's outstanding interest with same name. If
           *  same name exists in PIT, it means it has recently been sent, and 
           *  should be moved to the end of the queue.
           * However, for forwarded interest, send it even if its name exists 
           *  in PIT.
           */
          // Pit pit = getCurrentPit_();
          if (getCurrentPit_().find(*packet.interest) != nullptr && 
              packet.packet_origin != Packet::FORWARDED) {
            if (log_verbose)
              VSYNC_LOG_TRACE ("node(" << nid_ << ") Data interest already in PIT: i.name=" << n.toUri() );
            /* Only add ORIGINAL packets back to queue */
            switch (packet.packet_origin) {
              case Packet::ORIGINAL:
                /* Add packet back to queue with longer delay to avoid retransmissions */
                scheduler_.scheduleEvent(kRetxDataInterestTime, [this, packet] {
                  // packet.last_sent = ns3::Simulator::Now().GetMicroSeconds();
                  pending_data_interest.push_back(packet);
                });
                /** Schedule next async transmission with delay, to avoid looping 
                 *  through the queue too fast */
                scheduler_.scheduleEvent(time::milliseconds(1), [this] {
                  AsyncSendPacket();
                });
                // break;
                return;
              default:
                /* Best effort, don't add to queue. Send next packet immediately */
                AsyncSendPacket();
                return;
            }
          } 
          else {  /* Not in PIT */
            face_.expressInterest(*packet.interest,
                                  std::bind(&Node::OnDataReply, this, _2, packet.packet_origin),
                                  [](const Interest&, const lp::Nack&) {},
                                  [](const Interest&) {});
            int sorrounding = getNumSorroundingNodes_();
            // should_receive_interest += sorrounding;
            switch (packet.packet_origin) {
              case Packet::ORIGINAL:
                VSYNC_LOG_TRACE ("node(" << nid_ << ") Send data interest: i.name=" << n.toUri()
                                 << ", should be received by " << sorrounding );
                // VSYNC_LOG_TRACE ("node(" << nid_ << ") queue length=" << pending_data_interest.size() );
                /* Add packet back to queue with longer delay to avoid retransmissions */
                scheduler_.scheduleEvent(kRetxDataInterestTime, [this, packet] {
                  pending_data_interest.push_back(packet);
                });
                break;
              case Packet::FORWARDED:
                VSYNC_LOG_TRACE ("node(" << nid_ << ") Forward data interest: i.name=" << n.toUri()
                                 << ", should be received by " << sorrounding );
                /* Best effort, don't add to queue */
                break;
              default:
                assert(0);
            }
          }
        } 
        else if (n.compare(0, 2, kBundledDataPrefix) == 0) {  /* Bundled data interest */
          face_.expressInterest(*packet.interest,
                                std::bind(&Node::OnBundledDataReply, this, _2),
                                [](const Interest&, const lp::Nack&) {},
                                [](const Interest&) {});
          pending_data_interest.push_back(packet);
          VSYNC_LOG_TRACE ("node(" << nid_ << ") Send bundled data interest: i.name=" << n.toUri() );
        } 
        else if (n.compare(0, 2, kSyncNotifyPrefix) == 0) {   /* Sync interest */
          face_.expressInterest(*packet.interest,
                                std::bind(&Node::OnSyncAck, this, _2),
                                [](const Interest&, const lp::Nack&) {},
                                [](const Interest&) {});
          int sorrounding = getNumSorroundingNodes_();
          VSYNC_LOG_TRACE ("node(" << nid_ << ") Send sync interest: i.name=" << n.toUri() 
                           << ", should be received by " << sorrounding );
          // VSYNC_LOG_TRACE ("node(" << nid_ << ") Should receive interest: " << getNumSorroundingNodes_() );
          should_receive_interest += sorrounding;
        }
        break;

      case Packet::DATA_TYPE:
        n = (packet.data)->getName();
        if (n.compare(0, 2, kSyncDataPrefix) == 0) {            /* Data */
          data_reply++;
          face_.put(*packet.data);
          VSYNC_LOG_TRACE( "node(" << nid_ << ") Send data = " << (packet.data)->getName());
        } else if (n.compare(0, 2, kBundledDataPrefix) == 0) {  /* Bundled data */
          face_.put(*packet.data);
        } else if (n.compare(0, 2, kSyncNotifyPrefix) == 0) {   /* Sync ACK */
          VSYNC_LOG_TRACE ("node(" << nid_ << ") replying ACK:" << n.toUri() );
          face_.put(*packet.data);
        } else {                                                /* Shouldn't get here */
          assert(0);
        }
        break;

      default:
        assert(0);  /* Shouldn't get here */
    }
  }
  
  /* Schedule self */
  int delay = packet_dist(rengine_);
  // VSYNC_LOG_TRACE ("node(" << nid_ << ") Re-scheduling" );
  scheduler_.scheduleEvent(time::microseconds(delay), [this] {
    AsyncSendPacket();
  });
}


/* Packet processing pipeline */
/* 1. Sync packet processing */
void Node::SendSyncInterest() {
  // int delay = dt_dist(rengine_);
  // scheduler_.scheduleEvent(time::microseconds(delay), [this] {
  //   std::string encoded_vv = EncodeVVToName(version_vector_);
  //   // Don't know why append time stamp, but easier to keep it than remove
  //   auto cur_time = ns3::Simulator::Now().GetMicroSeconds();
  //   auto pending_sync_notify = MakeSyncNotifyName(nid_, encoded_vv, cur_time);
    

  //   VSYNC_LOG_TRACE( "node(" << nid_ << ") Send sync interest: i.name=" << pending_sync_notify.toUri());
  //   face_.expressInterest(pending_sync_notify, std::bind(&Node::OnSyncAck, this, _2),
  //                         [](const Interest&, const lp::Nack&) {},
  //                         [](const Interest&) {});
  // });
  std::string encoded_vv = EncodeVVToName(version_vector_);
  auto cur_time = ns3::Simulator::Now().GetMicroSeconds();
  auto pending_sync_notify = MakeSyncNotifyName(nid_, encoded_vv, cur_time);
  Packet packet;
  packet.packet_type = Packet::INTEREST_TYPE;
  packet.interest = std::make_shared<Interest>(pending_sync_notify, kSendOutInterestLifetime);
  pending_sync_interest.clear();
  pending_sync_interest.push_back(packet);
  // VSYNC_LOG_TRACE( "node(" << nid_ << ") Added sync interest to queue: i.name=" << pending_sync_notify.toUri());
}

void Node::OnSyncInterest(const Interest &interest) {

  const auto& n = interest.getName();
  NodeID node_id = ExtractNodeID(n);
  auto other_vv = DecodeVVFromName(ExtractEncodedVV(n));
  received_sync_interest++;
  received_interest++;

  /* Merge state vector, add missing data to pending_data_interest */
  bool otherVectorNew = false;
  std::vector<Packet> missing_data;
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
        Packet packet;
        packet.packet_type = Packet::INTEREST_TYPE;
        packet.packet_origin = Packet::ORIGINAL;
        packet.last_sent = 0;
        packet.interest = std::make_shared<Interest>(n, kSendOutInterestLifetime);
        missing_data.push_back(packet);
      }
      version_vector_[node_id] = seq_other;  // Will be set later
    }
  }
  if (missing_data.size() > kMissingDataThreshold) {
    auto n = MakeBundledDataName(node_id, EncodeVVToName(mv));
    Packet packet;
    packet.packet_type = Packet::INTEREST_TYPE;
    packet.interest = std::make_shared<Interest>(n, kSendOutInterestLifetime);
  } else {
    for (size_t i = 0; i < missing_data.size(); ++i)
      pending_data_interest.push_back(missing_data[i]);
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


  /* If incoming state not newer, reset timer to delay sending next sync interest */
  if (!otherVectorNew && !myVectorNew) {  /* Case 1: Other vector same  */
    scheduler_.cancelEvent(retx_event);
    int delay = retx_dist(rengine_);
    VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest:" << n.toUri()
                     << ", will reset retx timer" );
    pending_sync_interest.clear();
    retx_event = scheduler_.scheduleEvent(time::microseconds(delay), [this] { 
      RetxSyncInterest();
    });
    suppressed_sync_interest++;
  } else if (!otherVectorNew) {           /* Case 2: Other vector doesn't contain newer state */
    /* Do nothing */
    VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest:" << n.toUri()
                     << ", do nothing" );
  } else {                                /* Case 3: Other vector contain newer state */
    VSYNC_LOG_TRACE ("node(" << nid_ << ") Recv a syncNotify Interest:" << n.toUri()
                     << ", will do flooding");
    RetxSyncInterest(); // Flood
  }

  /**
   * If suppress ACK, always overhear other ACKs, otherwise always return ACK.
   * In both cases set ACK delay timer based on whether I have new state.
   */
  if (kSyncAckSuppression) {
    Interest interest_overhear(n, kAddToPitInterestLifetime);
    face_.expressInterest(interest_overhear, std::bind(&Node::OnSyncAck, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    auto p = overheard_sync_interest.find(n);
    if (p != overheard_sync_interest.end()) {
      scheduler_.cancelEvent(p -> second);  // Cancel to refresh event 
      overheard_sync_interest.erase(p);
    }
    if (myVectorNew) {
      // int delay = dt_dist(rengine_);
      // overheard_sync_interest[n] = scheduler_.scheduleEvent(
      //   time::microseconds(delay), [this, n] {
          // overheard_sync_interest.erase(n);
          VSYNC_LOG_TRACE ("node(" << nid_ << ") will reply ACK with new state: " << n.toUri() );
          SendSyncAck(n);
      //   }
      // );
    } 
    else {
      int delay = ack_dist(rengine_);
      VSYNC_LOG_TRACE ("node(" << nid_ << ") will reply ACK without new state: " << n.toUri() );
      overheard_sync_interest[n] = scheduler_.scheduleEvent(
        time::microseconds(delay), [this, n] {
          overheard_sync_interest.erase(n);
          SendSyncAck(n);
        }
      );
    }
  } 
  else {
    int delay;
    if (myVectorNew) {
      delay = dt_dist(rengine_);
      VSYNC_LOG_TRACE ("node(" << nid_ << ") will reply ACK immediately:" << n.toUri() );
    } else {
      delay = ack_dist(rengine_);
      VSYNC_LOG_TRACE ("node(" << nid_ << ") will reply ACK with delay:" << n.toUri() );
    }
    scheduler_.scheduleEvent(time::microseconds(delay), [this, n] { 
      SendSyncAck(n); 
    });
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

  Packet packet;
  packet.packet_type = Packet::DATA_TYPE;
  packet.data = ack;
  pending_ack.push_back(packet);
}

void Node::OnSyncAck(const Data &ack) {
  const auto& n = ack.getName();

  if (kSyncAckSuppression){
    /* Remove pending ACK from both pending events and queue */
    for (auto it = pending_ack.begin(); it != pending_ack.end(); ++it) {
      if (it->data->getName() == n) {
        // it = pending_ack.erase(it);  // TODO: Cause bug for unknown reason
      }
    }
    if (overheard_sync_interest.find(n) != overheard_sync_interest.end()) {
      scheduler_.cancelEvent(overheard_sync_interest[n]);
      overheard_sync_interest.erase(n);
      VSYNC_LOG_TRACE ("node(" << nid_ << ") Overhear sync ack, suppress pending ACK: " << ack.getName().toUri());
      return;
    } 
  }

  VSYNC_LOG_TRACE ("node(" << nid_ << ") RECV sync ack: " << ack.getName().toUri());

  /* Extract difference and add to pending_data_interest */
  const auto& content = ack.getContent();
  proto::AckContent content_proto;
  if (!content_proto.ParseFromArray(content.value(), content.value_size())) {
    VSYNC_LOG_WARN("Invalid data AckContent format: nid=" << nid_);
    assert(false);
  }
  auto vector_other = DecodeVV(content_proto.vv());
  // bool otherVectorNew = false;    /* Set if remote vector has newer state */
  std::vector<Packet> missing_data;
  for (auto entry : vector_other) {
    auto node_id = entry.first;
    auto node_seq = entry.second;
    if (version_vector_.find(node_id) == version_vector_.end() || 
        version_vector_[node_id] < node_seq) {
      // otherVectorNew = true;
      auto start_seq = version_vector_.find(node_id) == version_vector_.end() ? 
                       1: version_vector_[node_id] + 1;
      for (auto seq = start_seq; seq <= node_seq; ++seq) {
        logStateStore(node_id, seq);
        auto n = MakeDataName(node_id, seq);
        Packet packet;
        packet.packet_type = Packet::INTEREST_TYPE;
        packet.packet_origin = Packet::ORIGINAL;
        packet.interest = std::make_shared<Interest>(n, kSendOutInterestLifetime);
        missing_data.push_back(packet);
      }
      version_vector_[node_id] = node_seq;
    }
  }
  for (size_t i = 0; i < missing_data.size(); ++i)
    pending_data_interest.push_back(missing_data[i]);

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
  /* Do nothing */
}

void Node::OnDataInterest(const Interest &interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data interest: i.name=" << n.toUri());
  // received_interest++;

  auto iter = data_store_.find(n);
  if (iter != data_store_.end()) {
    Packet packet;
    packet.packet_type = Packet::DATA_TYPE;
    if (isStatic) { /* Set content type for repos */
      Data data(n);
      data.setFreshnessPeriod(time::seconds(3600));
      data.setContent(iter->second->getContent().value(), iter->second->getContent().size());
      data.setContentType(kRepoData);
      key_chain_.sign(data, signingWithSha256());
      packet.data = std::make_shared<Data>(data);
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Send type repo data = " << iter->second->getName());
    } else {
      packet.data = iter -> second;
    }
    pending_data_reply.push_back(packet);
  } else if (kMultihopData) {
    /* Otherwise add to my PIT, but send probabilistically */
    int p = mhop_dist(rengine_);
    if (p < pMultihopForwardDataInterest) {
      Packet packet;
      packet.packet_type = Packet::INTEREST_TYPE;
      packet.packet_origin = Packet::FORWARDED;
      packet.interest = std::make_shared<Interest>(n, kSendOutInterestLifetime);

      /** 
       * Need to remove PIT, otherwise interferes with checking PIT entry. 
       * Remove only in-record of wifi face, and out-record of app face.
       **/
      // // TODO
      // if (getCurrentPit_().find(*packet.interest) != nullptr) {
      //   auto it = getCurrentPit_().find(*packet.interest);
      //   VSYNC_LOG_TRACE( "node(" << nid_ << ") Data interest to forward already in PIT: i.name=" << n.toUri());  
      // } else {
      //   pending_data_interest.push_front(packet);
      //   VSYNC_LOG_TRACE( "node(" << nid_ << ") Add forwarded interest to queue: i.name=" << n.toUri());
      // }

      pending_data_interest.push_front(packet);
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Add forwarded interest to queue: i.name=" << n.toUri());

    } else {
      // int delay = dt_dist(rengine_);
      // scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
        VSYNC_LOG_TRACE( "node(" << nid_ << ") Suppress data interest: i.name=" << n.toUri());
        Interest interest_suppress(n, kAddToPitInterestLifetime);
        face_.expressInterest(interest_suppress, std::bind(&Node::OnDataReply, this, _2, Packet::SUPPRESSED),
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

void Node::OnDataReply(const Data &data, Packet::SourceType sourceType) {

  if (!isStatic)
    received_data_mobile++;

  const auto& n = data.getName();
  // assert(n.compare(waiting_data) == 0);              // TODO: no longer true
  // assert(data_store_.find(n) == data_store_.end());  // TODO: no longer true
  if (data_store_.find(n) != data_store_.end()) {
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Drops duplicate data: name=" << n.toUri());
    return;
  }

  /* Print based on source */
  switch(sourceType) {
    case Packet::ORIGINAL:
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv data reply: name=" << n.toUri());
      break;
    case Packet::FORWARDED:
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv forwarded data reply: name=" << n.toUri());
      break;
    case Packet::SUPPRESSED:
      VSYNC_LOG_TRACE( "node(" << nid_ << ") Recv suppressed data reply: name=" << n.toUri());
      break;
    default:
      assert(0);
  }

  /* Save data */
  /* Check content type */
  if (data.getContentType() == kRepoData) {
    std::shared_ptr<Data> data_no_flag = std::make_shared<Data>(n);
    data_no_flag -> setFreshnessPeriod(time::seconds(3600));
    data_no_flag -> setContent(data.getContent().value(), data.getContent().size());
    data_no_flag -> setContentType(kUserData);
    key_chain_.sign(*data_no_flag, signingWithSha256());
    data_store_[n] = data_no_flag;
    VSYNC_LOG_TRACE( "node(" << nid_ << ") Receive data from repo: name=" << n.toUri());
    if (!isStatic)
      received_data_mobile_from_repo++;
  } else {
    data_store_[n] = data.shared_from_this();
  }
  logDataStore(n);

  // /* Trigger sending data interest */  // TODO: What's this?
  // scheduler_.cancelEvent(wt_data_interest);
  // left_retx_count = kInterestTransmissionTime;
  // SendDataInterest();

  /* Broadcast the received data for multi-hop */
  if (kMultihopData) {
    // int delay = dt_dist(rengine_);
    // scheduler_.scheduleEvent(time::microseconds(delay), [this, n] {
    //   data_reply++;
    //   face_.put(*data_store_[n]);
    //   VSYNC_LOG_TRACE( "node(" << nid_ << ") Re-broadcast data: name=" << n.toUri());
    // });
    Packet packet;
    packet.packet_type = Packet::DATA_TYPE;
    packet.data = std::make_shared<Data>(data);
    pending_data_reply.push_back(packet);
  }
}



/* 3. Bundled data packet processing */
void Node::SendBundledDataInterest() {
  /* Do nothing */
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

      size_t cur_data_content_size = pack_data_proto.SerializeAsString().size();
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
  // assert(pending_interest.front().size() == 1);   /* Is bundled interest */
  // pending_interest.front().pop();
  // TODO: Remove bundled interest from queue
  if (!next_vv.empty()) {
    std::cout << "node(" << nid_ << ") sends next bundled interest: " 
              << next_bundle.toUri() << std::endl;
    // pending_interest.front().push(next_bundle);
    Packet packet;
    packet.packet_type = Packet::INTEREST_TYPE;
    packet.interest = std::make_shared<Interest>(next_bundle, kSendOutInterestLifetime);
    pending_data_interest.push_front(packet);
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
  scheduler_.cancelEvent(retx_event);
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