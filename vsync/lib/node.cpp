/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <random>
#include <fstream>

#include <boost/log/trivial.hpp>

#include <ndn-cxx/util/digest.hpp>

#include "node.hpp"
#include "vsync-helper.hpp"
#include "logging.hpp"
#include "vector-clock-status.hpp"

VSYNC_LOG_DEFINE(SyncForSleep);

namespace ndn {
namespace vsync {

static time::milliseconds kReplyWaitingTime = time::milliseconds(1000);
static time::milliseconds kInterval = time::milliseconds(40000);
static time::milliseconds kSyncTime = time::milliseconds(1000);
static time::milliseconds kSnapshotInterval = time::milliseconds(3000);
static const std::string availabilityFileName = "availability.txt";
static const int kActiveInGroup = 2;
static const int kSnapshotNum = 65;

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
  energy_consumption = 0.0;
  sleeping_time = 0.0;
  current_sync_sender = 0;

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

  face_.setInterestFilter(
      Name(kSleepCommandPrefix).append(gid_), std::bind(&Node::OnSleepCommandInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register sleep command interest prefix: " << reason); 
        throw Error("Failed to register sleep command interest prefix: " + reason);
      });  

  face_.setInterestFilter(
      Name(kSyncACKPrefix).append(gid_), std::bind(&Node::OnSyncACKInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Failed to register sync ack interest prefix: " << reason); 
        throw Error("Failed to register sync ack interest prefix: " + reason);
      });

  scheduler_.scheduleEvent(time::milliseconds(4 * nid_ * 1000),
                           [this] { SendProbeInterest(); });

  scheduler_.scheduleEvent(time::milliseconds(3000), [this] { PrintVectorClock(); });
}

//OK
void Node::PublishData(const std::string& content, uint32_t type) {
  if (node_state != kActive) return;
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

  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Publish Data: d.name=" << n.toUri() << " d.type=" << type << " d.content=" << content);
}

void Node::SyncData() {
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Start to Sync Data");
  SendSyncInterest();
}

void Node::SendSyncInterest() {
  // make the sync interest name
  std::string vv_encode = EncodeVV(version_vector_);
  auto n = MakeSyncInterestName(gid_, nid_, vv_encode);

  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Send Sync Interest: i.name=" << n.toUri());

  Interest i(n, time::milliseconds(4000));
  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnSyncInterest(const Interest& interest) {
  if (node_state != kActive) return;
  const auto& n = interest.getName();
  // Check sync interest name size
  if (n.size() != kSyncPrefix.size() + 3) {
    VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Invalid sync interest name: " << n.toUri());
    return;
  }

  auto group_id = ExtractGroupID(n);
  auto sync_sender = ExtractNodeID(n);
  auto other_vv_str = ExtractEncodedVV(n);
  VersionVector other_vv = DecodeVV(other_vv_str);
  VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Recv Sync Interest: i.version_vector=" << VersionVectorToString(other_vv));
  if (other_vv.size() != version_vector_.size()) {
    VSYNC_LOG_TRACE("Different Version Vector Size in Group: " << gid_);
    return;
  }

  SendSyncReply(n);
  // Process version vector
  current_sync_sender = sync_sender;
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
      syncing_nid.insert(i);
    }
  }
  if (syncing_nid.empty()) {
    // send ack to tell the sync-interest-initiated node to go to sleep
    std::uniform_int_distribution<> rdist_ack(0, 500);
    sync_ack_scheduler = scheduler_.scheduleEvent(time::milliseconds(rdist_ack(rengine_)),
                                                  [this] { SendSyncACKInterest(); });
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
  // if node_state == kIntermediate, you should also process the interest!
  if (node_state == kSleeping) return;
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
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") sends the data: " << dl_encode);
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
  if (node_state == kSleeping) return;
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
      size_t start_index = version_vector_[node_id] + 1 - start_seq;
      
      std::string recv_data_list= "";
      for (int i = start_index; i < data_list.size(); ++i) {
        auto cur_name = MakeDataName(gid_, node_id, i);
        auto cur_type = data_list[i].first;
        auto cur_content = data_list[i].second;

        std::cout << "data type = " << cur_type << " data_content = " << cur_content << std::endl;
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
      
      syncing_nid.erase(node_id);
      if (syncing_nid.empty()) {
        std::uniform_int_distribution<> rdist_ack(0, 500);
        sync_ack_scheduler = scheduler_.scheduleEvent(time::milliseconds(rdist_ack(rengine_)),
                                 [this] { SendSyncACKInterest(); });
      }
      
    }
    else {
      VSYNC_LOG_TRACE("node(" << gid_ << " " << nid_ << ") Decode Data List Fail!");
      return;
    }
  }
}

void Node::SendSyncACKInterest() {
  Name n = MakeSyncACKInterestName(gid_, current_sync_sender);
  Interest i(n, time::milliseconds(1000));
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Send SyncACKInterest: i.name=" << n.toUri());

  face_.expressInterest(i, [](const Interest&, const Data&) {},
                        [](const Interest&, const lp::Nack&) {},
                        [](const Interest&) {});
}

void Node::OnSyncACKInterest(const Interest& interest) {
  const auto& n = interest.getName();

  if (node_state == kSleeping) return;
  else if (node_state == kActive) {
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Receive SyncACKInterest from other member: i.name=" << n.toUri());
    scheduler_.cancelEvent(sync_ack_scheduler);
    scheduler_.cancelEvent(current_sync_scheduler);
    return;
  }
  else if (node_state == kIntermediate) {
    NodeID received_node = ExtractNodeID(n);
    if (received_node != nid_) {
      VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Receive wrong SyncACKInterest: i.name=" << n.toUri());
      return;
    }

    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Receive SyncACKInterest: i.name=" << n.toUri());
    std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
    Interest i(kLocalhostSleepingCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    scheduler_.cancelEvent(current_sync_scheduler);
    node_state = kSleeping;
    sleep_start = time::system_clock::now();
  }
}

// sleeping mechanisms
void Node::SendProbeInterest() {
  if (node_state == kSleeping) {
    time::system_clock::time_point sleep_end = time::system_clock::now();
    std::time_t time_span = time::system_clock::to_time_t(sleep_end) - time::system_clock::to_time_t(sleep_start);
    std::cout << "node(" << gid_ << " " << nid_ << ") slept for " << time_span << " seconds." << std::endl;
    sleeping_time += time_span;
    /*
    std::chrono::high_resolution_clock::time_point sleep_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_span = sleep_end - sleep_start;
    std::cout << "node(" << gid_ << " " << nid_ << ") slept for " << time_span.count() << " seconds." << std::endl;
    sleeping_time += time_span.count();
    */
  }
  scheduler_.scheduleEvent(kInterval, [this] { SendProbeInterest(); });
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

void Node::SyncDataTimeOut(uint32_t sync_time) {
  if (sync_time == 4) return;
  SyncData();
  current_sync_scheduler = scheduler_.scheduleEvent(kSyncTime, [this, sync_time] {
    this->SyncDataTimeOut(sync_time + 1);
  });
}

void Node::CalculateReply() {
  /* we should guarantee that there should be at least two active nodes in the group. if so, the current 
  node can continue to go to sleep. Otherwise it should start to work.
  */
  size_t active_in_group = received_reply.size();
  std::string received_nodes = "";
  for (auto entry: received_reply) {
    received_nodes += to_string(entry.first) + ",";
  }
  std::cout << "node(" << gid_ << " " << nid_ << ") receives " << active_in_group << " replies: " <<  received_nodes << std::endl;
  if (active_in_group >= kActiveInGroup) {
    // we should choose the node with smallest sleeping time to go to sleep
    NodeID sleep_node = nid_;
    uint64_t smallest_sleeping_time = sleeping_time;
    for (auto entry: received_reply) {
      if (entry.second < smallest_sleeping_time) {
        sleep_node = entry.first;
        smallest_sleeping_time = entry.second;
      }
    }
    if (sleep_node == nid_) {
      SyncData();
      // we need to wait for some time to make the data synced before going to sleep
      
      current_sync_scheduler = scheduler_.scheduleEvent(kSyncTime, [this] {
        this->SyncDataTimeOut(2);
      });
      
      /*
      scheduler_.scheduleEvent(kSyncTime, [this] { 
        std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
        Interest i(kLocalhostSleepingCommand);
        face_.expressInterest(i, [](const Interest&, const Data&) {},
                              [](const Interest&, const lp::Nack&) {},
                              [](const Interest&) {});
        node_state = kSleeping;
        sleep_start = time::system_clock::now();
        // sleeping_time += kSleepInterval; // currently sleeping time is fixed, = 10 seconds
      });
      */
    }
    else {
      std::cout << "node(" << gid_ << " " << nid_ << ") will send sleep command to " << "node(" << gid_ << " " << sleep_node << ")" << std::endl;
      auto sleepCommandName = MakeSleepCommandName(gid_, sleep_node);
      Interest i(sleepCommandName);
      face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      node_state = kActive;
    }
  }
  else {
    // start to work
    // we have waken up the nfd in SendProbeInterest(), so no need to do again here
    /*
    Interest i(kLocalhostWakeupCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    */
    std::cout << "node(" << gid_ << " " << nid_ << ") will start to work" << std::endl;
    node_state = kActive;
  }
  // schedule to next round
  received_reply.clear();
}

void Node::OnProbeInterest(const Interest& interest) {
  if (node_state == kActive) {
    const auto& n = interest.getName();
    VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv probe interest: name=" << n.toUri());
    std::uniform_int_distribution<> rdist2_(0, 900);
    scheduler_.scheduleEvent(time::milliseconds(rdist2_(rengine_)),
                             [this] { SendReplyInterest(); });
  }
}

void Node::SendReplyInterest() {
  if (node_state == kActive) {
    auto n = MakeReplyInterestName(gid_, nid_, sleeping_time);
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
    auto sleeping_time = ExtractSleepingTime(n);
    received_reply[reply_node] = sleeping_time;
  }
}

void Node::OnSleepCommandInterest(const Interest& interest) {
  const auto& n = interest.getName();
  NodeID received_node = ExtractNodeID(n);
  if (received_node != nid_) return;
  VSYNC_LOG_TRACE( "node(" << gid_ << " " << nid_ << ") Recv sleep command interest: name=" << n.toUri());
  if (node_state != kActive) {
    std::cout << "problem 0 here!" << std::endl;
  }
  node_state = kIntermediate;
  // go to sleep
  SyncData();
  // we need to wait for some time to make the data synced before going to sleep
  /*
  scheduler_.scheduleEvent(kSyncTime, [this] { 
    std::cout << "node(" << gid_ << " " << nid_ << ") will go to sleep" << std::endl;
    Interest i(kLocalhostSleepingCommand);
    face_.expressInterest(i, [](const Interest&, const Data&) {},
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    node_state = kSleeping;
    sleep_start = time::system_clock::now();
  });
  */
  // we need to wait for some time to make the data synced before going to sleep
  current_sync_scheduler = scheduler_.scheduleEvent(kSyncTime, [this] {
    this->SyncDataTimeOut(2);
  });
  // sleeping_time += kSleepInterval; // currently sleeping time is fixed, = 10 seconds
}

// print the vector clock every 5 seconds
void Node::PrintVectorClock() {
  if (data_snapshots.size() == kSnapshotNum) return;
  data_snapshots.push_back(version_vector_[nid_]);
  if (node_state != kSleeping) {
    /*
    std::ofstream out;
    out.open(availabilityFileName, std::ofstream::out | std::ofstream::app);
    if (out.is_open()) {
      out << "node(" << gid_ << " " << nid_ << "): " << VersionVectorToString(version_vector_) << "\n";
      out.close();
    } 
    */  
    vv_snapshots.push_back(version_vector_);
  }
  else vv_snapshots.push_back(VersionVector(version_vector_.size(), 0));
  scheduler_.scheduleEvent(kSnapshotInterval, [this] { PrintVectorClock(); });
}
  
}  // namespace vsync
}  // namespace ndn
