#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include "vsync-node.hpp"

namespace ns3 {
namespace ndn {

namespace vsync = ::ndn::vsync;

class SyncApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("SyncApp")
      .SetParent<Application>()
      .AddConstructor<SyncApp>()
      .AddAttribute("GroupID", "GroupID for sync node", StringValue("0"),
                    MakeStringAccessor(&SyncApp::gid_), MakeStringChecker())
      .AddAttribute("NodeID", "NodeID for sync node", UintegerValue(0),
                    MakeUintegerAccessor(&SyncApp::nid_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("Prefix", "Prefix for sync node", StringValue("/"),
                    MakeNameAccessor(&SyncApp::prefix_), MakeNameChecker())
      .AddAttribute("GroupSize", "Size of sync node's group", UintegerValue(0),
                    MakeUintegerAccessor(&SyncApp::group_size_), MakeUintegerChecker<uint64_t>());
      

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::SimpleNode(gid_, nid_, prefix_, group_size_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::SimpleNode> m_instance;
  vsync::GroupID gid_;
  vsync::NodeID nid_;
  Name prefix_;
  uint64_t group_size_;
};

} // namespace ndn
} // namespace ns3
