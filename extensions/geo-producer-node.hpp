#ifndef NDN_GEO_PRODUCER_NODE_HPP_
#define NDN_GEO_PRODUCER_NODE_HPP_

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
namespace geo_forwarding {

static const Name kGeoForwardingPrefix = Name("/ndn/geoForwarding");

class GeoProducerNode {
 public:
  GeoProducerNode(const std::string& position, const std::string& virtualPayloadSize)
      : position_(position),
        m_virtualPayloadSize(boost::lexical_cast<uint32_t>(virtualPayloadSize)),
        key_chain_(ns3::ndn::StackHelper::getKeyChain()),
        scheduler_(face_.getIoService()),
        rengine_(rdevice_()),
        rdist_(500, 10000) 
  {
    Name interest_register = Name(kGeoForwardingPrefix);
    interest_register.append(position_);

    //std::cout << "Starting to register the geo-interest in geo-producer" << std::endl;
    face_.setInterestFilter(
      interest_register, std::bind(&GeoProducerNode::OnGeoInterest, this, _2),
      [this](const Name&, const std::string& reason) {
        std::cout << "Failed to register geo-forwarding-producer: " << reason << std::endl;
      });
  }

  void Start() {
    //face_.processEvents();
  }



  void OnGeoInterest(const Interest& interest) {
    scheduler_.scheduleEvent(time::milliseconds(10),
                             bind(&GeoProducerNode::SendData, this, interest));
    
  }

  void SendData(const Interest& interest) {
    Name data_name(interest.getName());
    std::shared_ptr<Data> data = std::make_shared<Data>(data_name);
    data->setFreshnessPeriod(time::seconds(3600));
    data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
    key_chain_.sign(*data, signingWithSha256());
    face_.put(*data);
    std::cout << "node(" << position_ << ") Send Data" << std::endl;
  }

private:
  Face face_;
  std::string position_;
  std::uint32_t m_virtualPayloadSize;
  KeyChain& key_chain_;
  Scheduler scheduler_;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;
};

}  // namespace geo_forwarding
}  // namespace ndn

#endif // NDN_GEO_PRODUCER_NODE_HPP