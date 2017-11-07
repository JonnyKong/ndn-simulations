#ifndef NDN_GEO_CONSUMER_NODE_HPP_
#define NDN_GEO_CONSUMER_NODE_HPP_

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

class GeoConsumerNode {
 public:
  GeoConsumerNode(const std::string& position, const std::string& dest_batch)
      : position_(position),
        key_chain_(ns3::ndn::StackHelper::getKeyChain()),
        scheduler_(face_.getIoService()),
        rengine_(rdevice_()),
        rdist_(10000, 20000) 
  {
    // parse the dest_batch into list
    /*
    std::string new_dest = "";
    for(auto c: dest_batch) {
      if (c == ' ') {
        if (new_dest != "") {
          dest_list_.push_back(new_dest);
          new_dest = "";
        }
        else continue;
      }
      else new_dest.push_back(c);
    }
    if (new_dest != "") dest_list_.push_back(new_dest);
    */
    
    for (int i = 3; i <= 30; i+=3) {
      for (int j = 3; j <= 30; j+=3) {
        int pos = i * 30 * 10000 + j * 30;
        dest_list_.push_back(std::to_string(pos));
        std::cout << pos << std::endl;
      }
    }
  }

  void Start() {
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { SendInterest(); });
  }

  void SendInterest() {
    /*
    if (dest_index != dest_list_.size()) {
      // sned yhe interest
      auto interest_name = MakeGeoInterestName(dest_index);
      uint32_t pos = boost::lexical_cast<std::uint32_t>(position_);
      Interest interest(interest_name, pos, pos);
      face_.expressInterest(interest, std::bind(&GeoConsumerNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      std::cout << "node(" << position_ << ") Send interest name=" << interest_name.toUri() << std::endl; 
      dest_index++;
    }
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { SendInterest(); });
    */
    Name interest_name(kGeoForwardingPrefix);
    interest_name.append("6000600");
    uint32_t pos = boost::lexical_cast<std::uint32_t>(position_);
    Interest interest(interest_name, pos, pos);
    face_.expressInterest(interest, std::bind(&GeoConsumerNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
    std::cout << "node(" << position_ << ") Send interest name=" << interest_name.toUri() << std::endl; 
  }

  Name MakeGeoInterestName(const int& dest_index) {
    Name n(kGeoForwardingPrefix);
    n.append(dest_list_[dest_index]);
    return n;
  }

  void OnRemoteData(const Data& data) {
    const auto& data_name = data.getName();
    std::cout << "node(" << position_ << ") Recv data: name=" << data_name.toUri() << std::endl;
  }

private:
  Face face_;
  std::string position_;
  KeyChain& key_chain_;
  Scheduler scheduler_;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;

  unsigned int dest_index = 0;
  std::vector<std::string> dest_list_;
};

}  // namespace geo_forwarding
}  // namespace ndn

/*
class GeoConsumerNode {
 public:
  GeoConsumerNode(const std::string& srcVid, uint32_t& numNodeInGroup, uint32_t& vidRange, uint32_t& numDest)
      : srcVid_(srcVid),
        numNodeInGroup_(numNodeInGroup),
        vidRange_(vidRange),
        numDest_(numDest),
        key_chain_(ns3::ndn::StackHelper::getKeyChain()),
        scheduler_(face_.getIoService()),
        rengine_(rdevice_()),
        rdist_(10000, 20000)
  {
    // initialize the dest_list_ for the experiment
    std::cout << "Initializing the dest_list_:" << std::endl;
    for (uint32_t i = 0; i < numDest_; ++i) {
      std::uniform_int_distribution<> rdist_vid(0, vidRange_ - 1);
      std::uniform_int_distribution<> rdist_nid(0, numNodeInGroup_ - 1);

      int vid_x = rdist_vid(rengine_);
      int vid_y = rdist_vid(rengine_);
      std::string vid = std::to_string(10000 * vid_x + vid_y);
      std::string nid = std::to_string(rdist_nid(rengine_));

      dest_list_.push_back(pair<std::string, std::string>(vid, nid));
      std::cout << "dest " << i << ": vid=" << vid << " nid=" << nid << std::endl;
    }
  }

  void Start() {
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { SendInterest(); });
  }

  void SendInterest() {
    if (dest_index != dest_list_.size()) {
      // sned yhe interest
      auto interest_name = MakeGeoInterestName(dest_index);
      uint32_t src_vid = boost::lexical_cast<std::uint32_t>(srcVid_);
      Interest interest(interest_name, src_vid, src_vid);
      face_.expressInterest(interest, std::bind(&GeoConsumerNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      std::cout << "node(" << srcVid_ << ") Send interest name=" << interest_name.toUri() << std::endl; 
      dest_index++;
    }
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { SendInterest(); });
  }

  Name MakeGeoInterestName(const int& dest_index) {
    Name n(kGeoForwardingPrefix);
    n.append(dest_list_[dest_index].first).append(dest_list_[dest_index].second).append(std::to_string(dest_index + 2));
    return n;
  }

  void OnRemoteData(const Data& data) {
    const auto& data_name = data.getName();
    std::cout << "node(" << srcVid_ << ") Recv data: name=" << data_name.toUri() << std::endl;
  }

private:
  Face face_;
  std::string srcVid_;
  uint32_t numNodeInGroup_;
  uint32_t vidRange_;
  uint32_t numDest_;
  KeyChain& key_chain_;
  Scheduler scheduler_;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;

  unsigned int dest_index = 0;
  std::vector<pair<std::string, std::string>> dest_list_;
};

}  // namespace geo_forwarding
}  // namespace ndn

*/
#endif