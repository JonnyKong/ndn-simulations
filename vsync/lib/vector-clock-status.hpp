#ifndef NDN_VECTOR_CLOCK_HPP_
#define NDN_VECTOR_CLOCK_HPP_

#include "vsync-common.hpp"
#include "vsync-helper.hpp"

namespace ndn {
namespace vsync {

static const uint32_t group_size = 5;
class TotalVectorClockStatus {
  public:
   static TotalVectorClockStatus& getInstace() {
   	 static TotalVectorClockStatus vc_status;
     return vc_status;
   }

   static void setTotalVectorClock(uint32_t index, uint64_t seq) {
     vc[index] = seq;
   }

   static VersionVector getTotalVersionVector() {
   	return vc;
   }

   private:
   	static VersionVector vc;

   	TotalVectorClockStatus() {
   	  vc = VersionVector(group_size, 0);
   	}
};

}  // ndn
}  // vsync

#endif