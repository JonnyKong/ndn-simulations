/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_NODE_HPP_
#define NDN_VSYNC_NODE_HPP_

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>

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

 private:

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  inline void SendSyncInterest();
  inline void SendDataInterest(const NodeID& node_id, uint64_t start_seq, uint64_t end_seq);
  inline void SendSyncReply(const Name& n);

  void OnSyncInterest(const Interest& interest);
  void OnDataInterest(const Interest& interest);
  void OnRemoteData(const Data& data);

  Face& face_;
  KeyChain& key_chain_;

  const NodeID nid_;
  Name prefix_;
  const GroupID gid_;
  VersionVector version_vector_;

  //std::unordered_map<std::string, VersionVector> digest_log_;
  //VVQueue vector_data_queue_;

  //ViewInfo view_info_;

  // Hash table mapping node ID to its receive window
  std::unordered_map<NodeID, ReceiveWindow> recv_window_;

  // Ordered map mapping view id to a hash table that maps node id to its
  // version vector queue
  //std::map<ViewID, std::unordered_map<NodeID, VVQueue>> causality_graph_;

  //std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  std::vector<std::vector<std::shared_ptr<Data>>> data_store_;
  DataCb data_cb_;

  //std::random_device rdevice_;
  //std::mt19937 rengine_;
  //std::uniform_int_distribution<> rdist_;

};

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_NODE_HPP_
