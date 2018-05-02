/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_COMMON_HPP_
#define NDN_VSYNC_COMMON_HPP_

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <unordered_map>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/time.hpp>

#include "vsync-message.pb.h"

namespace ndn {
namespace vsync {

// Type and constant declarations for VectorSync

using NodeID = uint64_t;
using VersionVector = std::unordered_map<NodeID, uint64_t>;
using heartbeatVector = std::unordered_map<NodeID, uint64_t>;
using GroupID = std::string;

static const Name kSyncNotifyPrefix = Name("/ndn/syncNotify");
static const Name kSyncDataPrefix = Name("/ndn/vsyncData");
static const Name kBundledDataPrefix = Name("/ndn/bundledData");
static const Name kGetNDNTraffic = Name("/ndn/getNDNTraffic");
static const Name kBeaconPrefix = Name("/ndn/beacon");

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_COMMON_HPP_
