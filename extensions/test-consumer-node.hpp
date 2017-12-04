#ifndef NDN_TEST_CONSUMER_NODE_HPP_
#define NDN_TEST_CONSUMER_NODE_HPP_

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

static const Name kTestPrefix = Name("/ndn/test");

class TestConsumerNode {
 public:
  TestConsumerNode()
  {
    std::cout << 0 << std::endl;
  }

  void Start() {
    SendInterest();
  }

  void SendInterest() {
    Interest interest(kTestPrefix);
    const std::string& content = "hello!";
    std::cout << 1 << std::endl;
    //interest.setContent(reinterpret_cast<const uint8_t*>(content.data()),
    //                    content.size());
    std::cout << 2 << std::endl;
    face_.expressInterest(interest, std::bind(&TestConsumerNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    std::cout << "Consumer Send interest name=" << kTestPrefix.toUri() << std::endl; 
  }


  void OnRemoteData(const Data& data) {
    std::cout << "Consumer Recv data name=" << data.getName().toUri() << std::endl;
  }

 private:
  Face face_;
};

}

#endif