#ifndef NDN_GEO_CONSUMER_HPP_
#define NDN_GEO_CONSUMER_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "geo-consumer-node.hpp"

namespace ns3 {
namespace ndn {

namespace geo_forwarding = ::ndn::geo_forwarding;

class GeoConsumer : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("GeoConsumer")
      .SetParent<Application>()
      .AddConstructor<GeoConsumer>()
      .AddAttribute("Position", "Position for sensor node", StringValue("0"),
                    MakeStringAccessor(&GeoConsumer::m_position), MakeStringChecker())
      .AddAttribute("DestBatch", "Destination batch for sensor node", StringValue("0"),
                    MakeStringAccessor(&GeoConsumer::dest_batch), MakeStringChecker());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new geo_forwarding::GeoConsumerNode(m_position, dest_batch));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<geo_forwarding::GeoConsumerNode> m_instance;
  std::string m_position;
  std::string dest_batch;
};

/*
class GeoConsumer : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("GeoConsumer")
      .SetParent<Application>()
      .AddConstructor<GeoConsumer>()
      .AddAttribute("ViewID", "Position for sensor node", StringValue("0"),
                    MakeStringAccessor(&GeoConsumer::m_vid), MakeStringChecker())
      .AddAttribute("NumNodeInGroup", "Number of nodes in a group", IntegerValue(0),
                    MakeIntegerAccessor(&GeoConsumer::m_numNodeInGroup), MakeIntegerChecker<uint32_t>())
      .AddAttribute("VidRange", "Vid range", IntegerValue(0),
                    MakeIntegerAccessor(&GeoConsumer::m_vidRange), MakeIntegerChecker<uint32_t>())
      .AddAttribute("NumDest", "Number of dest nodes", IntegerValue(0),
                    MakeIntegerAccessor(&GeoConsumer::m_numDest), MakeIntegerChecker<uint32_t>());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new geo_forwarding::GeoConsumerNode(m_vid, m_numNodeInGroup, m_vidRange, m_numDest));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<geo_forwarding::GeoConsumerNode> m_instance;
  std::string m_vid;
  uint32_t m_numNodeInGroup;
  uint32_t m_vidRange;
  uint32_t m_numDest;
};
*/

} // namespace ndn
} // namespace ns3

#endif // NEN_GEO_CONSUMER_HPP_
