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

inline std::string EncodeVV(const VersionVector& v) {
  std::string vv_encode = "";
  for (auto seq: v) {
    vv_encode += to_string(seq) + "-";
  }
  return vv_encode;
}

inline VersionVector DecodeVV(const std::string& vv_encode) {
  int start = 0;
  VersionVector vv;
  for (size_t i = 0; i < vv_encode.size(); ++i) {
    if (vv_encode[i] == '-') {
      vv.push_back(std::stoull(vv_encode.substr(start, i - start)));
      start = i + 1;
    }
  }
  return vv;
}

/*
inline void EncodeVV(const VersionVector& v, proto::VV* vv_proto) {
  for (const auto& seq: v) {
    vv_proto->add_entry(seq);
  }
}

inline void EncodeVV(const VersionVector& v, std::string& out) {
  proto::VV vv_proto;
  EncodeVV(v, &vv_proto);
  // vv_proto.SerializeAsString();
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
  if (!vv_proto.ParseFromArray(buf, buf_size)) {
    std::cout << "VersionVector ParseFromArray fail!" << std::endl;
    return VersionVector(0);
  }
  return DecodeVV(vv_proto);
}
*/

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

// sync interest
inline Name MakeSyncInterestName(const GroupID& gid, const NodeID& nid, const std::string& encoded_vv, const NodeID& syncup_node) {
  // name = /[vsync_prefix]/[group_id]/[node_id]/[syncup_node]/[encoded_version_vector]
  Name n(kSyncPrefix);
  n.append(gid).appendNumber(nid).appendNumber(syncup_node).append(encoded_vv);
  return n;
}

// probe interest
inline Name MakeProbeInterestName(const GroupID& gid) {
  // name = /[probe_prefix]/[group_id]/%00/%00/%00
  // ?????question : [node_id] is not needed here? 
  Name n(kProbePrefix);
  n.append(gid).appendNumber(0).appendNumber(0).appendNumber(0);
  return n;
}

// reply interest
inline Name MakeReplyInterestName(const GroupID& gid, const NodeID& nid, uint64_t sleeping_time) {
  // name = /[reply_prefix]/[group_id]/[node_id]/[sleeping_time]/%00
  Name n(kReplyPrefix);
  n.append(gid).appendNumber(nid).appendNumber(sleeping_time).appendNumber(0);
  return n;
}

// sleepCommand interest
inline Name MakeSleepCommandName(const GroupID& gid, const NodeID& sleep_node, const NodeID& syncup_node) {
  // name = /[sleep_command_prefix]/[group_id]/[sleep_node]/[syncup_node]/%00
  Name n(kSleepCommandPrefix);
  n.append(gid).appendNumber(sleep_node).appendNumber(syncup_node).appendNumber(0);
  return n;
}

// syncACK interest
inline Name MakeSyncACKInterestName(const GroupID& gid, const NodeID& sync_sender) {
  // name = /[sync_ack_interest_prefix]/[group_id]/[sync_sender]/[timestamp]/%00
  time::system_clock::time_point cur = time::system_clock::now();
  std::string timestamp = to_string(time::system_clock::to_time_t(cur));
  Name n(kSyncACKPrefix);
  n.append(gid).appendNumber(sync_sender).append(timestamp).appendNumber(0);
  return n;
}

// data interest
inline Name MakeDataListName(const GroupID& gid, const NodeID& nid,
                             uint64_t start_seq, uint64_t end_seq) {
  // name = /[vsyncDatalist_prefix]/[group_id]/[node_id]/[start_seq]/[end_seq]
  Name n(kSyncDataListPrefix);
  n.append(gid).appendNumber(nid).appendNumber(start_seq).appendNumber(end_seq);
  return n;
}

inline Name MakeDataName(const GroupID& gid, const NodeID& nid, uint64_t seq) {
  // name = [vsyncData_prefix]/[group_id]/[node_id]/[seq]
  Name n(kSyncDataPrefix);
  n.append(gid).appendNumber(nid).appendNumber(seq);
  return n;
}

inline GroupID ExtractGroupID(const Name& n) {
  GroupID group_id = n.get(-4).toUri();
  return group_id;
}

inline uint64_t ExtractNodeID(const Name& n) {
  return n.get(-3).toNumber();
}

inline uint64_t ExtractSyncupNode(const Name& n) {
  return n.get(-2).toNumber();
}

inline std::string ExtractEncodedVV(const Name& n) {
  return n.get(-1).toUri();
}


inline uint64_t ExtractStartSequenceNumber(const Name& n) {
  return n.get(-2).toNumber();
}

inline uint64_t ExtractEndSequenceNumber(const Name& n) {
  return n.get(-1).toNumber();
}

inline uint64_t ExtractSleepingTime(const Name& n) {
  return n.get(-2).toNumber();
}

}  // namespace vsync
}  // namespace ndn

#endif  // NDN_VSYNC_INTEREST_HELPER_HPP_
