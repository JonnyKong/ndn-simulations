/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <functional>
#include <iostream>
#include <random>
#include "fstream"

#include "vsync.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace vsync {
namespace sync_for_sleep {

static const std::string snapshotFileName = "snapshot.txt";
class SimpleNode {
 public:
  SimpleNode(const GroupID& gid, const NodeID& nid, const Name& prefix, const uint64_t group_size)
      : scheduler_(face_.getIoService()),
        nid_(nid),
        gid_(gid),
        node_(face_, scheduler_, ns3::ndn::StackHelper::getKeyChain(), nid, prefix, gid, group_size,
              std::bind(&SimpleNode::OnData, this, _1)),
        rengine_(rdevice_()),
        rdist_(1000, 35000)
        {
        }

  void Start() {
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { PublishData(); });
  }

  void OnData(const VersionVector& vv) {
    //std::cout << "node(" << gid_  << " " << nid_ << "), version vector=" << VersionVectorToString(vv)
    //           << std::endl;
  }

  void Stop() {
    std::ofstream out;
    out.open(snapshotFileName, std::ofstream::out | std::ofstream::app);
    if (out.is_open()) {
      out << node_.GetSleepingTime() << "\n";
      std::vector<uint64_t> data_snapshots = node_.GetDataSnapshots();
      std::vector<VersionVector> vv_snapshots = node_.GetVVSnapshots();
      std::cout << "data snapshot size = " << data_snapshots.size() << std::endl;
      std::cout << "vv snapshot size = " << vv_snapshots.size() << std::endl;
      if (data_snapshots.size() != vv_snapshots.size()) {
        std::cout << "data_snapshots size doesn't equal to vv_snapshots size" << std::endl;
      }
      out << ToString(data_snapshots) << "\n";
      for (auto vv: vv_snapshots) {
        out << ToString(vv) << "\n";
      }
    }
    else {
      std::cout << "Fail to write files" << std::endl; 
    }
  }

  void PublishData() {
    // std::cout << "node(" << gid_ << "," << nid_ << ") PublishData" << std::endl; 
    node_.PublishData("Hello from " + to_string(node_.GetNodeID()));
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { PublishData(); });
  }

  std::string ToString(std::vector<uint64_t> list) {
    if (list.size() == 0) return "";
    std::string res = to_string(list[0]);
    for (int i = 1; i < list.size(); ++i) {
      res += "," + to_string(list[i]);
    }
    return res;
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

