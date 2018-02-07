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
      .AddAttribute("NodeID", "NodeID for sync node", UintegerValue(0),
                    MakeUintegerAccessor(&SyncForSleepApp::nid_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("Prefix", "Prefix for sync node", StringValue("/"),
                    MakeNameAccessor(&SyncForSleepApp::prefix_), MakeNameChecker());
      

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::sync_for_sleep::SimpleNode(nid_, prefix_));
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
};

} // namespace ndn
} // namespace ns3
