/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#include <functional>
#include <iostream>
#include <random>

#include "vsync.hpp"

#include <ndn-cxx/face.hpp>

namespace ndn {
namespace vsync {
namespace sink_node {

class SimpleNode {
 public:
  SimpleNode(const GroupID& gid, const Name& prefix, const uint64_t group_size)
      : scheduler_(face_.getIoService()),
        gid_(gid),
        rengine_(rdevice_()),
        rdist_(5000, 10000)
        {
        }

  void Start() {
    scheduler_.scheduleEvent(time::milliseconds(rdist_(rengine_)),
                             [this] { RequestData(); });
  }

  void RequestData() {
  }

private:
  Face face_;
  Scheduler scheduler_;
  GroupID gid_;

  std::random_device rdevice_;
  std::mt19937 rengine_;
  std::uniform_int_distribution<> rdist_;
};

}  // two_node_sync
}  // namespace vsync
}  // namespace ndn

