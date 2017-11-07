
#ifndef NDN_MESH_CONSUMER_HPP_
#define NDN_MESH_CONSUMER_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "mesh-consumer-node.hpp"

namespace ns3 {
namespace ndn {

namespace geo_forwarding = ::ndn::geo_forwarding;

class MeshConsumer : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("MeshConsumer")
      .SetParent<Application>()
      .AddConstructor<MeshConsumer>()
      .AddAttribute("ViewID", "Position for sensor node", StringValue("0"),
                    MakeStringAccessor(&MeshConsumer::m_vid), MakeStringChecker())
      .AddAttribute("NumNodeInGroup", "Number of nodes in a group", IntegerValue(0),
                    MakeIntegerAccessor(&MeshConsumer::m_numNodeInGroup), MakeIntegerChecker<uint32_t>())
      .AddAttribute("VidRange", "Vid range", IntegerValue(0),
                    MakeIntegerAccessor(&MeshConsumer::m_vidRange), MakeIntegerChecker<uint32_t>())
      .AddAttribute("NumDest", "Number of dest nodes", IntegerValue(0),
                    MakeIntegerAccessor(&MeshConsumer::m_numDest), MakeIntegerChecker<uint32_t>());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new geo_forwarding::MeshConsumerNode(m_vid, m_numNodeInGroup, m_vidRange, m_numDest));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<geo_forwarding::MeshConsumerNode> m_instance;
  std::string m_vid;
  uint32_t m_numNodeInGroup;
  uint32_t m_vidRange;
  uint32_t m_numDest;
};


} // namespace ndn
} // namespace ns3

#endif // NEN_GEO_CONSUMER_HPP_
