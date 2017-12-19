#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

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
      .AddAttribute("GroupID", "GroupID for sync node", StringValue("0"),
                    MakeStringAccessor(&SyncForSleepApp::gid_), MakeStringChecker())
      .AddAttribute("NodeID", "NodeID for sync node", UintegerValue(0),
                    MakeUintegerAccessor(&SyncForSleepApp::nid_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("Prefix", "Prefix for sync node", StringValue("/"),
                    MakeNameAccessor(&SyncForSleepApp::prefix_), MakeNameChecker())
      .AddAttribute("GroupSize", "Size of sync node's group", UintegerValue(0),
                    MakeUintegerAccessor(&SyncForSleepApp::group_size_), MakeUintegerChecker<uint64_t>());
      

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    std::cout << "calling StartApplication" << std::endl;
    m_instance.reset(new vsync::sync_for_sleep::SimpleNode(gid_, nid_, prefix_, group_size_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    std::cout << "calling StopApplication" << std::endl;
    m_instance->Stop();
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::sync_for_sleep::SimpleNode> m_instance;
  vsync::GroupID gid_;
  vsync::NodeID nid_;
  Name prefix_;
  uint64_t group_size_;
};

} // namespace ndn
} // namespace ns3
