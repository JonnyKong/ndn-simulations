/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <functional>
#include <iostream>
#include <random>

#include "vsync.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace vsync {
namespace sleeping {

class SimpleNode {
 public:
  SimpleNode(const GroupID& gid, const NodeID& nid, const Name& prefix, const uint64_t group_size)
      : scheduler_(face_.getIoService()),
        nid_(nid),
        gid_(gid),
        node_(face_, scheduler_, ns3::ndn::StackHelper::getKeyChain(), nid, prefix, gid, group_size,
              std::bind(&SimpleNode::OnData, this, _1)),
        rengine_(rdevice_()),
        rdist_(3000, 10000)
        {
        }

  void Start() {
    std::cout << "Sleeping node starts to work now!" << std::endl;
  }

  void OnData(const VersionVector& vv) {
  std::cout << "node(" << gid_  << " " << nid_ << "), version vector=" << VersionVectorToString(vv)
            << std::endl;
  }

private:
  Face face_;
  Scheduler scheduler_;
  NodeID nid_;
  GroupID gid_;
  Node node_;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;
};

}  // two_node_sync
}  // namespace vsync
}  // namespace ndn

