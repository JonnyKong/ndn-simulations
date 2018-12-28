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
  const NodeID nid_;            /* To be configured by application */
  Name prefix_;                 /* To be configured by application */
  Face& face_;
  KeyChain& key_chain_;
  std::mt19937 rengine_;

  /* Node states */
  VersionVector version_vector_;
  std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  bool generate_data;           /* If false, PubishData() returns immediately */
  Name pending_sync_notify;     /* Sync interest name sent or will be sent */
  Name waiting_sync_notify;     /* Sync interest name sent but not yet ACKed */
  std::shared_ptr<Data> ack;    /* ACK packet to be sent */
  unsigned int notify_time;     /* No. of retx left for same sync interest */
  unsigned int left_retx_count; /* No. of retx left for same data interest */
  bool vv_updated;              /* Whether state have updated since last sync interest sent */
  std::queue<std::queue<Name>> pending_interest;  /* Queue of unsatisfied interests */
  Name waiting_data;            /* Name of outstanding data interest from pending_interest queue */
  std::unordered_map<NodeID, ReceiveWindow> recv_window;  /* Record received data for logging */

  /* Node statistics */
  unsigned int data_num;              /* Number of data this node generated */
  unsigned int retx_sync_interest;    /* No of retx for sync interest */
  unsigned int retx_data_interest;    /* No of retx for data interest */
  unsigned int retx_bundled_interest; /* No of retx for bundled data interest */

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
  EventId wt_notify;                   /* Send sync interest wait timer */
  EventId dt_notify;                   /* Send sync interest delay timer */
  void OnNotifyDTTimeout();
  void OnNotifyWTTimeout();

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