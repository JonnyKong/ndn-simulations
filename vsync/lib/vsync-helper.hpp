/* -*- Mode:C++; c-file-style:"google"; indent-tabs-mode:nil; -*- */

#ifndef NDN_VSYNC_INTEREST_HELPER_HPP_
#define NDN_VSYNC_INTEREST_HELPER_HPP_

#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>

#include <ndn-cxx/name.hpp>

#include "vsync-common.hpp"

namespace ndn {
namespace vsync {


// Helpers for version vector processing
inline std::string VersionVectorToString(const VersionVector& v) {
  std::string s = "[";
  for (auto entry: v) {
    s += to_string(entry.first) + ":" + to_string(entry.second) + ",";
  }
  s += "]";
  return s;
}

inline std::string EncodeVVToName(const VersionVector& v) {
  std::string vv_encode = "";
  for (auto entry: v) {
    vv_encode += (to_string(entry.first) + "-" + to_string(entry.second) + "_");
  }
  return vv_encode;
}

inline VersionVector DecodeVVFromName(const std::string& vv_encode) {
  int start = 0;
  VersionVector vv;
  for (size_t i = 0; i < vv_encode.size(); ++i) {
    if (vv_encode[i] == '_') {
      std::string str = vv_encode.substr(start, i - start);
      size_t sep = str.find("-");
      NodeID nid = std::stoull(str.substr(0, sep));
      uint64_t seq = std::stoull(str.substr(sep + 1));
      vv[nid] = seq;
      start = i + 1;
    }
  }
  return vv;
}

inline void EncodeVV(const VersionVector& v, proto::VV* vv_proto) {
  for (auto item: v) {
    auto* entry = vv_proto->add_entry();
    entry->set_nid(item.first);
    entry->set_seq(item.second);
  }
}

inline void EncodeVV(const VersionVector& v, std::string& out) {
  proto::VV vv_proto;
  EncodeVV(v, &vv_proto);
  vv_proto.AppendToString(&out);
}

inline VersionVector DecodeVV(const proto::VV& vv_proto) {
  VersionVector vv;
  for (int i = 0; i < vv_proto.entry_size(); ++i) {
    const auto& entry = vv_proto.entry(i);
    auto nid = entry.nid();
    auto seq = entry.seq();
    vv[nid] = seq;
  }
  return vv;
}

inline VersionVector DecodeVV(const void* buf, size_t buf_size) {
  proto::VV vv_proto;
  if (!vv_proto.ParseFromArray(buf, buf_size)) {
    VersionVector res;
    return res;
  }
  return DecodeVV(vv_proto);
}

// Naming conventions for interests and data
// TBD 
// actually, the [state-vector] is no needed to be carried because the carried data contains the vv.
// but lixia said we maybe should remove the vv from data.
inline Name MakeSyncNotifyName(const NodeID& nid, std::string encoded_vv, int64_t timestamp) {
  // name = /[syncNotify_prefix]/[nid]/[state-vector]/[heartbeat-vector]
  Name n(kSyncNotifyPrefix);
  n.appendNumber(nid).append(encoded_vv).appendNumber(timestamp);
  return n;
}

inline Name MakeDataName(const NodeID& nid, uint64_t seq) {
  // name = /[vsyncData_prefix]/[node_id]/[seq]/%0
  Name n(kSyncDataPrefix);
  n.appendNumber(nid).appendNumber(seq).appendNumber(0);
  return n;
}

inline Name MakeBundledDataName(const NodeID& nid, std::string missing_data_vector) {
  // name = /[bundledData_prefix]/[node_id]/[missing_data_vector]/%0
  Name n(kBundledDataPrefix);
  n.appendNumber(nid).append(missing_data_vector).appendNumber(0);
  return n;
}

inline Name MakeBeaconName(uint64_t nid) {
  Name n(kBeaconPrefix);
  n.appendNumber(nid).appendNumber(0).appendNumber(0);
  return n;
}

// inline Name MakeHeartbeatName(const NodeID& nid, std::string encoded_hv, std::string tag) {
//   Name n(kHeartbeatPrefix);
//   n.appendNumber(nid).append(encoded_hv).appendNumber(0);
//   return n;
// }

// inline Name MakeBeaconFloodName(uint64_t sender, uint64_t initializer, uint64_t seq) {
//   Name n(kBeaconFloodPrefix);
//   n.appendNumber(sender).appendNumber(initializer).appendNumber(seq);
//   return n;
// }

inline uint64_t ExtractNodeID(const Name& n) {
  return n.get(-3).toNumber();
}

inline std::string ExtractEncodedVV(const Name& n) {
  return n.get(-2).toUri();
}

inline std::string ExtractEncodedMV(const Name& n) {
  return n.get(-2).toUri();
}

inline uint64_t ExtractSequence(const Name& n) {
  return n.get(-2).toNumber();
}

inline std::string ExtractEncodedHV(const Name& n) {
  return n.get(-2).toUri();
}

inline std::string ExtractTag(const Name& n) {
  return n.get(-1).toUri();
}

inline uint64_t ExtractBeaconSender(const Name& n) {
  return n.get(-3).toNumber();
}

inline uint64_t ExtractBeaconInitializer(const Name& n) {
  return n.get(-2).toNumber();
}

inline uint64_t ExtractBeaconSeq(const Name& n) {
  return n.get(-1).toNumber();
}

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_INTEREST_HELPER_HPP_
