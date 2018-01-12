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
      std::vector<std::vector<ReceiveWindow>> rw_snapshots = node_.GetRWSnapshots();
      double receive_first_syncACK_delay = node_.ReceiveFirstSyncACKDelay();
      double receive_last_syncACK_delay = node_.ReceiveLastSyncACKDelay();
      
      std::cout << "data snapshot size = " << data_snapshots.size() << std::endl;
      std::cout << "rw snapshot size = " << rw_snapshots.size() << std::endl;
      std::cout << "average first-syncACK-delay = " << receive_first_syncACK_delay << std::endl;
      std::cout << "average last-syncACK-delay = " << receive_last_syncACK_delay << std::endl;
      //out << receive_first_syncACK_delay << "\n";
      //out << receive_last_syncACK_delay << "\n";
      // out << node_.GetCollisionNum() << "\n";
      //out << node_.GetSuppressionNum() << "\n";
      //out << node_.GetCollisionNum() << "\n";
      //out << node_.GetOutDataInterestNum() << "\n";
      out << node_.GetInterestSize() << "\n";
      out << node_.GetDataSize() << "\n";
      // get the outVsyncData & outVsyncInterest info
      /*
      std::string outVsyncinfo = node_.GetOutVsyncInfo();
      size_t sep = outVsyncinfo.find(",");
      uint64_t outVsyncInterest = std::stoull(outVsyncinfo.substr(0, sep));
      uint64_t outVsyncData = std::stoull(outVsyncinfo.substr(sep + 1));
      std::cout << "get the outVsyncInfo: " << outVsyncinfo << ": outVsyncInterest = " << outVsyncInterest << ", outVsyncData = " << outVsyncData << std::endl;
      out << outVsyncInterest << "\n";
      out << outVsyncData << "\n";
      */

      if (data_snapshots.size() != vv_snapshots.size()) {
        std::cout << "data_snapshots size doesn't equal to vv_snapshots size" << std::endl;
      }
      out << ToString(node_.GetActiveRecord()) << "\n";
      out << ToString(data_snapshots) << "\n";
      for (auto rw: rw_snapshots) {
        out << ToString(rw) << "\n";
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

  std::string ToString(std::vector<ReceiveWindow> rw) {
    std::string res = "";
    for (int i = 0; i < rw.size(); ++i) {
      ReceiveWindow recv_window = rw[i];
      ReceiveWindow::SeqNumIntervalSet win = recv_window.getWin();
      if (win.iterative_size() == 0) continue;
      auto it = win.begin();
      while (it != win.end()) {
        res += "(" + to_string(i) + ":" + to_string(it->lower()) + "-" + to_string(it->upper()) + ")";
        it++;
      }
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

