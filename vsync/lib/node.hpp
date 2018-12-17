/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_NODE_HPP_
#define NDN_VSYNC_NODE_HPP_

#include "ndn-common.hpp"
#include "vsync-common.hpp"
#include "vsync-helper.hpp"
#include "recv-window.hpp"

namespace ndn {
namespace vsync {

class Node {
public:
  /* Dependence injection: callback for application */
  using DataCb = std::function<void(const VersionVector& vv)>;

  /* Public */
  Node(Face &face, Scheduler &scheduler, Keychain &key_chain, const NodeID &nid,
       const Name &prefix, DataCb on_data);

  void PublishData(const std::string& content, uint32_t type = kUserData);

private:
  /* Node properties */
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  const NodeID nid_;      /* Configured by application */
  Name prefix_;           /* Configured by application */
  Face& face_;
  KeyChain& key_chain_;

  /* Node states */
  VersionVector version_vector_;
  std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  bool generate_data;       /* If false, PubishData() returns immediately */
  Name pending_sync_notify; /* Sync interest name sent or will be sent */
  Name waiting_sync_notify; /* Sync interest name sent but not yet ACKed */
  unsigned int notify_time; /* No. of retx left for same sync interest */

  /* Node statistics */
  unsigned int data_num;    /* Number of data this node generated */

  /* Helper functions */
  void StartSimulation();
  void PrintNDNTraffic();
  void logDataStore(const Name& name);                  /* Logger */
  void logStateStore(const NodeID& nid, int64_t seq);   /* Logger */

  /* Packet processing pipeline */
  /* 1. Sync packet processing */
  void SendSyncInterest();
  void OnSyncInterest(const Interest &interest);
  void SendSyncAck();
  void onSyncAck(const Data &data);

  /* 2. Data packet processing */
  void SendDataInterest();
  void OnDataInterest(const Interest &interest);
  void SendDataReply();
  void onDataReply(const Data &data);

  /* 3. Bundled data packet processing */
  void SendBundledDataInterest();
  void OnBundledDataInterest(const Interest &interest);
  void SendBundledDataReply();
  void onBundledDataReply(const Data &data);

  /* 4. Beacons */
  void SendBeacon();
  void onBeacon(const Interest &beacon);
};

} // namespace vsync
} // namespace ndn