#ifndef NDN_TEST_RANGE_NODE_HPP_
#define NDN_TEST_RANGE_NODE_HPP_

#include <functional>
#include <iostream>
#include <random>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/backports.hpp>
#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/signal.hpp>

namespace ndn {
namespace test_range {

static const Name kTestRangePrefix = Name("/ndn/testRange");

class TestRangeNode {
 public:
  TestRangeNode()
  {
  }

  void Start() {
    SendInterest();
  }

  void SendInterest() {
    Interest interest(kTestRangePrefix);
    face_.expressInterest(interest, std::bind(&TestRangeNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    std::cout << "Send interest name=" << kTestRangePrefix.toUri() << std::endl; 
  }


  void OnRemoteData(const Data& data) {
  }

private:
  Face face_;
};

}  // namespace geo_forwarding
}  // namespace ndn

#endif