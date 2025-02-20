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


void spp_l3::prefetcher::issue(CACHE* cache)
{
  // WL: issue context switch prefetches first 
  //if (!reset_misc::dq_prefetch_communicate.empty()) {
  if (!context_switch_issue_queue.empty()) {

    auto q_occupancy = cache->get_pq_occupancy();
    auto mshr_occupancy = cache->get_mshr_occupancy();

    //if (q_occupancy < cache->get_pq_size())  // q_occupancy[2] <= 15 && 
    if (mshr_occupancy < 32)
    {

      auto [addr, priority, cycle] = context_switch_issue_queue.front();
      bool prefetched = cache->prefetch_line(addr, priority, 0);

      issue_queue.clear();

      if (prefetched) {
        context_switch_issue_queue.pop_front();
        issued_cs_pf.insert((addr >> 6) << 6);
        total_issued_cs_pf++;

        //std::cout << "Issued " << addr << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(1024))) << " at cycle " << cache->current_cycle << std::endl;
      }
    }
  }
}


