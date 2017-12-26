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

NS_LOG_COMPONENT_DEFINE ("ndn.TestTransmissionTime");

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

  Ptr<UniformRandomVariable> randomizer0 = CreateObject<UniformRandomVariable> ();
  randomizer0->SetAttribute ("Min", DoubleValue (0));
  randomizer0->SetAttribute ("Max", DoubleValue (0));

  Ptr<UniformRandomVariable> randomizer50 = CreateObject<UniformRandomVariable> ();
  randomizer50->SetAttribute ("Min", DoubleValue (50));
  randomizer50->SetAttribute ("Max", DoubleValue (50));  

  MobilityHelper mobility1;
  mobility1.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", PointerValue (randomizer0),
                                 "Y", PointerValue (randomizer0),
                                 "Z", PointerValue (randomizer0));

  mobility1.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  MobilityHelper mobility2;
  mobility2.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", PointerValue (randomizer50),
                                 "Y", PointerValue (randomizer50),
                                 "Z", PointerValue (randomizer0));

  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");  

  NodeContainer nodes;
  nodes.Create (2);

  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, nodes);

  // 2. Install Mobility model
  mobility1.Install (nodes.Get(0));
  mobility2.Install (nodes.Get(1));

  // 3. Install NDN stack
  NS_LOG_INFO ("Installing NDN stack");
  StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback (MyNetDeviceFaceCallback));
  //ndnHelper.SetDefaultRoutes (true);
  ndnHelper.InstallAll();

  // 4. Set Forwarding Strategy
  //StrategyChoiceHelper::InstallAll("/ndn/geoForwarding", "/localhost/nfd/strategy/broadcast");
  //StrategyChoiceHelper::Install<nfd::fw::BroadcastStrategy>(nodes, "/");
  StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");

  // initialize the total vector clock

  // install SyncApp
  Ptr<MobilityModel> position1 = nodes.Get(0)->GetObject<MobilityModel>();
  Ptr<MobilityModel> position2 = nodes.Get(1)->GetObject<MobilityModel>();
  Vector pos1 = position1->GetPosition();
  Vector pos2 = position2->GetPosition();
  std::cout << "node 0 position: " << pos1.x << " " << pos1.y << std::endl;
  std::cout << "node 1 position: " << pos2.x << " " << pos2.y << std::endl;

  // install Consumer
  AppHelper testTransmissionTimeHelper0("testTransmissionTime");
  testTransmissionTimeHelper0.SetAttribute("NodeID", UintegerValue(0));
  testTransmissionTimeHelper0.Install(nodes.Get(0)).Start(Seconds(2));

  AppHelper testTransmissionTimeHelper1("testTransmissionTime");
  testTransmissionTimeHelper1.SetAttribute("NodeID", UintegerValue(1));
  testTransmissionTimeHelper1.Install(nodes.Get(1)).Start(Seconds(2));

  FibHelper::AddRoute(nodes.Get(0), "/ndn/testTransmissionTime", std::numeric_limits<int32_t>::max());
  FibHelper::AddRoute(nodes.Get(1), "/ndn/testTransmissionTime", std::numeric_limits<int32_t>::max());

  ////////////////

  Simulator::Stop (Seconds (100.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
