#ifndef NDN_TEST_CONSUMER_HPP_
#define NDN_TEST_CONSUMER_HPP_

#include "ns3/ndnSIM-module.h"
#include "ns3/integer.h"
#include "ns3/string.h"
#include "test-consumer-node.hpp"

namespace ns3 {
namespace ndn{

class testConsumer : public Application 
{
public:
  static TypeId
  GetTypeId()
  {
    static TypeId tid = TypeId("testConsumer")
      .SetParent<Application>()
      .AddConstructor<testConsumer>();

    return tid;
  }

protected:
  // inherited from Application base class.
  virtual void
  StartApplication()
  {
    m_instance.reset(new ::ndn::TestConsumerNode());
    m_instance->Start();
  }

  virtual void
  StopApplication()
  {
    m_instance.reset();
  }

private:
  std::unique_ptr<::ndn::TestConsumerNode> m_instance;
};

}
}

#endif