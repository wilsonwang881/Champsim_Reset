#include "cache.h"
#include "spp.h"

#include <algorithm>
#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp_l3::prefetcher> SPP_L3;
}

void CACHE::prefetcher_initialize() {
  std::cout << std::endl;
  std::cout << "Oracle prefetcher at " << this->NAME << std::endl;
  std::cout << std::endl;

  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE) {
    pref.oracle.init();

    while (true) {

      if (pref.oracle.ORACLE_ACTIVE && pref.oracle.oracle_pf.size() > 0) {
      
        // Update the prefetch queue.
        if (!pref.call_poll()) 
          break;
      }
      else 
        break;
    }

    std::cout << "Oracle: try to prefetch " << pref.context_switch_issue_queue.size() << "/" << (NUM_WAY * NUM_SET) << " blocks at the beginning." << std::endl;
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];
  base_addr = (base_addr>> 6) << 6;
  bool original_hit = cache_hit;

  if (base_addr != pref.last_handled_addr) 
    pref.last_handled_addr = base_addr; 
  else 
    return metadata_in;

  if (pref.debug_print) 
    std::cout << "Hit/miss " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << " MSHR usage " << this->get_mshr_occupancy() << std::endl;

  // Return if a demand misses and cannot merge in MSHR and MSHR is full.
  /*
  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit) && !cache_hit && type != 3) {
    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr == this->MSHR.end() && this->get_mshr_occupancy() == this->get_mshr_size()) 
      return metadata_in; 
  }
  */

  bool found_in_MSHR = false;

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit) && !cache_hit) {
    bool found_in_pending_queue = false;
    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr != this->MSHR.end()) 
      found_in_pending_queue = true;  
    else {
      auto search_pq = std::find_if(std::begin(this->internal_PQ), std::end(this->internal_PQ),
                                   [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                     return (entry.address >> shamt) == match; 
                                   });

      if (search_pq != this->internal_PQ.end()) {
        found_in_pending_queue = true;
        this->internal_PQ.erase(search_pq); 
      }
      else {
        auto search_pending_pf = std::find_if(std::begin(pref.context_switch_issue_queue), std::end(pref.context_switch_issue_queue),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (std::get<0>(entry) >> shamt) == match; 
                                 });

        if (search_pending_pf != pref.context_switch_issue_queue.end()) {
          pref.context_switch_issue_queue.erase(search_pending_pf); 
          found_in_pending_queue = true;
        }
        else {
          auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf), std::end(pref.oracle.oracle_pf),
                                  [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.addr >> shamt) == match; 
                                  });

          if (search_oracle_pq != pref.oracle.oracle_pf.end()) {
            uint64_t set = search_oracle_pq->set;
            uint64_t way = pref.oracle.check_set_pf_avail(search_oracle_pq->addr);

            if (pref.debug_print) 
              std::cout << "Found addr " << search_oracle_pq->addr << " set " << search_oracle_pq->set << " counter " << search_oracle_pq->miss_or_hit << " set_availability " << pref.oracle.set_availability[search_oracle_pq->set] << " found way " << way << std::endl;

            if (way < NUM_WAY) {

              if (pref.oracle.cache_state[set * NUM_WAY + way].addr != search_oracle_pq->addr) 
                pref.oracle.set_availability[set]--;

              pref.oracle.cache_state[set * NUM_WAY + way].pending_accesses += (int)(search_oracle_pq->miss_or_hit);
              pref.oracle.cache_state[set * NUM_WAY + way].addr = search_oracle_pq->addr;
              pref.oracle.cache_state[set * NUM_WAY + way].require_eviction = search_oracle_pq->require_eviction;
              assert(pref.oracle.set_availability[set] >= 0);
              pref.oracle.oracle_pf.erase(search_oracle_pq); 
              found_in_pending_queue = true;
            }
          }
        }
      }
    }

    if (found_in_pending_queue) {
      useful_prefetch = true; 
      cache_hit = true;
      pref.oracle.hit_in_MSHR++;
      found_in_MSHR = true;

      if (pref.debug_print) 
        std::cout << "Hit in MSHR ? " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;
    }
  }

  if (useful_prefetch && !(type == 2 && cache_hit)) 
    pref.oracle.update_demand(this->current_cycle, base_addr, 0, 0, type);
  else if (!(type == 2 && cache_hit)) 
      pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit, 1, type);

  if (type == 3) {
    int erased = 1; //pref.rfo_write_addr.erase(base_addr);

    if (pref.debug_print) 
      std::cout << "WRITE operation " << base_addr << " update erased ? " << erased << std::endl; 

    if (erased) {
      found_in_MSHR = true;
      cache_hit = true;
    } 
  }

  if (pref.oracle.ORACLE_ACTIVE && type != 2 && type != 3 && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if ((remaining_acc == 0) && evict) {  
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way(base_addr, set);

      auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                   [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                     return (entry.address >> shamt) == match; 
                                   });

      if (search_mshr != this->MSHR.end()) 
        found_in_MSHR = true;  

      if (found_in_MSHR) {
        if (pref.debug_print) 
          std::cout << "set " << set << " addr " << base_addr << " pushed to do not fill set" << std::endl; 

        this->do_not_fill_address.push_back(base_addr);
      } 

      if (way < NUM_WAY) {
        if (pref.debug_print) 
          std::cout << "set " << set << " addr " << base_addr << " cleared LRU bits" << std::endl; 

        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
      }

      pref.call_poll();
    }
    else if (remaining_acc > 0) {
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way(base_addr, set);

      if (way < NUM_WAY) 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    }
  }
  else if (pref.oracle.ORACLE_ACTIVE && type != 2 && type == 3 && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int current_acc = pref.oracle.check_pf_status(base_addr);

    // Last access to the prefetched block used.
    if (current_acc == 1 && evict) {  
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way(base_addr, set);

      //if (pref.debug_print) 

      if (found_in_MSHR && !original_hit) {
        pref.pending_write_fills.insert(base_addr); 
        std::cout << "set " << set << " addr " << base_addr << " pushed to wait fill set" << std::endl; 
      }
      else if (original_hit) {
       pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

       if (way < NUM_WAY && original_hit) {
        if (pref.debug_print) 
          std::cout << "set " << set << " addr " << base_addr << " cleared LRU bits" << std::endl; 

         champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0)); 
       }

       pref.call_poll();
      }
    }
    else if (current_acc > 1) {
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way(base_addr, set);
      pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

      if (way < NUM_WAY) 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    }
  }

  if ((pref.issued_cs_pf.find(base_addr) != pref.issued_cs_pf.end()) && useful_prefetch) {
    pref.issued_cs_pf_hit++; 
    pref.issued_cs_pf.erase(base_addr);
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];
  addr = (addr >> 6) << 6;

  if (pref.debug_print) 
    std::cout << "Filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << " remaining access " << pref.oracle.check_pf_status(addr) << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && pref.oracle.check_pf_status(addr) > 0) {

    // Clear the counter of a WRITE when the WRITE actually fills.
    if (auto search = pref.pending_write_fills.find(addr); search != pref.pending_write_fills.end()) {
      champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
      pref.pending_write_fills.erase(search);
      pref.oracle.update_pf_avail(addr, current_cycle - pref.oracle.interval_start_cycle);
      std::cout << "set " << this->get_set_index(addr) << " cleared WRITE " << addr << std::endl;
      pref.call_poll();
    }
    else 
      champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  }
  else {
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));

    if (pref.debug_print) {
      std::cout << "set " << this->get_set_index(addr) << " addr " << addr << " cleared LRU bits in cache fill" << std::endl; 
    }
    pref.call_poll();
  }

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  auto &pref = ::SPP_L3[{this, cpu}];
  pref.issue(this);
}

void CACHE::prefetcher_final_stats() {
  std::cout << "Oracle STATISTICS" << std::endl;
  std::cout << std::endl;
  std::cout << "Oracle prefetch accuracy: " << ::SPP_L3[{this, cpu}].issued_cs_pf_hit << "/" << ::SPP_L3[{this, cpu}].total_issued_cs_pf << "." << std::endl;

  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.finish();
}


