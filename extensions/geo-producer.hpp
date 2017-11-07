#ifndef NDN_GEO_PRODUCER_HPP_
#define NDN_GEO_PRODUCER_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "geo-producer-node.hpp"

namespace ns3 {
namespace ndn {

namespace geo_forwarding = ::ndn::geo_forwarding;

class GeoProducer : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("GeoProducer")
      .SetParent<Application>()
      .AddConstructor<GeoProducer>()
      .AddAttribute("Position", "Position for sensor node", StringValue("0"),
                    MakeStringAccessor(&GeoProducer::m_position), MakeStringChecker())
      .AddAttribute("PayLoad", "Pay Load Size", StringValue("0"),
                    MakeStringAccessor(&GeoProducer::m_virtualPayloadSize), MakeStringChecker());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new geo_forwarding::GeoProducerNode(m_position, m_virtualPayloadSize));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<geo_forwarding::GeoProducerNode> m_instance;
  std::string m_position;
  std::string m_virtualPayloadSize;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_GEO_PRODUCER_HPP