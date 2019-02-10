/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_NODE_HPP_
#define NDN_VSYNC_NODE_HPP_

#include <functional>
#include <exception>
#include <map>
#include <deque>
#include <unordered_map>

#include "ndn-common.hpp"
#include "vsync-common.hpp"
#include "vsync-helper.hpp"
#include "recv-window.hpp"

#include "ns3/ndnSIM/NFD/daemon/table/pit.hpp"

using nfd::pit::Pit;

namespace ndn {
namespace vsync {

typedef struct {
  std::shared_ptr<Interest> interest;
  std::shared_ptr<Data> data;

  enum PacketType { INTEREST_TYPE, DATA_TYPE } packet_type;
  enum SourceType { ORIGINAL, FORWARDED, SUPPRESSED } packet_origin;  /* Used in data only */

  int64_t last_sent;
} Packet;

class Node {
public:
  /* Dependence injection: callback for application */
  using DataCb = std::function<void(const VersionVector& vv)>;

  /* typedef */
  enum DataType : uint32_t {
    kUserData       = 0,
    kGeoData        = 1,
    kSyncReply      = 9668,
    kConfigureInfo  = 9669,
    kVectorClock    = 9670
  };
 
  using GetCurrentPos = std::function<double()>;
  using GetCurrentPit = std::function<Pit&()>;
  using GetNumSorroundingNodes = std::function<int()>;

  class Error : public std::exception {
   public:
    Error(const std::string& what) : what_(what) {}
    virtual const char* what() const noexcept override { return what_.c_str(); }
   private:
    std::string what_;
  };

  /* For user application */
  Node(Face &face, Scheduler &scheduler, KeyChain &key_chain, const NodeID &nid,
       const Name &prefix, DataCb on_data, GetCurrentPos getCurrentPos, 
       GetCurrentPit getCurrentPit, GetNumSorroundingNodes getNumSorroundingNodes);

  void PublishData(const std::string& content, uint32_t type = kUserData);

private:
  /* Node properties */
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;
  Face& face_;
  Scheduler& scheduler_;
  KeyChain& key_chain_;
  const NodeID nid_;            /* To be configured by application */
  Name prefix_;                 /* To be configured by application */
  std::random_device rdevice_;
  std::mt19937 rengine_;

  /* Node states */
  VersionVector version_vector_;
  std::unordered_map<Name, std::shared_ptr<const Data>> data_store_;
  bool generate_data;           /* If false, PubishData() returns immediately */
  unsigned int notify_time;     /* No. of retx left for same sync interest */
  unsigned int left_retx_count; /* No. of retx left for same data interest */
  std::deque<Packet> pending_sync_interest; /* Multi-level queue */
  std::deque<Packet> pending_packet;
  Name waiting_data;            /* Name of outstanding data interest from pending_interest queue */
  std::unordered_map<NodeID, EventId> one_hop;              /* Nodes within one-hop distance */
  std::unordered_map<Name, EventId> overheard_sync_interest;/* For sync ack suppression */  
  bool isStatic;                /* Static nodes don't generate data or log store */

  /* Callbacks */
  DataCb data_cb_;              /* Never used in simulation */
  GetCurrentPos getCurrentPos_; 
  GetCurrentPit getCurrentPit_;
  GetNumSorroundingNodes getNumSorroundingNodes_;

  /* Node statistics */
  unsigned int retx_sync_interest;    /* No of retx for sync interest */
  unsigned int retx_data_interest;    /* No of retx for data interest */
  unsigned int retx_bundled_interest; /* No of retx for bundled data interest */
  unsigned int received_sync_interest;    /* No of sync interest received */
  unsigned int suppressed_sync_interest;  /* No of sync interest suppressed */
  unsigned int should_receive_interest;   /* No of interest should received receiver side */
  unsigned int received_interest;     /* No of interest (sync + data) received */

  /* Helper functions */
  void StartSimulation();
  void PrintNDNTraffic();
  void logDataStore(const Name& name);                  /* Logger */
  void logStateStore(const NodeID& nid, int64_t seq);   /* Logger */

  /* Packet processing pipeline */
  /* Unified queue for outgoing interest */
  void AsyncSendPacket();

  /* 1. Sync packet processing */
  void SendSyncInterest();
  void OnSyncInterest(const Interest &interest);
  void SendSyncAck(const Name &n);
  void OnSyncAck(const Data &ack);
  void OnNotifyDTTimeout();
  void OnNotifyWTTimeout();

  /* 2. Data packet processing */
  void SendDataInterest();
  void OnDataInterest(const Interest &interest);
  void SendDataReply();
  void OnDataReply(const Data &data, Packet::SourceType sourceType);
  EventId wt_data_interest; /* Event for sending next data interest */

  /* 3. Bundled data packet processing */
  void SendBundledDataInterest();
  void OnBundledDataInterest(const Interest &interest);
  void SendBundledDataReply();
  void OnBundledDataReply(const Data &data);

  /* 4. Pro-active events (beacons and sync interest retx) */
  void RetxSyncInterest();
  void SendBeacon();
  void OnBeacon(const Interest &beacon);
  EventId retx_event;   /* Event for retx next sync intrest */
  EventId beacon_event; /* Event for retx next beacon */
};

} // namespace vsync
} // namespace ndn

#endif  // NDN_VSYNC_NODE_HPP_