#ifndef NDN_TEST_RANGE_HPP_
#define NDN_TEST_RANGE_HPP_

#include "ns3/ndnSIM-module.h"
#include "test-transmission-time-node.hpp"
#include "ns3/uinteger.h"

namespace ns3 {
namespace ndn {

namespace test_transmission_time = ::ndn::test_transmission_time;

class testTransmissionTime : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("testTransmissionTime")
      .SetParent<Application>()
      .AddConstructor<testTransmissionTime>()
      .AddAttribute("NodeID", "NodeID for testTransmissionTime node", UintegerValue(0),
                    MakeUintegerAccessor(&testTransmissionTime::nid_), MakeUintegerChecker<uint64_t>());

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new test_transmission_time::TestTransmissionTimeNode(nid_));
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  uint32_t nid_;
  std::unique_ptr<test_transmission_time::TestTransmissionTimeNode> m_instance;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_GEO_PRODUCER_HPP