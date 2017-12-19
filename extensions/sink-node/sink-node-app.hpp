#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include "sink-node.hpp"

namespace ns3 {
namespace ndn {

namespace vsync = ::ndn::vsync;

class SinkNodeApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("SinkNodeApp")
      .SetParent<Application>()
      .AddConstructor<SinkNodeApp>()
      .AddAttribute("GroupID", "GroupID for sink node", StringValue("0"),
                    MakeStringAccessor(&SinkNodeApp::gid_), MakeStringChecker())
      .AddAttribute("Prefix", "Prefix for sink node", StringValue("/"),
                    MakeNameAccessor(&SinkNodeApp::prefix_), MakeNameChecker())
      .AddAttribute("GroupSize", "Size of sink node's group", UintegerValue(0),
                    MakeUintegerAccessor(&SinkNodeApp::group_size_), MakeUintegerChecker<uint64_t>());
      

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::sink_node::SimpleNode(gid_, prefix_, group_size_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::sink_node::SimpleNode> m_instance;
  vsync::GroupID gid_;
  Name prefix_;
  uint64_t group_size_;
};

} // namespace ndn
} // namespace ns3
