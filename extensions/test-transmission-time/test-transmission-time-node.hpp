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

#include <chrono>

namespace ndn {
namespace test_transmission_time {

static const Name kTestTransmissionTimePrefix = Name("/ndn/testTransmissionTime");

class TestTransmissionTimeNode {
 public:
  TestTransmissionTimeNode(uint32_t nid) : 
    scheduler_(face_.getIoService()),
    key_chain_(ns3::ndn::StackHelper::getKeyChain()),
    nid_(nid)
  {
    face_.setInterestFilter(
      kTestTransmissionTimePrefix, std::bind(&TestTransmissionTimeNode::OnTestTransmissionTimInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        std::cout << "Failed to register testTransmissionTime prefix: " << reason << std::endl;
      });
  }

  void Start() {
    if (nid_ == 0) {
      scheduler_.scheduleEvent(time::milliseconds(100), [this] { SendInterest(); });
    }
  }

  void SendInterest() {
    Interest interest(kTestTransmissionTimePrefix);
    send_interest_time = time::system_clock::now();
    // std::time_t time_span = time::system_clock::to_time_t(sleep_end) - time::system_clock::to_time_t(sleep_start);
    face_.expressInterest(interest, std::bind(&TestTransmissionTimeNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    uint64_t milliseconds = time::toUnixTimestamp(send_interest_time).count();
    std::cout << "node(" << nid_ << ")Send interest name=" << kTestTransmissionTimePrefix.toUri() << " current time = " << milliseconds << std::endl; 
  }

  void OnTestTransmissionTimInterest(const Interest& interest) {
    time::system_clock::time_point cur_time = time::system_clock::now();
    std::cout << "node(" << nid_ << ")receives the testTransmissionTime interest, current time = " << time::toUnixTimestamp(cur_time).count() << std::endl;
    
    std::shared_ptr<Data> data = std::make_shared<Data>(kTestTransmissionTimePrefix);
    data->setFreshnessPeriod(time::seconds(3600));
    data->setContent(make_shared< ::ndn::Buffer>(5));
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);
  }


  void OnRemoteData(const Data& data) {
    time::system_clock::time_point cur_time = time::system_clock::now();
    std::cout << "node(" << nid_ << ")receives the testTransmissionTime data, current time = " << time::toUnixTimestamp(cur_time).count() << std::endl;

    std::cout << "Transmission round time = " << time::toUnixTimestamp(cur_time) - time::toUnixTimestamp(send_interest_time) << std::endl;
  }

private:
  Face face_;
  KeyChain& key_chain_;
  Scheduler scheduler_;
  time::system_clock::time_point send_interest_time;

  uint32_t nid_;

};

}  // namespace geo_forwarding
}  // namespace ndn

#endif