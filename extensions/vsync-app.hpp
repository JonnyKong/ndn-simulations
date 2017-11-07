#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"

#include "vsync-node.hpp"

namespace ns3 {
namespace ndn {

namespace vsync = ::ndn::vsync;

class VectorSyncApp : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("VectorSyncApp")
      .SetParent<Application>()
      .AddConstructor<VectorSyncApp>()
      .AddAttribute("Prefix", "Prefix for vsync node", StringValue("/"),
                    MakeNameAccessor(&VectorSyncApp::m_prefix), MakeNameChecker())
      .AddAttribute("NodeID", "NodeID for sync node", StringValue("0"),
                    MakeStringAccessor(&VectorSyncApp::m_nodeID), MakeStringChecker())
      .AddAttribute("ViewID", "ViewID for sync node", StringValue("0"),
                    MakeStringAccessor(&VectorSyncApp::m_viewID), MakeStringChecker());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new vsync::SimpleNode(m_nodeID, m_prefix, m_viewID));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<vsync::SimpleNode> m_instance;
  std::string m_nodeID;
  std::string m_viewID;
  Name m_prefix;
};

} // namespace ndn
} // namespace ns3
