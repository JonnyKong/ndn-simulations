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

/* Public */
Node::Node(Face &face, Scheduler &scheduler, Keychain &key_chain, const NodeID &nid,
           const Name &prefix, DataCb on_data) : 
           face_(face), scheduler_(scheduler),key_chain_(key_chain), nid_(nid),
           prefix_(prefix), data_cb_(std::move(on_data)) {
  
  /* Initialize statistics */
  data_num = 0;

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

  /* Initiate event scheduling */
  /* 2s: Start simulation */
  scheduler_.scheduleEvent(time::milliseconds(2000), [this] { StartSimulation(); });

  /* 400s: Stop data generation */

  /* 1195s: Print NFD statistics */
  scheduler_.scheduleEvent(time::seconds(1195), [this] {
    // std::cout << "node(" << nid_ << ") outInterest = " << out_interest_num << std::endl;
    // std::cout << "node(" << nid_ << ") average time to meet a new node = " << total_time / (double)count << std::endl;
    std::cout << "node(" << nid_ << ") retx_notify_interest = " << retx_notify_interest << std::endl;
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
  notify_time = kSyncNotifyMax;
  
}

void Node::OnSyncInterest(const Interest &interest) {

}

void Node::SendSyncAck() {

}

void Node::onSyncAck(const Data &data) {

}


/* 2. Data packet processing */
void Node::SendDataInterest() {

}

void Node::OnDataInterest(const Interest &interest) {

}

void Node::SendDataReply() {

}

void Node::onDataReply(const Data &data) {

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