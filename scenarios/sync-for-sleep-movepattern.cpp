#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/netanim-module.h"

#include <random>
#include <map>

using namespace std;
using namespace ns3;

using ns3::ndn::StackHelper;
using ns3::ndn::AppHelper;
using ns3::ndn::StrategyChoiceHelper;
using ns3::ndn::L3RateTracer;
using ns3::ndn::FibHelper;

// NS_LOG_COMPONENT_DEFINE ("ndn.SyncForSleep");

uint32_t MacTxDropCount, PhyTxDropCount, PhyRxDropCount, PhyRxBeginCount, PhyRxEndCount;

void
MacTxDrop(Ptr<const Packet> p)
{
  // NS_LOG_INFO("Packet Drop");
  MacTxDropCount++;
}

void
PrintDrop()
{
  std::cout << "******************** Collision Results ******************" << std::endl;
  std::cout << "MacTxDropCount: " << MacTxDropCount << std::endl;
  std::cout << "PhyTxDropCount: " << PhyTxDropCount << std::endl;
  std::cout << "PhyRxDropCount: " << PhyRxDropCount << std::endl;
  std::cout << "PhyRxBeginCount: " << PhyRxBeginCount << std::endl; 
  std::cout << "PhyRxEndCount: " << PhyRxEndCount << std::endl;
  std::cout << "******************** Collision Results ******************" << std::endl;
}

void
PhyTxDrop(Ptr<const Packet> p)
{
  // NS_LOG_INFO("Packet Drop");
  PhyTxDropCount++;
}
void
PhyRxDrop(Ptr<const Packet> p)
{
  // NS_LOG_INFO("Packet Drop");
  PhyRxDropCount++;
}
void
PhyRxBegin(Ptr<const Packet> p)
{
  PhyRxBeginCount++;
}
void
PhyRxEnd(Ptr<const Packet> p)
{
  PhyRxEndCount++;
}

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

// std::poisson_distribution<> data_generation_dist(data_generation_rate_mean);
// std::uniform_real_distribution<> dt_dist(0, kInterestDT);

static const int node_num = 40;
static const int generate_data_time = 900;
static const int sim_time = generate_data_time * 3;
static const int x_range = 800;
static const int y_range = 800;
static const int max_speed = 20;
// wifi-coverage is about 160

double getDistance(double x1, double y1, double x2, double y2) {
  return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

void installMobility(NodeContainer& c, bool constant_pause, int pause_time) {
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::WaypointMobilityModel");
  mobility.Install(c);

  /*
  std::random_device rdevice_;
  std::mt19937 rengine_(rdevice_());

  std::uniform_real_distribution<> x_dist(0, x_range);
  std::uniform_real_distribution<> y_dist(0, y_range);
  std::uniform_int_distribution<> speed_dist(1, max_speed);
  std::poisson_distribution<> pause_time_dist(pause_time);
  */
  Ptr<UniformRandomVariable> x_dist = CreateObject<UniformRandomVariable> ();
  x_dist->SetAttribute ("Min", DoubleValue(0));
  x_dist->SetAttribute ("Max", DoubleValue(x_range));
  Ptr<UniformRandomVariable> y_dist = CreateObject<UniformRandomVariable> ();
  y_dist->SetAttribute ("Min", DoubleValue(0));
  y_dist->SetAttribute ("Max", DoubleValue(y_range));
  Ptr<UniformRandomVariable> speed_dist = CreateObject<UniformRandomVariable> ();
  speed_dist->SetAttribute ("Min", DoubleValue(1));
  speed_dist->SetAttribute ("Max", DoubleValue(max_speed));

  int idx = 0;
  for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
    // std::cout << "installing mobility for node(" << idx++ << ")" << std::endl;
    Ptr<Node> object = *i;
    Ptr<WaypointMobilityModel> wayMobility = object->GetObject<WaypointMobilityModel>();
    int cur_time = 0;
    double cur_x = x_dist->GetValue();
    double cur_y = y_dist->GetValue();
    wayMobility->AddWaypoint(Waypoint(Seconds(cur_time), Vector3D(cur_x, cur_y, 0)));
    std::cout << "node(" << idx << ") start position: " << cur_x << ", " << cur_y << std::endl;
    while (cur_time <= sim_time) {
      double next_x = x_dist->GetValue();
      double next_y = y_dist->GetValue();
      double distance = getDistance(cur_x, cur_y, next_x, next_y);
      int speed = speed_dist->GetInteger();
      int duration = distance / speed;
      if (duration == 0) duration = 1;
      wayMobility->AddWaypoint(Waypoint(Seconds(cur_time + duration), Vector3D(next_x, next_y, 0)));
      // pause
      int pause = pause_time;
      // if (constant_pause == true) pause = pause_time;
      // else pause = pause_time_dist(rengine_);
      if (pause != 0) {
        wayMobility->AddWaypoint(Waypoint(Seconds(cur_time + duration + pause), Vector3D(next_x, next_y, 0)));
        // std::cout << "add time: " << cur_time + duration + pause << std::endl;
      }
      // update location and time
      cur_time = cur_time + duration + pause;
      cur_x = next_x;
      cur_y = next_y;
    }
    idx++;
  }
}

int
main (int argc, char *argv[])
{
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("OfdmRate24Mbps"));

  bool constant_pause = true;
  int pause_time = 0;
  double loss_rate = 0.0;
  int range = -1;
  int run = 0;
  // parameters for app
  bool useHeartbeat = true;
  bool useFastResync = true;
  uint64_t heartbeatTimer = 5;
  uint64_t detectPartitionTimer = 20;
  CommandLine cmd;
  cmd.AddValue("constantPause", "if the pause_time is constant", constant_pause);
  cmd.AddValue("wifiRange", "the wifi range", range);
  cmd.AddValue("pauseTime", "pause time", pause_time);
  cmd.AddValue("lossRate", "loss rate", loss_rate);
  cmd.AddValue("run", "run number", run);

  cmd.AddValue("useHeartbeat", "useHeartbeat", useHeartbeat);
  cmd.AddValue("useFastResync", "useFastResync", useFastResync);
  cmd.AddValue("heartbeatTimer", "heartbeatTimer", heartbeatTimer);
  cmd.AddValue("detectPartitionTimer", "detectPartitionTimer", detectPartitionTimer);
  cmd.Parse (argc,argv);
  assert(range != -1);
  RngSeedManager::SetRun (run);

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
  // wifiChannel.AddPropagationLoss ("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(range));

  //YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default ();
  wifiPhyHelper.SetChannel (wifiChannel.Create ());
  // wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
  // wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

  NqosWifiMacHelper wifiMacHelper = NqosWifiMacHelper::Default ();
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  NodeContainer nodes;
  nodes.Create (node_num);

  ////////////////
  // 1. Install Wifi
  NetDeviceContainer wifiNetDevices = wifi.Install (wifiPhyHelper, wifiMacHelper, nodes);

  // 2. Install Mobility model
  // installMobility(nodes, constant_pause, pause_time);
  auto traceFile = "scenario1.ns_movements";
  Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
  ns2.Install ();

  // 3. Install NDN stack
  StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback (MyNetDeviceFaceCallback));
  //ndnHelper.SetDefaultRoutes (true);
  ndnHelper.InstallAll();

  // 4. Set Forwarding Strategy
  StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");

  // install SyncApp
  uint64_t idx = 0;
  for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
    Ptr<Node> object = *i;
    Ptr<MobilityModel> position = object->GetObject<MobilityModel>();
    Vector pos = position->GetPosition();
    std::cout << "node " << idx << " position: " << pos.x << " " << pos.y << std::endl;

    AppHelper syncForSleepAppHelper("SyncForSleepApp");
    syncForSleepAppHelper.SetAttribute("NodeID", UintegerValue(idx));
    syncForSleepAppHelper.SetAttribute("Prefix", StringValue("/"));
    syncForSleepAppHelper.SetAttribute("UseHeartbeat", BooleanValue(useHeartbeat));
    syncForSleepAppHelper.SetAttribute("UseFastResync", BooleanValue(useFastResync));
    syncForSleepAppHelper.SetAttribute("HeartbeatTimer", UintegerValue(heartbeatTimer));
    syncForSleepAppHelper.SetAttribute("DetectPartitionTimer", UintegerValue(detectPartitionTimer));
    auto app = syncForSleepAppHelper.Install(object);
    app.Start(Seconds(2));

    StackHelper::setNodeID(idx, object);
    StackHelper::setLossRate(loss_rate, object);
    FibHelper::AddRoute(object, "/ndn/syncNotify", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/vsyncData", std::numeric_limits<int32_t>::max());
    // FibHelper::AddRoute(object, "/ndn/heartbeat", std::numeric_limits<int32_t>::max());
    FibHelper::AddRoute(object, "/ndn/bundledData", std::numeric_limits<int32_t>::max());
    idx++;
  }

  // Trace Collisions
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacTxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop", MakeCallback(&PhyRxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxDrop", MakeCallback(&PhyTxDrop));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin", MakeCallback(&PhyRxBegin));
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback(&PhyRxEnd));
  ////////////////

  Simulator::Stop (Seconds (sim_time));

  // Simulator::Schedule(Seconds(200.0), &PrintDrop);

  // std::string animFile = "ad-hoc-animation.xml";
  // AnimationInterface anim (animFile);
  // anim.SetMobilityPollInterval (Seconds (1));

  // L3RateTracer::InstallAll("test-rate-trace.txt", Seconds(0.5));
  // L2RateTracer::InstallAll("drop-trace.txt", Seconds(0.5));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}


