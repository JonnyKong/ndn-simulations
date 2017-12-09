/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <functional>
#include <iostream>
#include <random>

#include "vsync.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace vsync {
namespace two_node_sync {

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
    int time = (nid_ + 1) * 10000;
    std::cout << "will publish data after " << time << std::endl;
    scheduler_.scheduleEvent(time::milliseconds(time), [this] { PublishData(); });
  }

  void OnData(const VersionVector& vv) {
    std::cout << "node(" << gid_  << " " << nid_ << "), version vector=" << VersionVectorToString(vv)
              << std::endl;
  }

  void PublishData() {
    generation--;
    if (generation == 0) {
      if (nid_ == 0) node_.SyncData();
      return;
    }
    std::cout << "node(" << gid_ << "," << nid_ << ") PublishData" << std::endl; 
    node_.PublishData("Hello from " + to_string(node_.GetNodeID()));
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { PublishData(); });
  }

private:
  Face face_;
  Scheduler scheduler_;
  NodeID nid_;
  GroupID gid_;
  Node node_;
  int generation = 3;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;
};

}  // two_node_sync
}  // namespace vsync
}  // namespace ndn

