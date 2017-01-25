#include "random-load-balancer-strategy.hpp"

#include <boost/random/uniform_int_distribution.hpp>

#include <ndn-cxx/util/random.hpp>

#include "core/logger.hpp"

NFD_LOG_INIT("RandomLoadBalancerStrategy");

namespace nfd {
namespace fw {

const Name
  RandomLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/random-load-balancer");

RandomLoadBalancerStrategy::RandomLoadBalancerStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

RandomLoadBalancerStrategy::~RandomLoadBalancerStrategy()
{
}

static bool
canForwardToNextHop(shared_ptr<pit::Entry> pitEntry, const fib::NextHop& nexthop)
{
  return pitEntry->canForwardTo(*nexthop.getFace());
}

static bool
hasFaceForForwarding(const fib::NextHopList& nexthops, shared_ptr<pit::Entry>& pitEntry)
{
  return std::find_if(nexthops.begin(), nexthops.end(), bind(&canForwardToNextHop, pitEntry, _1))
         != nexthops.end();
}

void
RandomLoadBalancerStrategy::afterReceiveInterest(const Face& inFace, const Interest& interest,
                                                 shared_ptr<fib::Entry> fibEntry,
                                                 shared_ptr<pit::Entry> pitEntry)
{
  NFD_LOG_TRACE("afterReceiveInterest");

  if (pitEntry->hasUnexpiredOutRecords()) {
    // not a new Interest, don't forward
    return;
  }

  const fib::NextHopList& nexthops = fibEntry->getNextHops();

  // Ensure there is at least 1 Face is available for forwarding
  if (!hasFaceForForwarding(nexthops, pitEntry)) {
    this->rejectPendingInterest(pitEntry);
    return;
  }

  fib::NextHopList::const_iterator selected;
  do {
    boost::random::uniform_int_distribution<> dist(0, nexthops.size() - 1);
    const size_t randomIndex = dist(m_randomGenerator);

    uint64_t currentIndex = 0;

    for (selected = nexthops.begin(); selected != nexthops.end() && currentIndex != randomIndex;
         ++selected, ++currentIndex) {
    }
  } while (!canForwardToNextHop(pitEntry, *selected));

  this->sendInterest(pitEntry, selected->getFace());
}

} // namespace fw
} // namespace nfd