#pragma once

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

#include "ns3/ndnSIM/NFD/daemon/table/pit.hpp"

#include "sync-sleep-node.hpp"

using nfd::pit::Pit;

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
      // .AddAttribute("UseHeartbeat", "if use heartbeat", BooleanValue(true),
      //               MakeBooleanAccessor(&SyncForSleepApp::useHeartbeat_), MakeBooleanChecker())
      // .AddAttribute("UseHeartbeatFlood", "if use heartbeat flood", BooleanValue(false),
      //               MakeBooleanAccessor(&SyncForSleepApp::useHeartbeatFlood_), MakeBooleanChecker())
      .AddAttribute("UseBeacon", "if use beacon", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useBeacon_), MakeBooleanChecker())
      .AddAttribute("UseBeaconSuppression", "if use suppression for beacon", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useBeaconSuppression_), MakeBooleanChecker())
      .AddAttribute("UseRetx", "if use retx for sync notify", BooleanValue(false),
                    MakeBooleanAccessor(&SyncForSleepApp::useRetx_), MakeBooleanChecker());
      // .AddAttribute("UseBeaconFlood", "if beacon flood", BooleanValue(false),
      //               MakeBooleanAccessor(&SyncForSleepApp::useBeaconFlood_), MakeBooleanChecker());
    return tid;
  }

  double
  GetCurrentPosition() {
    double cur_pos = GetNode()->GetObject<MobilityModel>()->GetPosition().x;
    //std::cout << "App " << m_appId << " on Node " << GetNode()->GetId() << " connected to " << dest << std::endl;
    return cur_pos;
  }

  Pit &
  GetCurrentPIT() {
    Ptr<L3Protocol> protoNode = L3Protocol::getL3Protocol(GetNode());
    return protoNode -> getForwarder() -> getPit();
  }

  int GetNumSurroundingNodes_() {
    int num = 0;
    for (NodeContainer::Iterator i = container_->Begin(); i != container_->End(); ++i) {
      if (*i == GetNode())
        continue;
      double distance = (GetNode() -> GetObject<MobilityModel>())
                        -> GetDistanceFrom((*i) -> GetObject<MobilityModel>());
      if (distance <= wifi_range + 1e-3)
        num += 1;
    }
    return num;
  }

  ::std::shared_ptr<::nfd::Face>
  GetFaceById(int id) {
    Ptr<L3Protocol> protoNode = L3Protocol::getL3Protocol(GetNode());
    return protoNode -> getFaceById(id);
  }

  NodeContainer *container_;  // Point back to container in simulator
  float wifi_range;           // Wifi range from simulator


protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::sync_for_sleep::SimpleNode(
      nid_, 
      prefix_, 
      std::bind(&SyncForSleepApp::GetCurrentPosition, this),
      std::bind(&SyncForSleepApp::GetCurrentPIT, this),
      std::bind(&SyncForSleepApp::GetNumSurroundingNodes_, this),
      std::bind(&SyncForSleepApp::GetFaceById, this, _1),
      useBeacon_, 
      useRetx_
    ));
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
  bool useBeacon_;
  bool useBeaconSuppression_;
  bool useRetx_;
};

} // namespace ndn
} // namespace ns3
