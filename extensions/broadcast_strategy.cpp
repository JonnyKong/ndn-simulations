#include "broadcast_strategy.hpp"

#include <boost/random/uniform_int_distribution.hpp>

#include <ndn-cxx/util/random.hpp>

#include "fw/algorithm.hpp"
#include "core/logger.hpp"

namespace nfd {
namespace fw {

NFD_LOG_INIT("BroadcastStrategy");

const Name BroadcastStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/broadcast/%FD%01");

NFD_REGISTER_STRATEGY(BroadcastStrategy);

BroadcastStrategy::BroadcastStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

/*
void 
BroadcastStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
    Face& outFace = it->getFace();
    NFD_LOG_DEBUG("candidate outFace=" << outFace.getId());
    if (!wouldViolateScope(inFace, interest, outFace) &&
        canForwardTo(*pitEntry, outFace)) {
      NFD_LOG_DEBUG("send interest from inFace=" << inFace.getId() << " to outFace=" << outFace.getId());
      this->sendInterest(pitEntry, outFace, interest);
    }
  }

  if (!hasPendingOutRecords(*pitEntry)) {
    this->rejectPendingInterest(pitEntry);
  }
}
*/

void 
BroadcastStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  // if inFace is appFace, don't forward to self
  // TBD: how to distinguish from application and others!
  if (inFace.getScope() == ndn::nfd::FACE_SCOPE_LOCAL) {
    NFD_LOG_DEBUG("local scope! inFace = " << inFace.getId());
    for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
      Face& outFace = it->getFace();
      NFD_LOG_DEBUG("next face = " << outFace.getId());
      if (!wouldViolateScope(inFace, interest, outFace) &&
          inFace.getId() != outFace.getId()) {
        NFD_LOG_DEBUG("send interest from inFace=" << inFace.getId() << " to outFace=" << outFace.getId());
        this->sendInterest(pitEntry, outFace, interest);
      }
    }
  }
  else {
    for (fib::NextHopList::const_iterator it = nexthops.begin(); it != nexthops.end(); ++it) {
      Face& outFace = it->getFace();
      NFD_LOG_DEBUG("candidate outFace=" << outFace.getId());
      if (!wouldViolateScope(inFace, interest, outFace) //&&
          /*canForwardTo(*pitEntry, outFace)*/) {
        NFD_LOG_DEBUG("send interest from inFace=" << inFace.getId() << " to outFace=" << outFace.getId());
        this->sendInterest(pitEntry, outFace, interest);
      }
    }
  }

  if (!hasPendingOutRecords(*pitEntry)) {
    this->rejectPendingInterest(pitEntry);
  }
}

bool
BroadcastStrategy::canForwardTo(const pit::Entry& pitEntry, const Face& face)
{
  time::steady_clock::TimePoint now = time::steady_clock::now();

  // if previously the same interest has been sent through this outFace
  bool hasUnexpiredOutRecord = std::any_of(pitEntry.out_begin(), pitEntry.out_end(),
    [&face, &now] (const pit::OutRecord& outRecord) {
      return &outRecord.getFace() == &face && outRecord.getExpiry() >= now;
    });
  if (hasUnexpiredOutRecord) {
    return false;
  }

  bool hasUnexpiredOtherInRecord = std::any_of(pitEntry.in_begin(), pitEntry.in_end(),
    [&face, &now] (const pit::InRecord& inRecord) {
      //return &inRecord.getFace() != &face && inRecord.getExpiry() >= now;
      return inRecord.getExpiry() >= now;
    });
  if (!hasUnexpiredOtherInRecord) {
    return false;
  }

  return true;
}


} // namespace fw
} // namespace nfd
