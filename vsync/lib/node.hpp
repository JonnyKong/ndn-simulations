/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_NODE_HPP_
#define NDN_VSYNC_NODE_HPP_

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "ndn-common.hpp"
#include "recv-window.hpp"
#include "vsync-common.hpp"
#include "vsync-helper.hpp"

namespace ndn {
namespace vsync {

class Node {
 public:
  using DataCb =
      std::function<void(const VersionVector& vv)>;

  enum DataType : uint32_t {
    kUserData = 0,
    kGeoData  = 1,
    kSyncReply = 9668,
    kConfigureInfo = 9669,
    kVectorClock = 9670,
  };

  enum NodeState : uint32_t {
    kActive = 0,
    kIntermediate = 1,
    kSleeping = 2,
  };

  class Error : public std::exception {
   public:
    Error(const std::string& what) : what_(what) {}

    virtual const char* what() const noexcept override { return what_.c_str(); }

   private:
    std::string what_;
  };

  /**
   * @brief 
   *
   * @param face       Reference to the Face object on which the node runs
   * @param scheduler  Reference to the scheduler associated with @p face
   * @param key_chain  Reference to the KeyChain object used by the application
   * @param nid        Unique node ID (index in the vector clock)
   * @param prefix     Data prefix of the node
   * @param gid        group_id of the node
   * @param gsize      the group size (the size of the vector clock)
   * @param on_data    Callback for notifying new data to the application
   */
  Node(Face& face, Scheduler& scheduler, KeyChain& key_chain, const NodeID& nid,
       const Name& prefix, const GroupID& gid, const uint64_t group_size, DataCb on_data);

  const NodeID& GetNodeID() const { return nid_; };

  void PublishData(const std::string& content, uint32_t type = kUserData);

  void SyncData(const NodeID& syncup_node);

  double GetEnergyConsumption() {
    return energy_consumption;
  }

  double GetSleepingTime() {
    return sleeping_time;
  }

  std::vector<uint64_t> GetDataSnapshots() {
    return data_snapshots;
  }

  std::vector<VersionVector> GetVVSnapshots() {
    return vv_snapshots;
  }

 private:

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  struct MissingData {
    NodeID node_id;
    uint64_t start_seq;
    uint64_t end_seq;
    MissingData(NodeID node_id_, uint64_t start_seq_, uint64_t end_seq_) {
      node_id = node_id_;
      start_seq = start_seq_;
      end_seq = end_seq_;
    }
  };

  inline void SendSyncInterest(const Name& sync_interest_name, const uint32_t& sync_interest_time);
  inline void SendDataInterest(const NodeID& node_id, uint64_t start_seq, uint64_t end_seq);

  void OnSyncInterest(const Interest& interest);
  void OnDataInterest(const Interest& interest);
  void OnRemoteData(const Data& data);

  Face& face_;
  KeyChain& key_chain_;

  const NodeID nid_;
  Name prefix_;
  const GroupID gid_;
  uint32_t group_size;

  VersionVector version_vector_;
  std::vector<std::vector<std::shared_ptr<Data>>> data_store_;
  DataCb data_cb_;

  //std::unordered_set<NodeID> received_reply;
  std::unordered_map<NodeID, uint64_t> received_reply;
  Scheduler& scheduler_;
  NodeState node_state;
  double energy_consumption;
  double sleeping_time;
  std::vector<uint64_t> data_snapshots;
  std::vector<VersionVector> vv_snapshots;
  time::system_clock::time_point sleep_start;
  std::vector<MissingData> missing_data;
  NodeID current_sync_sender;

  EventId sync_interest_scheduler;
  EventId syncACK_scheduler;
  EventId data_interest_scheduler;

  bool receive_first_data_interest;
  bool is_syncup_node;
  bool is_syncing;
  NodeState lastState;

  uint32_t index;

  // functions for sleeping mechanisms
  inline void MakeDecision(uint32_t make_decision_time);
  inline void OnProbeIntermediateInterest(const Interest& interest);
  inline void SendProbeInterest();
  inline void SendReplyInterest();
  inline void OnProbeInterest(const Interest& interest);
  inline void OnReplyInterest(const Interest& interest);
  inline void OnSleepCommandInterest(const Interest& interest);
  inline void CalculateReply();
  inline void SendSyncACKInterest(uint32_t syncACK_time, const Name& syncACK_name);
  inline void OnSyncACKInterest(const Interest& interest);
  inline void SyncInterestTimeout(const Name& sync_interest_name, const uint32_t& sync_interest_time);
  inline void RequestMissingData();
  inline void OnDataInterestTimeout();
  inline void CheckOneWakeupNode();

  // helper functions
  inline void PrintVectorClock();

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;

};

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_NODE_HPP_
