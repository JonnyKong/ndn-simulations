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
#include "vsync-common.hpp"
#include "vsync-helper.hpp"
#include "recv-window.hpp"

namespace ndn {
namespace vsync {

class Node {
 public:
  using DataCb =
      std::function<void(const VersionVector& vv)>;

  using GetCurrentPos =
      std::function<double()>;

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

  struct HistoryInfo {
    std::vector<int> update_node_count;
    int cur_update_node_count = 0;
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
       const Name& prefix, DataCb on_data, GetCurrentPos getCurrentPos,
       bool useHeartbeat, bool useFastResync, uint64_t heartbeatTimer, uint64_t detectPartitionTimer);

  const NodeID& GetNodeID() const { return nid_; };

  void PublishData(const std::string& content, uint32_t type = kUserData);

  void SendSyncNotify();

  std::vector<uint64_t> GetDataSnapshots() {
    return data_snapshots;
  }

  std::vector<VersionVector> GetVVSnapshots() {
    return vv_snapshots;
  }

  std::vector<std::unordered_map<NodeID, ReceiveWindow>> GetRWSnapshots() {
    return rw_snapshots;
  }

  uint64_t GetCollisionNum() {
    return collision_num;
  }

  uint64_t GetSuppressionNum() {
    return suppression_num;
  }

  uint64_t GetOutInterestNum() {
    return out_interest_num;
  }

 private:

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  Face& face_;
  KeyChain& key_chain_;

  const NodeID nid_;
  Name prefix_;
  Scheduler& scheduler_;

  VersionVector version_vector_;
  VersionVector heartbeat_vector_;
  std::unordered_map<NodeID, EventId> detect_partition_timer;
  std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  std::unordered_map<NodeID, ReceiveWindow> recv_window;
  DataCb data_cb_;
  GetCurrentPos get_current_pos_;

  std::vector<uint64_t> data_snapshots;
  std::vector<VersionVector> vv_snapshots;
  std::vector<std::unordered_map<NodeID, ReceiveWindow>> rw_snapshots;

  uint64_t collision_num;
  uint64_t suppression_num;
  uint64_t out_interest_num;
  int data_num;
  bool generate_data;

  std::unordered_map<Name, int> pending_interest;
  Name pending_sync_notify;
  Name waiting_sync_notify;
  int notify_time;
  EventId wt_notify;
  // Name pending_bundled_interest;
  std::unordered_map<Name, int> pending_bundled_interest;
  EventId inst_dt;
  std::unordered_map<Name, EventId> wt_list;
  bool in_dt;

  // retx and sync timer
  EventId heartbeat_scheduler;
  EventId adjust_heartbeat_scheduler;
  HistoryInfo history_info;
  Name latest_data;

  // heartbeat
  bool kHeartbeat;
  time::seconds kHeartbeatTimer;
  time::seconds kDetectPartitionTimer;
  // fast resync
  bool kFastResync;
  
  // functions for sync-responder
  void OnIncomingSyncInterest(const Interest& interest);
  void OnSyncNotify(const Interest& interest);
  void FastExchangeMissingData(const Name& sync_notify_name);
  void FetchMissingData();
  void OnSyncNotifyRetxTimeout();
  void OnSyncNotifyBeaconTimeout();
  void OnDetectPartitionTimeout(NodeID node_id);
  inline void SendDataInterest();
  void OnDataInterest(const Interest& interest);
  void OnDTTimeout();
  void OnWTTimeout(const Name& name, int cur_transmission_time);
  void OnRemoteData(const Data& data);
  void onNotifyACK(const Data& ack);

  // helper functions
  inline void StartSimulation();
  inline void PrintVectorClock();
  inline void PrintNDNTraffic();
  inline void OnSyncNotifyData(const Data& data);
  inline void logDataStore(const Name& name);
  inline void AdjustHeartbeatTimer();

  inline void SendHeartbeat();
  inline void UpdateHeartbeat();
  // inline void OnIncomingPacketNotify(const Interest& interest);

  // for fast recovery
  inline void SendBundledDataInterest(const NodeID& recv_id, VersionVector mv);
  inline void OnBundledDataInterest(const Interest& interest);
  inline void OnBundledData(const Data& data);

  std::random_device rdevice_;
  std::mt19937 rengine_;

  // detect the average time to meet a new node
  int count = 0;
  int64_t last;
  double total_time = 0;
};

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_NODE_HPP_
