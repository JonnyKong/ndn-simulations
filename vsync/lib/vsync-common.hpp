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
static const Name kSyncDataListPrefix = Name("/ndn/vsyncDatalist");
static const Name kSyncDataPrefix = Name("/ndn/vsyncData");

static const Name kProbePrefix = Name("/ndn/sleepingProbe");
static const Name kProbeIntermediatePrefix = Name("/ndn/sleepingProbeIntermediate");
static const Name kReplyPrefix = Name("/ndn/sleepingReply");
static const Name kSleepCommandPrefix = Name("/ndn/sleepingCommand");

static const Name kIncomingDataPrefix = Name("/ndn/incomingData");
static const Name kIncomingInterestPrefix = Name("/ndn/incomingInterest");

static const Name kSyncACKPrefix = Name("/ndn/syncACK");
static const Name kIncomignSyncACKPrefix = Name("/ndn/incomingSyncACK");

static const Name kLocalhostSleepingCommand = Name("/localhost/nfd/sleeping/go-to-sleep");
static const Name kLocalhostWakeupCommand = Name("/localhost/nfd/sleeping/wake-up");
static const Name kGetOutVsyncInfoCommand = Name("/localhost/nfd/getOutVsyncInfo");

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_COMMON_HPP_
