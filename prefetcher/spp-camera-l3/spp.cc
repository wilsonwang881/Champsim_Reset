#include "spp.h"
#include "cache.h"

#include <array>
#include <iostream>
#include <map>
#include <numeric>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp_l3::prefetcher> SPP_L3;
}

void spp_l3::prefetcher::issue(CACHE* cache) {
  if (!context_switch_issue_queue.empty()) {

    auto mshr_occupancy = cache->get_mshr_occupancy();
    auto rq_occupancy = cache->get_rq_occupancy().back();
    auto wq_occupancy = cache->get_wq_occupancy().back();
    auto pq_occupancy = cache->get_pq_occupancy().back();

    if ((mshr_occupancy + rq_occupancy + wq_occupancy + pq_occupancy) < 64) {
      auto [addr, priority, cycle] = context_switch_issue_queue.front();
      bool prefetched = cache->prefetch_line(addr, priority, 0, 0);

      if (prefetched) {
        context_switch_issue_queue.pop_front();
        issued_cs_pf.insert((addr >> 6) << 6);
        total_issued_cs_pf++;

        //std::cout << "Issued " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy << " rq " << rq_occupancy << " pq " << pq_occupancy << std::endl;
      }
    }
  }
}


