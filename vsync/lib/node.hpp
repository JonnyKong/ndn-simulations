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

  void SyncData();

  double GetEnergyConsumption() {
    return energy_consumption;
  }

  uint64_t GetSleepingTime() {
    return sleeping_time;
  }

  std::vector<uint64_t> GetDataSnapshots() {
    return data_snapshots;
  }

  std::vector<VersionVector> GetVVSnapshots() {
    return vv_snapshots;
  }

  std::vector<std::vector<ReceiveWindow>> GetRWSnapshots() {
    return rw_snapshots;
  }

  std::vector<std::pair<double, int>> ReceiveFirstSyncACKDelay() {
    /*
    double total = 0.0;
    for (auto delay: receive_first_syncACK_delay) {
      total += delay;
    }
    return total / sync_num;
    */
    return receive_first_syncACK_delay;
  }

  std::vector<std::pair<double, int>> ReceiveLastSyncACKDelay() {
    /*
    double total = 0.0;
    for (auto delay: receive_last_syncACK_delay) {
      total += delay;
    }
    return total / sync_num;
    */
    return receive_last_syncACK_delay;
  }

  std::string GetOutVsyncInfo() {
    return outVsyncInfo;
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

  std::vector<uint64_t> GetActiveRecord() {
    return active_record;
  }

  double GetSyncDelay() {
    double delay = 0.0;
    for (auto entry: sync_delay) {
      delay += entry;
    }
    return delay;
  }

  double GetWorkingTime() {
    return working_time;
  }

 private:

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  Face& face_;
  KeyChain& key_chain_;

  const NodeID nid_;
  Name prefix_;
  const GroupID gid_;
  uint32_t group_size;
  Scheduler& scheduler_;

  VersionVector version_vector_;
  std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  std::vector<ReceiveWindow> recv_window;
  DataCb data_cb_;
  NodeState node_state;
  double energy_consumption;
  time::system_clock::time_point sleep_start;
  time::system_clock::time_point wakeup;
  uint64_t sleeping_time;
  double working_time;
  uint32_t time_slot;

  std::vector<uint64_t> data_snapshots;
  std::vector<VersionVector> vv_snapshots;
  std::vector<std::vector<ReceiveWindow>> rw_snapshots;
  std::string outVsyncInfo;
  uint64_t collision_num;
  uint64_t suppression_num;
  uint64_t out_interest_num;
  std::vector<uint64_t> active_record;

  // state for sync-responder
  std::vector<std::pair<Name, int>> pending_interest;
  bool sync_responder_success;
  bool receive_sync_interest;
  // timers for sync-responder interests
  EventId inst_wt;
  EventId inst_dt;


  // state for sync-requester
  bool receive_ack_for_sync_interest;
  std::unordered_set<uint64_t> receive_syncACK_responder;
  bool sync_requester;
  time::system_clock::time_point send_sync_interest_time;
  std::vector<std::pair<double, int>> receive_first_syncACK_delay;
  std::vector<std::pair<double, int>> receive_last_syncACK_delay;
  std::vector<double> sync_delay;
  double sync_num;
  // timers for sync-responder interests
  EventId sync_interest_scheduler;
  EventId sync_duration_scheduler;

  // functions for sleeping scheduling
  inline void EnterIntermediateState();
  inline void CheckState();
  inline void Reset();

  // functions for sync-requester
  inline void OnIncomingSyncACKInterest(const Interest& interest);
  inline void OnSyncDurationTimeOut();
  inline void SendSyncInterest(const Name& sync_interest_name, const uint32_t& sync_interest_time);
  inline void SyncInterestTimeout(const Name& sync_interest_name, const uint32_t& sync_interest_time);
  inline void OnSyncACKInterest(const Interest& interest);

  // functions for sync-responder
  inline void OnIncomingData(const Interest& interest);
  inline void OnIncomingInterest(const Interest& interest);
  inline void SendInterest();
  void OnSyncInterest(const Interest& interest);
  void OnDataInterest(const Interest& interest);
  void OnRemoteData(const Data& data);
  inline void OnDataForSyncack(const Data& data);

  // helper functions
  inline void StartSimulation();
  inline void SendGetOutVsyncInfoInterest();
  inline void PrintVectorClock();
  inline void ReceiveInterest();
  inline void ReceiveData();

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;

};

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_NODE_HPP_
