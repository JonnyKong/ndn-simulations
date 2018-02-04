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
  TestRangeNode(uint64_t nid) : 
    scheduler_(face_.getIoService()),
    nid_(nid)
  {
    face_.setInterestFilter(
      kTestRangePrefix, std::bind(&TestRangeNode::OnTestRangeInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        std::cout << "Failed to register testRange prefix: " << reason << std::endl;
      });
  }

  void Start() {
    /*
    scheduler_.scheduleEvent(time::milliseconds(nid_ * 100),
                             [this] { SendInterest(); });
    */
    if (nid_ == 0) SendInterest();
  }

  void SendInterest() {
    Name n = kTestRangePrefix;
    n.appendNumber(nid_);
    Interest interest(n);
    face_.expressInterest(interest, std::bind(&TestRangeNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    time::system_clock::time_point send_interest_time = time::system_clock::now();
    uint64_t milliseconds = time::toUnixTimestamp(send_interest_time).count();
    std::cout << "node(" << nid_ << ")Send interest name=" << n.toUri() << " current time = " << milliseconds << std::endl; 
  }

  void OnTestRangeInterest(const Interest& interest) {
    time::system_clock::time_point cur_time = time::system_clock::now();
    std::cout << "node(" << nid_ << ") receives the interest, current time = " << time::toUnixTimestamp(cur_time).count() << std::endl;
  }


  void OnRemoteData(const Data& data) {
  }

private:
  Face face_;
  Scheduler scheduler_;
  uint64_t nid_;
};

}  // namespace geo_forwarding
}  // namespace ndn

#endif