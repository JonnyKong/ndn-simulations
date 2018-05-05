#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

#include "sync-sleep-node.hpp"

namespace ns3 {
namespace ndn {

namespace vsync = ::ndn::vsync;

class SyncForSleepApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("SyncForSleepApp")
      .SetParent<Application>()
      .AddConstructor<SyncForSleepApp>()
      .AddAttribute("NodeID", "NodeID for sync node", UintegerValue(0),
                    MakeUintegerAccessor(&SyncForSleepApp::nid_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("Prefix", "Prefix for sync node", StringValue("/"),
                    MakeNameAccessor(&SyncForSleepApp::prefix_), MakeNameChecker())
      .AddAttribute("UseHeartbeat", "if use heartbeat", BooleanValue(true),
                    MakeBooleanAccessor(&SyncForSleepApp::useHeartbeat_), MakeBooleanChecker())
      .AddAttribute("UseFastResync", "if use fast resync", BooleanValue(true),
                    MakeBooleanAccessor(&SyncForSleepApp::useFastResync_), MakeBooleanChecker())
      .AddAttribute("HeartbeatTimer", "HeartbeatTimer", UintegerValue(5),
                    MakeUintegerAccessor(&SyncForSleepApp::heartbeatTimer_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("DetectPartitionTimer", "DetectPartitionTimer", UintegerValue(20),
                    MakeUintegerAccessor(&SyncForSleepApp::detectPartitionTimer_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("UseHeartbeatFlood", "if use heartbeat flood", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useHeartbeatFlood_), MakeBooleanChecker())
      .AddAttribute("UseBeacon", "if use beacon", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useBeacon_), MakeBooleanChecker())
      .AddAttribute("UseBeaconSuppression", "if use suppression for beacon", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useBeaconSuppression_), MakeBooleanChecker());

      

    return tid;
  }

double
GetCurrentPosition() {
  double cur_pos = GetNode()->GetObject<MobilityModel>()->GetPosition().x;
  //std::cout << "App " << m_appId << " on Node " << GetNode()->GetId() << " connected to " << dest << std::endl;
  return cur_pos;
}

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::sync_for_sleep::SimpleNode(nid_, prefix_, std::bind(&SyncForSleepApp::GetCurrentPosition, this),
      useHeartbeat_, useFastResync_, heartbeatTimer_, detectPartitionTimer_, useHeartbeatFlood_, useBeacon_, useBeaconSuppression_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::sync_for_sleep::SimpleNode> m_instance;
  vsync::NodeID nid_;
  Name prefix_;
  bool useHeartbeat_;
  bool useFastResync_;
  uint64_t heartbeatTimer_;
  uint64_t detectPartitionTimer_;
  bool useHeartbeatFlood_;
  bool useBeacon_;
  bool useBeaconSuppression_;
};

} // namespace ndn
} // namespace ns3
