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
  std::cout << "Signature Path Prefetcher SPP_L3-Camera" << std::endl;
  std::cout << std::endl;

  // WL 
  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.init();

  // Testing the no context switch case.
  // WL 
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];

  // Return if a demand misses and cannot merge in MSHR and MSHR is full.
  /*
  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit) && !cache_hit) {

    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr == this->MSHR.end() && this->get_mshr_occupancy() == this->get_mshr_size()) {
      return metadata_in; 
    }
  }
  */

  pref.oracle.access_counter++;

  //std::cout << "Hit/miss " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit)) {

    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr != this->MSHR.end() && champsim::to_underlying(search_mshr->type) == 2) {
      //useful_prefetch = true; 
      //cache_hit = true;
      //std::cout << "Hit in MSHR " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;
    }

    if (useful_prefetch) {
      uint64_t res = pref.oracle.update_demand(this->current_cycle, base_addr, 0, 0);

      if (res != 0) {
        //std::cout << "Pushed addr " << base_addr << " to do not fill queue" << std::endl;
        this->do_not_fill_address.push_back(res);
      } 
    }
    else {
      uint64_t res = pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit, 1);

      if (res != 0) {
        //std::cout << "Pushed addr " << base_addr << " to do not fill queue" << std::endl;
        this->do_not_fill_address.push_back(res);
      }
    }      
  }

  pref.oracle.hit_address = (base_addr >> 6) << 6;

  if (pref.oracle.ORACLE_ACTIVE && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    int before_acc = pref.oracle.check_pf_status(base_addr);
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if ((before_acc > remaining_acc) && (remaining_acc == 0) && evict) {  

      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way((base_addr >> 6) << 6, set);

      if (way < NUM_WAY) {
        champsim::operable::lru_states_llc.push_back(std::make_tuple(set, way, 0));
      }
    }
    else if (remaining_acc > 0) {
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way((base_addr >> 6) << 6, set);

      if (way < NUM_WAY) {
        champsim::operable::lru_states_llc.push_back(std::make_tuple(set, way, 1));
        //std::cout << "Updated LRU for addr " << base_addr << " at set " << set << std::endl;
        //pref.oracle.update_persistent_lru_addr(base_addr, false); 
      } 
    }
  }

  if ((pref.issued_cs_pf.find((base_addr >> 6) << 6) != pref.issued_cs_pf.end()) && useful_prefetch) {
    pref.issued_cs_pf_hit++; 
    pref.issued_cs_pf.erase((base_addr >> 6) << 6);
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];

  //std::cout << "Filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " evicted_addr " << evicted_addr << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && prefetch) {
    champsim::operable::lru_states_llc.push_back(std::make_tuple(set, way, 1));
    //pref.oracle.update_persistent_lru_addr(addr, false);
  } 

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.done) {
    champsim::operable::kill_simulation_l3 = true; 
  }

  if (champsim::operable::kill_simulation_l2 && champsim::operable::kill_simulation_l3) {
    exit(0); 
  }

  // Normal operation.
  // No prefetch gathering via the signature and pattern tables.
  if (pref.oracle.ORACLE_ACTIVE && ((pref.oracle.oracle_pf.size() > 0))) // && pref.oracle.available_pf > 0 && pref.oracle.hit_address != 0)
  {
    std::tuple<uint64_t, uint64_t, bool> potential_cs_pf = pref.oracle.poll(pref.oracle.hit_address);
  
    if (std::get<0>(potential_cs_pf) != 0) {
      pref.context_switch_issue_queue.push_back({std::get<0>(potential_cs_pf), std::get<2>(potential_cs_pf), std::get<1>(potential_cs_pf)});
    }
  }

  pref.issue(this);
}

void CACHE::prefetcher_final_stats() {
  std::cout << "SPP_L3 STATISTICS" << std::endl;
  std::cout << std::endl;

  // WL 
  std::cout << "Context switch prefetch accuracy: " << ::SPP_L3[{this, cpu}].issued_cs_pf_hit << "/" << ::SPP_L3[{this, cpu}].total_issued_cs_pf << "." << std::endl;

  auto &pref = ::SPP_L3[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.finish();
}


