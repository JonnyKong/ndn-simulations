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
inline Name MakeSyncNotifyName(std::string encoded_vv, const Block& data_block) {
  // name = /[syncNotify_prefix]/%0/[state-vector]/[data]/[hop_count]
  Name n(kSyncNotifyPrefix);
  n.appendNumber(0).append(encoded_vv).append(data_block);
  return n;
}

inline Name MakeDataName(const NodeID& nid, uint64_t seq) {
  // name = /[vsyncData_prefix]/[node_id]/[seq]/%0
  Name n(kSyncDataPrefix);
  n.appendNumber(nid).appendNumber(seq).appendNumber(0);
  return n;
}

inline Name MakeHeartbeatName(const NodeID nid) {
  // name = /[heartbeat_prefix]/[node_id]/%0/%0
  Name n(kHeartbeatPrefix);
  n.appendNumber(nid).appendNumber(0).appendNumber(0);
  return n;
}

inline uint64_t ExtractNodeID(const Name& n) {
  return n.get(-3).toNumber();
}

inline std::string ExtractEncodedVV(const Name& n) {
  return n.get(-2).toUri();
}

inline uint64_t ExtractSequence(const Name& n) {
  return n.get(-2).toNumber();
}

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_INTEREST_HELPER_HPP_
