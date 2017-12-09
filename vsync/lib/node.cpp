/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <random>

#include <boost/log/trivial.hpp>

#include <ndn-cxx/util/digest.hpp>

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

static time::milliseconds kReplyWaitingTime = time::milliseconds(1000);
static time::milliseconds kInterval = time::milliseconds(10000);
static time::milliseconds kSyncTime = time::milliseconds(1000);

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, const GroupID& gid,
           const uint64_t group_size, Node::DataCb on_data)
           : face_(face),
             scheduler_(scheduler),
             key_chain_(key_chain),
             nid_(nid),        
             prefix_(prefix),
             gid_(name::Component(gid).toUri()),
             data_cb_(std::move(on_data)),
             rengine_(rdevice_()),
             rdist_(3000, 10000) {
  version_vector_ = VersionVector(group_size, 0);
  data_store_ = std::vector<std::vector<std::shared_ptr<Data>>>(group_size, std::vector<std::shared_ptr<Data>>(0));
  node_state = kActive;

  face_.setInterestFilter(
      Name(kSyncPrefix).append(gid_), std::bind(&Node::OnSyncInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register vsync prefix: " << reason); 
        throw Error("Failed to register vsync prefix: " + reason);
      });


  face_.setInterestFilter(
      Name(kSyncDataListPrefix).append(gid_), std::bind(&Node::OnDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register data prefix: " << reason); 
        throw Error("Failed to register data prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kProbePrefix).append(gid_), std::bind(&Node::OnProbeInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register probe interest prefix: " << reason); 
        throw Error("Failed to register probe interest prefix: " + reason);
      });

  face_.setInterestFilter(
      Name(kReplyPrefix).append(gid_), std::bind(&Node::OnReplyInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register reply interest prefix: " << reason); 
        throw Error("Failed to register reply interest prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                           [this] { SendProbeInterest(); });
}

//OK
void Node::PublishData(const std::string& content, uint32_t type) {
  if (node_state == kActive) {
    version_vector_[nid_]++;

    // make data dame
    auto n = MakeDataName(gid_, nid_, version_vector_[nid_]);
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    // set data content
    data->setContent(reinterpret_cast<const uint8_t*>(content.data()),
                     content.size());
    data->setContentType(type);
    key_chain_.sign(*data, signingWithSha256());

    data_store_[nid_].push_back(data);

    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Publish Data: d.name=" << n.toUri());
  }
}

void Node::SyncData() {
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Start to Sync Data");
  SendSyncInterest();
}

void Node::SendSyncInterest() {
  // make the sync interest name
  std::string vv_encode = EncodeVV(version_vector_);
/*
  std::cout << "``````````testing decode/encode VV```````````" << std::endl;
  std::cout << "version vector: " << VersionVectorToString(version_vector_) << std::endl;
  std::cout << "encodeVV size = " << vv_encode.size() << std::endl;
  VersionVector vv = DecodeVV(vv_encode);
  std::cout << "decoded version vector " << VersionVectorToString(vv) << std::endl;
  std::cout << "``````````````````````````````````````````````" << std::endl;
*/
  auto n = MakeSyncInterestName(gid_, nid_, vv_encode);

  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Send Sync Interest: i.name=" << n.toUri());

  Interest i(n, time::milliseconds(4000));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnSyncInterest(const Interest& interest) {
  const auto& n = interest.getName();
  // Check sync interest name size
  if (n.size() != kSyncPrefix.size() + 3) {
    VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Invalid sync interest name: " << n.toUri());
    return;
  }

  auto group_id = ExtractGroupID(n);
  auto node_id = ExtractNodeID(n);
  auto other_vv_str = ExtractEncodedVV(n);
  VersionVector other_vv = DecodeVV(other_vv_str);
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv Sync Interest: i.version_vector=" << VersionVectorToString(other_vv));
  if (other_vv.size() != version_vector_.size()) {
    VSYNC_LOG_TRACE("Different Version Vector Size in Group: " << gid_);
    return;
  }

  SendSyncReply(n);
  // Process version vector
  VersionVector old_vv = version_vector_;
  for (NodeID i = 0; i < version_vector_.size(); ++i) {
    uint64_t other_seq = other_vv[i];
    uint64_t my_seq = version_vector_[i];
    if (other_seq > my_seq) {
      /*
      Question:
      we should update the version_vector_[i] here or after we receive the data?? (in OnRemoteData())
      */
      SendDataInterest(i, my_seq + 1, other_seq);
    }
  }
}

void Node::SendSyncReply(const Name& n) {
  // currently do nothing here
}

void Node::SendDataInterest(const NodeID& node_id, uint64_t start_seq, uint64_t end_seq) {
  auto n = MakeDataListName(gid_, node_id, start_seq, end_seq);
  Interest i(n, time::milliseconds(1000));
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send: i.name=" << n.toUri());

  face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnDataInterest(const Interest& interest) {
  const auto& n = interest.getName();
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv Data Interest: i.name=" << n.toUri());

  if (n.size() < 6) {
    VSYNC_LOG_TRACE("Invalid data name: " << n.toUri());
    return;
  }

  auto group_id = ExtractGroupIDFromData(n);
  auto node_id = ExtractNodeIDFromData(n);
  auto start_seq = ExtractStartSequenceNumber(n);
  auto end_seq = ExtractEndSequenceNumber(n);

  if(group_id != gid_) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Ignore data interest from different group: " << group_id);
    return;
  }

  if (end_seq > version_vector_[node_id]) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") doesn't contain the requesting data range based on Version Vector");
    return;
  }
  else if (end_seq > data_store_[node_id].size()) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") doesn't contain the requesting data range based on Data Store");
    return;
  }
  else {
    // encode data(start_seq-end_seq) into one data packet
    std::vector<std::pair<uint32_t, std::string>> data_list;
    std::vector<std::shared_ptr<Data>> node_data = data_store_[node_id];
    for (int i = start_seq; i <= end_seq; ++i) {
      // data_store_ starts from 0, while version_vector_ starts from 1
      std::shared_ptr<Data> data = node_data[i - 1];
      uint32_t type = data->getContentType();
      // get the data content
      std::string content = readString(data->getContent());
      data_list.push_back(std::pair<uint32_t, std::string>(type, content));
    }
    // encode the data list
    std::string dl_encode;
    EncodeDL(data_list, dl_encode);
    // make the data packet
    std::shared_ptr<Data> data = std::make_shared<Data>(n);
    data->setFreshnessPeriod(time::seconds(3600));
    data->setContent(reinterpret_cast<const uint8_t*>(dl_encode.data()),
                     dl_encode.size());
    data->setContentType(kUserData);
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);
  }
}

void Node::OnRemoteData(const Data& data) {
  const auto& n = data.getName();
  if (n.size() < 6) {
    VSYNC_LOG_TRACE("Invalid data name: " << n.toUri());
    return;
  }

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv data: name=" << n.toUri());

  auto node_id = ExtractNodeIDFromData(n);
  auto start_seq = ExtractStartSequenceNumber(n);
  auto end_seq = ExtractEndSequenceNumber(n);

  // Store a local copy of received data
  if (version_vector_[node_id] < start_seq - 1) {
    // should not happen!!;
    VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") has data gap! Should not happen!");
    return;
  }
  else if (version_vector_[node_id] >= end_seq) {
    return;
  }
  else {
    auto content_type = data.getContentType();
    const auto& content = data.getContent();
    proto::DL dl_proto;
    if (dl_proto.ParseFromArray(reinterpret_cast<const uint8_t*>(content.value()),
                                content.value_size())) {
      auto data_list = DecodeDL(dl_proto);
      if (data_list.size() != end_seq - start_seq + 1) {
        VSYNC_LOG_TRACE("Data List size doesn't match endSeq - startSeq! Should not happen!");
        return;
      }
      if (version_vector_[node_id] + 1 >= start_seq) start_seq = version_vector_[node_id] + 1;
      
      std::string recv_data_list= "";
      for (int i = start_seq; i <= end_seq; ++i) {
        auto cur_name = MakeDataName(gid_, node_id, i);
        auto cur_type = data_list[i - 1].first;
        auto cur_content = data_list[i - 1].second;

        std::shared_ptr<Data> cur_data = std::make_shared<Data>(cur_name);
        cur_data->setFreshnessPeriod(time::seconds(3600));
        // set data content
        cur_data->setContent(reinterpret_cast<const uint8_t*>(cur_content.data()),
                             cur_content.size());
        cur_data->setContentType(cur_type);
        recv_data_list += cur_content + "; ";

        data_store_[node_id].push_back(cur_data);
      }
      VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv the data: " << recv_data_list);

      assert(data_store_[node_id].size() == end_seq);
      version_vector_[node_id] = end_seq;
      data_cb_(version_vector_);
    }
    else {
      VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Decode Data List Fail!");
      return;
    }
  }
}

// sleeping mechanisms
void Node::SendProbeInterest() {
  node_state = kIntermediate;
  // make the nfd work
  Interest wakeupNFD(kLocalhostWakeupCommand);
  face_.expressInterest(wakeupNFD, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});

  auto n = MakeProbeInterestName(gid_);
  Interest i(n, time::milliseconds(1000));
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send ProbeInterest: i.name=" << n.toUri());

  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
  scheduler_.scheduleEvent(kReplyWaitingTime, [this] { CalculateReply(); });
}

void Node::CalculateReply() {
  /* we should guarantee that there should be at least two active nodes in the group. if so, the current 
  node can continue to go to sleep. Otherwise it should start to work.
  */
  size_t active_in_group = received_reply.size();
  std::cout << "node(" << gid_ << " " << nid_ << ") receives " << active_in_group << " replies." << std::endl;
  if (active_in_group >= 2) {
    // go to sleep again
    SyncData();
    // we need to wait for some time to make the data synced before going to sleep
    scheduler_.scheduleEvent(kSyncTime, [this] { 
      std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
      Interest i(kLocalhostSleepingCommand);
      face_.expressInterest(i, [](const Interest&, const Data&) {},
                            [](const Interest&, const lp::Nack&) {},
                            [](const Interest&) {});
      received_reply.clear();
      node_state = kSleeping;
    });
  }
  else {
    // start to work
    std::cout << "node(" << gid_ << " " << nid_ << ") will start to work" << std::endl;
    Interest i(kLocalhostWakeupCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    received_reply.clear();
    node_state = kActive;
  }
  // schedule to next round
  scheduler_.scheduleEvent(kInterval, [this] { SendProbeInterest(); });
}

void Node::OnProbeInterest(const Interest& interest) {
  if (node_state == kActive) {
    const auto& n = interest.getName();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv probe interest: name=" << n.toUri());
    std::uniform_int_distribution<> rdist2_(0, 1000);
    scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
                             [this] { SendReplyInterest(); });
  }
}

void Node::SendReplyInterest() {
  if (node_state == kActive) {
    auto n = MakeReplyInterestName(gid_, nid_);
    Interest i(n, time::milliseconds(1000));
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send ReplyInterest: i.name=" << n.toUri());

    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
  }
}

void Node::OnReplyInterest(const Interest& interest) {
  if (node_state == kIntermediate) {
    const auto& n = interest.getName();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv reply interest: name=" << n.toUri());
    auto reply_node = ExtractNodeID(n);
    received_reply.insert(reply_node);
  }
}
  
}  // namespace vsync
}  // namespace ndn
