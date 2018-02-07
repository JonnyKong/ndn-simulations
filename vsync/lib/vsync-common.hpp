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
using VersionVector = std::unordered_map<uint64_t, uint64_t>;
using GroupID = std::string;

static const Name kSyncNotifyPrefix = Name("/ndn/syncNotify");
static const Name kIncomingSyncPrefix = Name("/ndn/incomingSync");
static const Name kSyncDataPrefix = Name("/ndn/vsyncData");

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_COMMON_HPP_
