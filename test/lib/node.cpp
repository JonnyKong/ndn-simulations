/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <random>

#include <boost/log/trivial.hpp>

#include <ndn-cxx/util/digest.hpp>

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"

namespace ndn {
namespace vsync {

VSYNC_LOG_DEFINE(VectorSync);

Node::Node(Face& face, Scheduler& scheduler, KeyChain& key_chain,
           const NodeID& nid, const Name& prefix, const GroupID& gid,
           const uint64_t group_size, Node::DataCb on_data)
           : face_(face),
             key_chain_(key_chain),
             nid_(nid),        
             prefix_(prefix),
             gid_(name::Component(gid).toUri()),
             data_cb_(std::move(on_data)) {
  version_vector_ = VersionVector(group_size, -1);
  data_store_ = std::vector<std::vector<std::shared_ptr<Data>>>(group_size, std::vector<std::shared_ptr<Data>>(0));

  face_.setInterestFilter(
      Name(kSyncPrefix).append(gid_), std::bind(&Node::OnSyncInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register vsync prefix: " << reason); 
        throw Error("Failed to register vsync prefix: " + reason);
      });


  face_.setInterestFilter(
      Name(kSyncDataPrefix).append(gid_), std::bind(&Node::OnDataInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register data prefix: " << reason); 
        throw Error("Failed to register data prefix: " + reason);
      });
}

//OK
void Node::PublishData(const std::string& content, uint32_t type) {
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

void Node::SyncData() {
  SendSyncInterest();
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Start to Sync Data");
}

void Node::SendSyncInterest() {
  // make the sync interest name
  std::string vv_encode;
  EncodeVV(version_vector_, vv_encode);
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
  auto other_vv = ExtractEncodedVV(n);
  if (other_vv.size() != version_vector_.size()) {
    VSYNC_LOG_TRACE("Different Version Vector Size in Group: " << gid_);
    return;
  }

  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv Sync Interest: i.name=" << n.toUri());
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
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send: i.name=" << i.toUri());

  face_.expressInterest(i, std::bind(&Node::OnRemoteData, this, _2),
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnDataInterest(const Interest& interest) {
  const auto& n = interest.getName();
  if (n.size() < 6) {
    VSYNC_LOG_TRACE("Invalid data name: " << n.toUri());
    return;
  }

  auto group_id = ExtractGroupID(n);
  auto start_seq = ExtractStartSequenceNumber(n);
  auto end_seq = ExtractEndSequenceNumber(n);

  if(group_id != gid_) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Ignore data interest from different group: " << group_id);
    return;
  }

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv Data Interest: i.name=" << n.toUri());
  if (end_seq > version_vector_[nid_]) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << "doesn't contain the requesting data range based on Version Vector");
    return;
  }
  else if (end_seq > data_store_[nid_].size()) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << "doesn't contain the requesting data range based on Data Store");
    return;
  }
  else {
    // encode data(start_seq-end_seq) into one data packet
    std::vector<std::pair<uint32_t, std::string>> data_list;
    std::vector<std::shared_ptr<Data>> node_data = data_store_[nid_];
    for (int i = start_seq; i <= end_seq; ++i) {
      std::shared_ptr<Data> data = node_data[i];
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

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << "Recv data: name=" << n.toUri());

  auto node_id = ExtractNodeIDFromData(n);
  auto start_seq = ExtractStartSequenceNumber(n);
  auto end_seq = ExtractEndSequenceNumber(n);

  // Store a local copy of received data
  if (version_vector_[node_id] < start_seq - 1) {
    // should not happen!!;
    VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << "has data gap! Should not happen!");
    return;
  }
  else if (version_vector_[node_id] >= end_seq) {
    return;
  }
  else {
    auto content_type = data.getContentType();
    const auto& content = data.getContent();
    proto::DL dl_proto;
    if (dl_proto.ParseFromArray(content.value(), content.value_size())) {
      auto data_list = DecodeDL(dl_proto);
      if (data_list.size() != end_seq - start_seq + 1) {
        VSYNC_LOG_TRACE("Data List size doesn't match endSeq - startSeq! Should not happen!");
        return;
      }
      if (version_vector_[node_id] + 1 >= start_seq) start_seq = version_vector_[node_id] + 1;
      for (int i = start_seq; i <= end_seq; ++i) {
        auto cur_name = MakeDataName(gid_, node_id, i);
        auto cur_type = data_list[i].first;
        auto cur_content = data_list[i].second;

        std::shared_ptr<Data> cur_data = std::make_shared<Data>(cur_name);
        cur_data->setFreshnessPeriod(time::seconds(3600));
        // set data content
        cur_data->setContent(reinterpret_cast<const uint8_t*>(cur_content.data()),
                             cur_content.size());
        cur_data->setContentType(cur_type);
        data_store_[node_id].push_back(cur_data);
      }
      version_vector_[node_id] = end_seq;
      data_cb_(version_vector_);
    }
    else {
      VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << "Decode Data List Fail!");
      return;
    }
  }
}
  
}  // namespace vsync
}  // namespace ndn
