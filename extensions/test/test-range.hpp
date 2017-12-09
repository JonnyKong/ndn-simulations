#ifndef NDN_TEST_RANGE_HPP_
#define NDN_TEST_RANGE_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "test-range-node.hpp"

namespace ns3 {
namespace ndn {

namespace test_range = ::ndn::test_range;

class testRange : public Application
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("testRange")
      .SetParent<Application>()
      .AddConstructor<testRange>();

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new test_range::TestRangeNode());
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<test_range::TestRangeNode> m_instance;
};

} // namespace ndn
} // namespace ns3

#endif // NDN_GEO_PRODUCER_HPP