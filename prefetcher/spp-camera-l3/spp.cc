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
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));
    uint64_t way = cache->get_way(addr, set);

    if (!RFO_write && 
        ((mshr_occupancy) < cache->get_mshr_size())) {
      
      if (way == cache->NUM_WAY) {
        bool prefetched = cache->prefetch_line(addr, priority, 0, 0);

        if (prefetched) {
          context_switch_issue_queue.pop_front();
          issued_cs_pf.insert((addr >> 6) << 6);
          total_issued_cs_pf++;

          if (debug_print) 
            std::cout << "Issued " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy << " rq " << rq_occupancy << std::endl;
        }
      }
      else 
        context_switch_issue_queue.pop_front();
    }
    else if (RFO_write) {
      rfo_write_addr.insert(addr);
      context_switch_issue_queue.pop_front();

      if (debug_print) 
        std::cout << "Issue WRITE operation " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy << " rq " << rq_occupancy << std::endl;
    }
  }
}

bool spp_l3::prefetcher::call_poll() {
  std::tuple<uint64_t, uint64_t, bool, bool> potential_cs_pf = oracle.poll();
      
  // Update the prefetch queue.
  if (std::get<0>(potential_cs_pf) != 0) {
    auto pq_place_at = [demanded = std::get<1>(potential_cs_pf)](auto& entry) {return std::get<2>(entry) > demanded;};
    auto pq_insert_it = std::find_if(context_switch_issue_queue.begin(), context_switch_issue_queue.end(), pq_place_at);
    context_switch_issue_queue.emplace(pq_insert_it,std::get<0>(potential_cs_pf), std::get<2>(potential_cs_pf), std::get<1>(potential_cs_pf), std::get<3>(potential_cs_pf));
    
    return true;
  }
  else 
    return false;
}


