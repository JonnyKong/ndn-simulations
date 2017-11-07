#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include "broadcast_strategy.hpp"
#include "ns3/random-variable-stream.h"

using namespace std;
using namespace ns3;

using ns3::ndn::StackHelper;
using ns3::ndn::AppHelper;
using ns3::ndn::StrategyChoiceHelper;
using ns3::ndn::L3RateTracer;
using ns3::ndn::FibHelper;

NS_LOG_COMPONENT_DEFINE ("ndn.GeoForwarding");

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

static inline
double dist(uint32_t Xsrc, uint32_t Ysrc, uint32_t Xdest, uint32_t Ydest)
{
  return static_cast<double>((Xsrc - Xdest) * (Xsrc - Xdest) + (Ysrc - Ydest) * (Ysrc - Ydest));
}

static inline
double calculateSD(uint32_t src, uint32_t self, uint32_t dest) {
  uint32_t Xself = (self / 10000) * 10;
  uint32_t Yself = (self % 10000) * 10;
  uint32_t Xsrc = (src / 10000) * 10;
  uint32_t Ysrc = (src % 10000) * 10;
  uint32_t Xdest = (dest / 10000) * 10;
  uint32_t Ydest = (dest % 10000) * 10;
  return static_cast<double>((dist(Xsrc, Ysrc, Xself, Yself) +
                              dist(Xsrc, Ysrc, Xdest, Ydest) -
                              dist(Xself, Yself, Xdest, Ydest)) / 
                             (2 * sqrt(dist(Xsrc, Ysrc, Xdest, Ydest)) * sqrt(dist(Xsrc, Ysrc, Xself, Yself)))
                            );
}

int
main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (100));
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
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(2));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(2));


  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (30.0),
                                 "DeltaY", DoubleValue (30.0),
                                 "GridWidth", UintegerValue (32),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  NodeContainer nodes;
  nodes.Create (1024);

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
  StrategyChoiceHelper::Install<nfd::fw::BroadcastStrategy>(nodes, "/ndn");


  // 5. Set Geo_tag on nfd
  //    and record the consumer&producer's position
  //std::string consumer_position, producer_position;
  /* geo-forwarding */
  //std::string dest_batch = "1000100 1800000 1800180 1000100 1800000 1800180 1000100 1800000";
  /* one node*/
  //std::string dest_batch = "1000100";
  /* with sleeping mode*/
  //std::string dest_batch = "1000100 1200120 1400140 1600160 1800180 1000120 1200100 1400160 1600140 1800160";


  // !!!!!!!!!!
  //std::string dest_batch = "3600360 2400240 1200160 1600120 800080 2800000 280 2800320 3200280 2000240 1200360";

  //std::string dest_batch = "4500450 9300930";
  std::string dest_batch = "1200120";
  auto now = ns3::ndn::time::steady_clock::now();
  ns3::ndn::time::steady_clock::TimePoint timelineStart = now + ns3::ndn::time::seconds(40000);

  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
    Ptr<Node> object = *i;
    Ptr<MobilityModel> position = object->GetObject<MobilityModel>();
    NS_ASSERT (position != 0);
    Vector pos = position->GetPosition();

    std::uint32_t node_position = static_cast<std::uint32_t>(pos.x * 10000 + pos.y);
    NS_LOG_INFO ("node position: " + std::to_string(node_position));

    double sd = calculateSD(0, node_position, 3800380);
    double d_range = 0.85;
    double t_fw_min = 10;
    double t_fw_max = 0.0;
    double t_gap = 100;
    if (sd > d_range) t_fw_max = t_fw_min * 2;
    else {
      t_fw_min *= 2;
      t_fw_max = t_fw_min + (d_range - sd) * t_gap;
    }
    Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
    //x->SetAttribute ("Min", DoubleValue (t_fw_min));
    //x->SetAttribute ("Max", DoubleValue (t_fw_max));
    // The values returned by a uniformly distributed random
    // variable should always be within the range
    //
    //     [min, max)  .
    //
    /*
    double value = 400 - x->GetValue (t_fw_min, t_fw_max) * 10;
    if (node_position == 0) value = 400;
    //std::cout << "node(" << pos.x / 20 << "," << pos.y / 20 << ") sd=" << sd << ", value=" << value << std::endl;
    int v = value;
    std::cout << pos.x << "," << pos.y << "," << v << std::endl;
    */
    
    /*
    if (node_position == 3000300) {
      AppHelper testHelper ("testRange");
      testHelper.Install (object).Start(Seconds(1));
      FibHelper::AddRoute(object, "/ndn/testRange", std::numeric_limits<int32_t>::max());
    }
    StackHelper::setGeoTag(node_position, object);
    
    */
    
    if (i == nodes.Begin()) {
      AppHelper geoConsumerHelper ("GeoConsumer");
      geoConsumerHelper.SetAttribute ("Position", StringValue(std::to_string(node_position)));
      geoConsumerHelper.SetAttribute ("DestBatch", StringValue(dest_batch));
      geoConsumerHelper.Install (object).Start(Seconds(1));
    }
    else {
      AppHelper geoProducerHelper ("GeoProducer");
      geoProducerHelper.SetAttribute ("Position", StringValue(std::to_string(node_position)));
      geoProducerHelper.SetAttribute ("PayLoad", StringValue("1024"));
      geoProducerHelper.Install (object).Start(Seconds(1));
    }
    FibHelper::AddRoute(object, "/ndn/geoForwarding", std::numeric_limits<int32_t>::max());
    StackHelper::setGeoTag(node_position, object);

    /*
    StackHelper::setInfoNum(value, object);
    StackHelper::setGeoTag(node_position, object);
    FibHelper::AddRoute(object, "/ndn/sleep_signal", std::numeric_limits<int32_t>::max());
    */
    StackHelper::setTimelineStart(timelineStart, object);
  }

  ////////////////

  Simulator::Stop (Seconds (30800.0));

  //L3RateTracer::InstallAll("rate-trace.txt", Seconds(0.5));
  //L2RateTracer::InstallAll("drop-trace.txt", Seconds(0.5));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}