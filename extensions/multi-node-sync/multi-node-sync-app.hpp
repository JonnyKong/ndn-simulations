#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include "vsync-node-multi.hpp"

namespace ns3 {
namespace ndn {

namespace vsync = ::ndn::vsync;

class MultiNodeSyncApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("MultiNodeSyncApp")
      .SetParent<Application>()
      .AddConstructor<MultiNodeSyncApp>()
      .AddAttribute("GroupID", "GroupID for sync node", StringValue("0"),
                    MakeStringAccessor(&MultiNodeSyncApp::gid_), MakeStringChecker())
      .AddAttribute("NodeID", "NodeID for sync node", UintegerValue(0),
                    MakeUintegerAccessor(&MultiNodeSyncApp::nid_), MakeUintegerChecker<uint64_t>())
      .AddAttribute("Prefix", "Prefix for sync node", StringValue("/"),
                    MakeNameAccessor(&MultiNodeSyncApp::prefix_), MakeNameChecker())
      .AddAttribute("GroupSize", "Size of sync node's group", UintegerValue(0),
                    MakeUintegerAccessor(&MultiNodeSyncApp::group_size_), MakeUintegerChecker<uint64_t>());
      

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::multi_node_sync::SimpleNode(gid_, nid_, prefix_, group_size_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::multi_node_sync::SimpleNode> m_instance;
  vsync::GroupID gid_;
  vsync::NodeID nid_;
  Name prefix_;
  uint64_t group_size_;
};

} // namespace ndn
} // namespace ns3
