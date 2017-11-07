
#ifndef NDN_MESH_CONSUMER_NODE_HPP_
#define NDN_MESH_CONSUMER_NODE_HPP_

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

class MeshConsumerNode {
 public:
  MeshConsumerNode(const std::string& srcVid, uint32_t& numNodeInGroup, uint32_t& vidRange, uint32_t& numDest)
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
    /*
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
    */
    //dest_vid.push_back("20004");
    //dest_vid.push_back("40002");
    dest_vid.push_back("30004");
    //dest_vid.push_back("30003");

    dest_nid.push_back("1");
    //dest_nid.push_back("2");
    //dest_nid.push_back("3");
    //dest_nid.push_back("4");
  }

  void Start() {
    scheduler_.scheduleEvent(time::milliseconds(120000),
                             [this] { SendInterest(); });
  }

  void SendInterest() {
    if (dest_index != dest_vid.size()) {
      // sned yhe interest
      auto interest_name = MakeGeoInterestName(dest_index);
      uint32_t src_vid = boost::lexical_cast<std::uint32_t>(srcVid_);
      Interest interest(interest_name, src_vid, src_vid);
      face_.expressInterest(interest, std::bind(&MeshConsumerNode::OnRemoteData, this, _2),
                          [](const Interest&, const lp::Nack&) {},
                          [](const Interest&) {});
      std::cout << "node(" << srcVid_ << ") Send interest name=" << interest_name.toUri() << std::endl; 
      dest_index++;
    }
    scheduler_.scheduleEvent(time::milliseconds(10000),
                             [this] { SendInterest(); });
  }

  Name MakeGeoInterestName(const int& dest_index) {
    Name n(kGeoForwardingPrefix);
    //n.append(dest_list_[dest_index].first).append(dest_list_[dest_index].second).append(std::to_string(dest_index + 2));
    n.append(dest_vid[dest_index]).append(dest_nid[dest_index]);
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
  std::vector<std::string> dest_vid;
  std::vector<std::string> dest_nid;
};

}  // namespace geo_forwarding
}  // namespace ndn

#endif
