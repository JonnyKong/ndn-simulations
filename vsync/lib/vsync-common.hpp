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
using VersionVector = std::vector<uint64_t>;
using GroupID = std::string;

static const Name kSyncPrefix = Name("/ndn/vsync");
static const Name kSyncDataListPrefix = Name("/ndn/vsyncDataList");
static const Name kSyncDataPrefix = Name("/ndn/vsyncData");

static const Name kWakeupPrefix = Name("/ndn/sleepingWakeup");
static const Name kProbePrefix = Name("/ndn/sleepingProbe");
static const Name kReplyPrefix = Name("/ndn/sleepingReply");
static const Name kSleepCommandPrefix = Name("/ndn/sleepingCommand");

static const Name kLocalhostSleepingCommand = Name("/localhost/nfd/sleeping/go-to-sleep");
static const Name kLocalhostWakeupCommand = Name("/localhost/nfd/sleeping/wake-up");

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_COMMON_HPP_
