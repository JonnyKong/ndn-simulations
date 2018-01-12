#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include "broadcast_strategy.hpp"

#include <map>

using namespace std;
using namespace ns3;

using ns3::ndn::StackHelper;
using ns3::ndn::AppHelper;
using ns3::ndn::StrategyChoiceHelper;
using ns3::ndn::L3RateTracer;
using ns3::ndn::FibHelper;

NS_LOG_COMPONENT_DEFINE ("ndn.SyncForSleep");

//
// DISCLAIMER:  Note that this is an extremely simple example, containing just 2 wifi nodes communicating
//              directly over AdHoc channel.
//

// Ptr<ndn::NetDeviceFace>
// MyNetDeviceFaceCallback (Ptr<Node> node, Ptr<ndn::L3Protocol> ndn, Ptr<NetDevice> device)
// {
//   // NS_LOG_DEBUG ("Create custom network device " << node->GetId ());
//   Ptr<ndn::NetDeviceFace> face = CreateObject<ndn::MyNetDeviceFace> (node, device);
//   ndn->AddFace (face);
//   return face;
// }

int
main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("OfdmRate24Mbps"));

  CommandLine cmd;
  cmd.Parse (argc,argv);

  //////////////////////
  //////////////////////
  //////////////////////
  WifiHelper wifi = WifiHelper::Default ();
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate24Mbps"));

  YansWifiChannelHelper wifiChannel;// = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  //YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannel.Create ());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(15));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(15));


  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  Ptr<UniformRandomVariable> randomizer = CreateObject<UniformRandomVariable> ();
  randomizer->SetAttribute ("Min", DoubleValue (0));
  randomizer->SetAttribute ("Max", DoubleValue (50));

  Ptr<UniformRandomVariable> randomizerZ = CreateObject<UniformRandomVariable> ();
  randomizerZ->SetAttribute ("Min", DoubleValue (0));
  randomizerZ->SetAttribute ("Max", DoubleValue (0));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", PointerValue (randomizer),
                                 "Y", PointerValue (randomizer),
                                 "Z", PointerValue (randomizerZ));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  NodeContainer nodes;
  nodes.Create (10);

  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, nodes);

  // 2. Install Mobility model
  mobility.Install (nodes);

  // 3. Install NDN stack
  NS_LOG_INFO ("Installing NDN stack");
  StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback (MyNetDeviceFaceCallback));
  //ndnHelper.SetDefaultRoutes (true);
  ndnHelper.InstallAll();

  // 4. Set Forwarding Strategy
  //StrategyChoiceHelper::InstallAll("/ndn/geoForwarding", "/localhost/nfd/strategy/broadcast");
  // StrategyChoiceHelper::Install<nfd::fw::BroadcastStrategy>(nodes, "/");
  StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");

  // initialize the total vector clock

  // install SyncApp
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
    Ptr<Node> object = *i;
    Ptr<MobilityModel> position = object->GetObject<MobilityModel>();
    Vector pos = position->GetPosition();
    std::cout << "node " << idx << " position: " << pos.x << " " << pos.y << std::endl;

    AppHelper syncForSleepAppHelper("SyncForSleepApp");
    syncForSleepAppHelper.SetAttribute("GroupID", StringValue("group0"));
    syncForSleepAppHelper.SetAttribute("NodeID", UintegerValue(idx));
    syncForSleepAppHelper.SetAttribute("Prefix", StringValue("/"));
    syncForSleepAppHelper.SetAttribute("GroupSize", UintegerValue(10));
    auto app = syncForSleepAppHelper.Install(object);
    app.Start(Seconds(2));
    app.Stop(Seconds (2100.0 + idx));

    StackHelper::setNodeID(idx, object);
    FibHelper::AddRoute(object, "/ndn/sleepingProbe/group0", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/sleepingReply/group0", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/vsync/group0", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/vsyncData/group0", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/sleepingCommand/group0", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/syncACK/group0", std::numeric_limits<int32_t>::max());
    idx++;
  }

  ////////////////

  Simulator::Stop (Seconds (2120.0));

  // L3RateTracer::InstallAll("test-rate-trace.txt", Seconds(0.5));
  // L2RateTracer::InstallAll("drop-trace.txt", Seconds(0.5));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
