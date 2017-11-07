// ndn-simple.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

int
main(int argc, char* argv[])
{
  std::cout << "start" << std::endl;
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"));

  std::cout << "start" << std::endl;
  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.Parse(argc, argv);
  std::cout << "start" << std::endl;
  // Creating nodes
  NodeContainer nodes;
  nodes.Create(2);
  std::cout << "start" << std::endl;
  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  std::cout << "start" << std::endl;
  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();
  std::cout << "start" << std::endl;
  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/multicast");

  // Installing applications

  std::cout << "installing applications" << std::endl;
  // Consumer
  ndn::AppHelper consumerHelper("ns3::ndn::testConsumer");
  // Consumer will request /prefix/0, /prefix/1, ...
  consumerHelper.Install(nodes.Get(0)).Start(Seconds(1));     // first node

  // Producer
  ndn::AppHelper producerHelper("ns3::ndn::testProducer");
  // Producer will reply to all requests starting with /prefix
  producerHelper.Install(nodes.Get(1)).Start(Seconds(1)); // last node

  Simulator::Stop(Seconds(20.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}