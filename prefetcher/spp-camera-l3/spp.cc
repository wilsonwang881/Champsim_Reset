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
    auto [addr, priority, cycle, RFO_write] = context_switch_issue_queue.front();

    if (!RFO_write && 
        ((mshr_occupancy + rfo_write_mshr_cap) < cache->get_mshr_size())) {
      
      bool prefetched = cache->prefetch_line(addr, priority, 0, 0);

      if (prefetched) {
        context_switch_issue_queue.pop_front();
        issued_cs_pf.insert((addr >> 6) << 6);
        total_issued_cs_pf++;

        if (debug_print) 
          std::cout << "Issued " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy << " rq " << rq_occupancy << std::endl;
      }
    }
    else if (RFO_write && rfo_write_mshr_cap < (cache->get_mshr_size() - 1)) {
      rfo_write_mshr_cap++;
      rfo_write_addr.insert(addr);
      context_switch_issue_queue.pop_front();

      if (debug_print) 
        std::cout << "WRITE operation " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy << " rq " << rq_occupancy << " rfo_write_mshr_cap = " << rfo_write_mshr_cap << std::endl;
    }
  }
}


