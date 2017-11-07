/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <functional>
#include <iostream>
#include <random>

#include "vsync.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace vsync {

class SimpleNode {
 public:
  SimpleNode(const NodeID& nid, const Name& prefix, const ViewID& vid)
      : scheduler_(face_.getIoService()),
        nid_(nid),
        vid_(vid),
        node_(face_, scheduler_, ns3::ndn::StackHelper::getKeyChain(), nid, prefix, vid,
              std::bind(&SimpleNode::OnData, this, _1, _2)),
        rengine_(rdevice_()),
        rdist_(3000, 10000),
        rdist2(10000, 20000)
        {
        }

  void Start() {
    //std::cout << "node(" << vid_ << "," << nid_ << ") PublishData" << std::endl; 
    // int time = (std::stoi(nid_) + 1) * 10000;
    //std::cout << "will publish data after " << time << std::endl;
    //scheduler_.scheduleEvent(time::milliseconds(time),
    //
    /*                         [this] { PublishData(); });
    if (vid_ == "40002") {
      int time = (std::stoi(nid_) + 1) * 10000;
      std::cout << "will publish data after " << time << std::endl;
      scheduler_.scheduleEvent(time::milliseconds(time),
                             [this] { PublishData(); });
    }
    */
    face_.processEvents();
  }

  void OnData(const std::string& content, const VersionVector& vv) {
    std::cout << "node( " << vid_  << " " << nid_ << ") Upcall OnData: content=\"" << content << '"' << ", version vector=" << ToString(vv)
              << std::endl;
  }

  void PublishData() {
    std::cout << "node(" << vid_ << "," << nid_ << ") PublishData" << std::endl; 
    node_.PublishData("Hello from " + node_.GetNodeID());
    //scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
    //                         [this] { PublishData(); });
  }

private:
  Face face_;
  Scheduler scheduler_;
  NodeID nid_;
  ViewID vid_;
  Node node_;
  bool vsync = false;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;
  std::uniform_int_distribution<> rdist2;
};

}  // namespace vsync
}  // namespace ndn

