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

uint64_t spp_l3::prefetcher::issue(CACHE* cache) {
  uint64_t res = 0;

  if (!context_switch_issue_queue.empty()) {

    auto mshr_occupancy = cache->get_mshr_occupancy();
    auto rq_occupancy = cache->get_rq_occupancy().back();
    auto wq_occupancy = cache->get_wq_occupancy().back();
    auto [addr, cycle, priority, RFO_write] = context_switch_issue_queue.front();
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));
    uint64_t way = cache->get_way(addr, set);
    auto search_mshr = std::find_if(std::begin(cache->MSHR), std::end(cache->MSHR),
                       [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                         return (entry.address >> shamt) == match; 
                       });
    auto search_inflight_writes = std::find_if(std::begin(cache->inflight_writes), std::end(cache->inflight_writes),
                       [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                         return (entry.address >> shamt) == match; 
                       });

    int remaining_acc = oracle.check_pf_status(addr);

    if (remaining_acc == -1) {
      std::cout << "Trying to issue " << addr 
        << " for set " << ((addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET))) 
        << " at cycle " << cache->current_cycle << " MSHR usage: " << mshr_occupancy 
        << " queue size " << context_switch_issue_queue.size() << " wq " << wq_occupancy 
        << " rq " << rq_occupancy << std::endl;

      assert(remaining_acc != -1);
      context_switch_issue_queue.pop_front();
      return 0;
    }

    if (!RFO_write && mshr_occupancy < cache->get_mshr_size()) { 
      if (way == cache->NUM_WAY && search_mshr == cache->MSHR.end() && search_inflight_writes == cache->inflight_writes.end()) {
        res = addr;
        bool prefetched = cache->prefetch_line(addr, priority, 0, 0);

        if (prefetched) {
          context_switch_issue_queue.pop_front();

          if (context_switch_issue_queue.size() % 100000 == 0) 
            context_switch_issue_queue.shrink_to_fit();

          issued_cs_pf.insert((addr >> 6) << 6);
          total_issued_cs_pf++;

          if (debug_print) 
            std::cout << "Issued " << addr << " for set " 
              << ((addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET))) 
              << " at cycle " << cache->current_cycle << " MSHR usage: " 
              << mshr_occupancy << " queue size " << context_switch_issue_queue.size() 
              << " wq " << wq_occupancy << " rq " << rq_occupancy << std::endl;
        }
      }
      else {
        context_switch_issue_queue.pop_front();
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));

        res = 0;

        if (debug_print) 
          std::cout << "Addr " << addr << " set " << set << " way " << way << " hit in cache before issuing" << std::endl; 
      }
    }
    else if (RFO_write) {
      res = 0;
      context_switch_issue_queue.pop_front();

      if (context_switch_issue_queue.size() % 100000 == 0) 
        context_switch_issue_queue.shrink_to_fit();

      if (debug_print) 
        std::cout << "Issue WRITE operation " << addr << " for set " 
          << ((addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET))) 
          << " at cycle " << cache->current_cycle << " MSHR usage: " 
          << mshr_occupancy << " queue size " << context_switch_issue_queue.size() 
          << " wq " << wq_occupancy << " rq " << rq_occupancy << std::endl;
    }
  }

  return res;
}

void spp_l3::prefetcher::call_poll(CACHE* cache) {
  if (oracle.oracle_pf.size() == 0) 
    return; 

  std::vector<std::tuple<uint64_t, uint64_t, bool, bool>> potential_cs_v = oracle.poll(cache);
      
  for(auto potential_cs_pf : potential_cs_v) {
    // Update the prefetch queue.
    if (std::get<0>(potential_cs_pf) != 0) {
      uint64_t addr = std::get<0>(potential_cs_pf);
      uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));
      uint64_t way = cache->get_way(addr, set);

      // Remove the prefetch target from do not fill queue.
      update_do_not_fill_queue(cache->do_not_fill_address, 
                               std::get<0>(potential_cs_pf), 
                               true,
                               cache,
                               "do not fill address in runahead");

      if (way < cache->NUM_WAY) {
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
      }
      else {
        auto possible_duplicate_pf = std::find_if(std::begin(context_switch_issue_queue), std::end(context_switch_issue_queue),
                                     [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                                       return (std::get<0>(entry) >> shamt) == match; 
                                     });

        auto possible_duplicate_mshr = std::find_if(std::begin(cache->MSHR), std::end(cache->MSHR),
                                     [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                                       return (entry.address >> shamt) == match; 
                                     });

        if (possible_duplicate_pf == context_switch_issue_queue.end() 
            && possible_duplicate_mshr == cache->MSHR.end()) {
          auto pq_place_at = [demanded = std::get<1>(potential_cs_pf)](auto& entry) {return std::get<1>(entry) > demanded;};
          auto pq_insert_it = std::find_if(context_switch_issue_queue.begin(), context_switch_issue_queue.end(), pq_place_at);
          context_switch_issue_queue.emplace(pq_insert_it,std::get<0>(potential_cs_pf), std::get<1>(potential_cs_pf), std::get<2>(potential_cs_pf), std::get<3>(potential_cs_pf));
        }
      }
    }
  }
}

void spp_l3::prefetcher::erase_duplicate_entry_in_ready_queue(CACHE* cache, uint64_t addr) {
  auto possible_duplicate_pf = std::find_if(std::begin(context_switch_issue_queue), std::end(context_switch_issue_queue),
                               [match = addr >> cache->OFFSET_BITS, shamt = cache->OFFSET_BITS](const auto& entry) {
                                 return (std::get<0>(entry) >> shamt) == match; 
                               });

  if (possible_duplicate_pf != context_switch_issue_queue.end()) 
    context_switch_issue_queue.erase(possible_duplicate_pf); 
}

void spp_l3::prefetcher::update_do_not_fill_queue(std::deque<uint64_t> &dq, uint64_t addr, bool erase, CACHE* cache, std::string q_name){
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));
  auto search_res = std::find_if(dq.begin(), dq.end(), 
                    [match = addr >> (cache->OFFSET_BITS), shamt = cache->OFFSET_BITS]
                    (const auto& entry) {
                      return (entry >> shamt) == match; 
                    });

  if (erase) {
    if (search_res != dq.end()) {
      dq.erase(search_res);

      if (debug_print) 
        std::cout << "Addr " << addr << " in set " << set << " erased from " << q_name << std::endl; 
    }
  }
  else {
    if (search_res == dq.end()) {
      dq.push_back(addr); 

      if (debug_print) 
        std::cout << "Addr " << addr << " in set " << set << " pushed to " << q_name << std::endl; 
    }
  }
}

void spp_l3::prefetcher::evict_stale_blocks(CACHE* cache, uint64_t addr) {
  // Check the set to kick out blocks
  // that are not accessed for more than 2000 cycles.
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(cache->NUM_SET));

  for (size_t i = set * cache->NUM_WAY; i < (set + 1) * cache->NUM_WAY; i++) {
    // Check if the block has not been accessed for a long time.
    uint64_t stale_way = cache->NUM_WAY;

    if (oracle.cache_state[i].addr != 0 && oracle.cache_state[i].pending_accesses == 1) 
      stale_way = cache->get_way(oracle.cache_state[i].addr, set);

    if (oracle.cache_state[i].addr != 0 &&
        stale_way < cache->NUM_WAY &&
       (oracle.cache_state[i].last_access_timestamp + 8000) < cache->current_cycle) {

      //std::cout << "addr " << oracle.cache_state[i].addr << " should be evicted: " << oracle.cache_state[i].last_access_timestamp << "->" << cache->current_cycle << std::endl;

      // Set LRU bits to 0 if already filled in cache.
      champsim::operable::lru_states.push_back(std::make_tuple(set, stale_way, 0));

      if (debug_print) {
        std::cout << "set " << set << " addr " << oracle.cache_state[i].addr << " cleared at cycle " << cache->current_cycle << " set_availability " << oracle.set_availability[set] << std::endl; 
      }

      // Clear the record.
      SPP_ORACLE::acc_timestamp rollback_pf;
      rollback_pf.cycle_demanded = oracle.cache_state[i].timestamp;
      rollback_pf.set = set;
      rollback_pf.addr = oracle.cache_state[i].addr;
      rollback_pf.miss_or_hit = oracle.cache_state[i].pending_accesses;
      rollback_pf.type = oracle.cache_state[i].type;
      rollback_pf.reuse_dist_lst_timestmp = oracle.cache_state[i].last_access_timestamp;
      oracle.bkp_pf[set].push_back(rollback_pf);

      oracle.cache_state[i].addr = 0;
      oracle.cache_state[i].pending_accesses = 0;
      oracle.cache_state[i].timestamp = 0;
      oracle.cache_state[i].type = 0;
      oracle.cache_state[i].accessed = false;
      oracle.cache_state[i].last_access_timestamp = 0;
      oracle.set_availability[set]++;

      assert(oracle.set_availability[set] <= (int)cache->NUM_WAY);
    } 
  }
}
