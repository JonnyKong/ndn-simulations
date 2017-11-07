#ifndef NDNSIM_BROADCAST_STRATEGY_HPP
#define NDNSIM_BROADCAST_STRATEGY_HPP

#include <boost/random/mersenne_twister.hpp>
#include "face/face.hpp"
#include "fw/strategy.hpp"

namespace nfd {
namespace fw {

class BroadcastStrategy : public Strategy {
public:
	BroadcastStrategy(Forwarder& forwarder, const Name& name = STRATEGY_NAME);

	virtual void
	afterReceiveInterest(const Face& inFace, const Interest& interest,
                         const shared_ptr<pit::Entry>& pitEntry) override;

	bool canForwardTo(const pit::Entry& pitEntry, const Face& face);

public:
	static const Name STRATEGY_NAME;
};

} // namespace fw
} // namespace nfd

#endif  //  NDNSIM_BROADCAST_STRATEGY_HPP
