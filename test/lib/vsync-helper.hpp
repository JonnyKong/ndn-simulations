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
  std::string s(1, '[');
  s.append(std::accumulate(std::next(v.begin()), v.end(),
                           std::to_string(v[0]),
                           [](std::string a, uint64_t b) {
                             return a + ',' + std::to_string(b);
                           }))
      .append(1, ']');
  return s;
}

inline void EncodeVV(const VersionVector& v, proto::VV* vv_proto) {
  for (const auto& seq: v) {
    vv_proto->add_entry(seq);
  }
}

inline void EncodeVV(const VersionVector& v, std::string& out) {
  proto::VV vv_proto;
  EncodeVV(v, &vv_proto);
  vv_proto.AppendToString(&out);
}

inline VersionVector DecodeVV(const proto::VV& vv_proto) {
  VersionVector vv(vv_proto.entry_size(), 0);
  for (int i = 0; i < vv_proto.entry_size(); ++i) {
    vv[i] = vv_proto.entry(i);
  }
  return vv;
}

inline VersionVector DecodeVV(const void* buf, size_t buf_size) {
  proto::VV vv_proto;
  if (!vv_proto.ParseFromArray(buf, buf_size)) return VersionVector(0);
  return DecodeVV(vv_proto);
}

inline void EncodeDL(const std::vector<std::pair<uint32_t, std::string>>& data_list, proto::DL* dl_proto) {
  for (const auto& data: data_list) {
    auto* entry = dl_proto->add_entry();
    entry->set_type(data.first);
    entry->set_content(data.second);
  }
}

inline void EncodeDL(const std::vector<std::pair<uint32_t, std::string>>& data_list, std::string& out) {
  proto::DL dl_proto;
  EncodeDL(data_list, &dl_proto);
  dl_proto.AppendToString(&out);
}

inline std::vector<std::pair<uint32_t, std::string>> DecodeDL(const proto::DL& dl_proto) {
  std::vector<std::pair<uint32_t, std::string>> data_list;
  for (int i = 0; i < dl_proto.entry_size(); ++i) {
    const auto& entry = dl_proto.entry(i);
    data_list.push_back(std::pair<uint32_t, std::string>(entry.type(), entry.content()));
  }
  return data_list;
}

inline std::vector<std::pair<uint32_t, std::string>> DecodeDL(const void* buf, size_t buf_size) {
  proto::DL dl_proto;
  if (!dl_proto.ParseFromArray(buf, buf_size)) return std::vector<std::pair<uint32_t, std::string>>(0);
  return DecodeDL(dl_proto);
}

// if l < r, return true; else return false
struct VVCompare {
  bool operator()(const VersionVector& l, const VersionVector& r) const {
    if (l.size() != r.size()) return false;
    size_t equal_count = 0;
    auto lv = l.begin(), rv = r.begin();
    while(lv != l.end()) { 
      if(*lv > *rv) return false;
      else if(*lv == *rv) equal_count ++;
      lv = std::next(lv);
      rv = std::next(rv);
    }
    if (equal_count == l.size())
      return false;
    else
      return true;
  }
};

// Helpers for interest processing

inline Name MakeSyncInterestName(const GroupID& gid, const NodeID& nid, const std::string& encoded_vv) {
  // name = /[vsync_prefix]/[group_id]/[node_id]/[encoded_version_vector]
  Name n(kSyncPrefix);
  n.append(gid).appendNumber(nid).append(encoded_vv);
  return n;
}

inline Name MakeWakeupInterestName(const GroupID& gid, const NodeID& nid) {
  // name = /[wakeup_prefix]/[group_id]/[node_id]/%00
  Name n(kWakeupPrefix);
  n.append("wakeup").append(gid).appendNumber(nid).appendNumber(0);
  return n;
}

inline GroupID ExtractGroupID(const Name& n) {
  GroupID group_id = n.get(-3).toUri();
  return group_id;
}

inline uint64_t ExtractNodeID(const Name& n) {
  return n.get(-2).toNumber();
}

inline std::string ExtractEncodedVV(const Name& n) {
  return n.get(-1).toUri();
}


// Helpers for data processing

inline Name MakeDataListName(const GroupID& gid, const NodeID& nid,
                             uint64_t start_seq, uint64_t end_seq) {
  // name = /[vsyncData_prefix]/[group_id]/[node_id]/[start_seq]/[end_seq]
  Name n(kSyncDataListPrefix);
  n.append(gid).appendNumber(nid).appendNumber(start_seq).appendNumber(end_seq);
  return n;
}

inline Name MakeDataName(const GroupID& gid, const NodeID& nid, uint64_t seq) {
  Name n(kSyncDataPrefix);
  n.append(gid).appendNumber(nid).appendNumber(seq);
  return n;
}

inline uint64_t ExtractStartSequenceNumber(const Name& n) {
  return n.get(-2).toNumber();
}

inline uint64_t ExtractEndSequenceNumber(const Name& n) {
  return n.get(-1).toNumber();
}

inline uint64_t ExtractNodeIDFromData(const Name& n) {
  return n.get(-3).toNumber();
}

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_INTEREST_HELPER_HPP_
