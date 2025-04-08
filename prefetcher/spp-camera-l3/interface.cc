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

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && type != 2 && !cache_hit) {
    bool found_in_pending_queue = false;
    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr != this->MSHR.end()) {
      if (pref.debug_print) 
        std::cout << "Hit in MSHR set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;

      found_in_pending_queue = true; 
      pref.oracle.MSHR_hits++;
    } 
    else {
      auto search_pq = std::find_if(std::begin(this->internal_PQ), std::end(this->internal_PQ),
                                   [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                     return (entry.address >> shamt) == match; 
                                   });

      if (search_pq != this->internal_PQ.end()) {
        if (pref.debug_print) 
          std::cout << "Hit in inernal_PQ set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;

        found_in_pending_queue = true;
        this->internal_PQ.erase(search_pq); 
        pref.oracle.internal_PQ_hits++;
      }
      else {
        auto search_pending_pf = std::find_if(std::begin(pref.context_switch_issue_queue), std::end(pref.context_switch_issue_queue),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (std::get<0>(entry) >> shamt) == match; 
                                 });

        if (search_pending_pf != pref.context_switch_issue_queue.end()) {
          if (pref.debug_print) 
            std::cout << "Hit in pending issue queue set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;

          pref.context_switch_issue_queue.erase(search_pending_pf); 
          found_in_pending_queue = true;
          pref.oracle.cs_q_hits++;
        }
        else {
          auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf), std::end(pref.oracle.oracle_pf),
                                  [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.addr >> shamt) == match; 
                                  });

          if (search_oracle_pq != pref.oracle.oracle_pf.end()) {
            uint64_t set = search_oracle_pq->set;
            uint64_t way = pref.oracle.check_set_pf_avail(search_oracle_pq->addr);

            if (pref.debug_print) {
              std::cout << "Hit in oracle_pf set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;
              std::cout << "Found addr " << search_oracle_pq->addr << " set " << search_oracle_pq->set << " counter " << search_oracle_pq->miss_or_hit << " set_availability " << pref.oracle.set_availability[search_oracle_pq->set] << " found way " << way << std::endl;
            }

            if (way < NUM_WAY) {
              if (pref.oracle.cache_state[set * NUM_WAY + way].addr != search_oracle_pq->addr) 
                pref.oracle.set_availability[set]--;

              pref.oracle.cache_state[set * NUM_WAY + way].pending_accesses += (int)(search_oracle_pq->miss_or_hit);
              pref.oracle.cache_state[set * NUM_WAY + way].addr = search_oracle_pq->addr;
              pref.oracle.cache_state[set * NUM_WAY + way].require_eviction = search_oracle_pq->require_eviction;
              pref.oracle.cache_state[set * NUM_WAY + way].timestamp = search_oracle_pq->cycle_demanded;
              pref.oracle.cache_state[set * NUM_WAY + way].type = search_oracle_pq->type;
              assert(pref.oracle.set_availability[set] >= 0);
              pref.oracle.oracle_pf.erase(search_oracle_pq); 
              found_in_pending_queue = true;
              pref.oracle.oracle_pf_hits++;
            }
            else {
              // Rollback prefetches.
              // Replace entry in cache_state.
              // Move prefetch in the ready queue to oracle_pf.
              // The missed prefetch target now becomes demand miss.

              // Find the address from cache_state with latest timestamp.
              uint64_t rollback_cache_state_entry_index = pref.oracle.rollback_prefetch(base_addr); 

              // Generate a rollback prefetch.
              spp_l3::SPP_ORACLE::acc_timestamp rollback_pf;
              rollback_pf.cycle_demanded = pref.oracle.cache_state[rollback_cache_state_entry_index].timestamp;
              rollback_pf.set = set;
              rollback_pf.addr = pref.oracle.cache_state[rollback_cache_state_entry_index].addr;
              rollback_pf.miss_or_hit = pref.oracle.cache_state[rollback_cache_state_entry_index].pending_accesses;
              rollback_pf.require_eviction = pref.oracle.cache_state[rollback_cache_state_entry_index].require_eviction;
              rollback_pf.type = pref.oracle.cache_state[rollback_cache_state_entry_index].type;

              // Update cache_state.
              pref.oracle.cache_state[rollback_cache_state_entry_index].addr = base_addr;
              pref.oracle.cache_state[rollback_cache_state_entry_index].pending_accesses = search_oracle_pq->miss_or_hit;
              pref.oracle.cache_state[rollback_cache_state_entry_index].timestamp = search_oracle_pq->cycle_demanded;
              pref.oracle.cache_state[rollback_cache_state_entry_index].require_eviction = search_oracle_pq->require_eviction;
              pref.oracle.cache_state[rollback_cache_state_entry_index].type = search_oracle_pq->type;
              
              // Erase the moved ahead prefetch in oracle_pf
              pref.oracle.oracle_pf.erase(search_oracle_pq); 

              // Put back the rollback prefetch.
              // Put the rollback prefetch back to oracle_pf.
              /*
              auto oracle_pf_back_pos = std::find_if_not(std::begin(pref.oracle.oracle_pf), std::end(pref.oracle.oracle_pf),
                                 [demanded = rollback_pf.cycle_demanded](const auto& entry) {
                                   return demanded > entry.cycle_demanded; 
                                 });
                                 */
              auto oracle_pf_back_pos = std::find_if(std::begin(pref.oracle.oracle_pf), std::end(pref.oracle.oracle_pf),
                                 [match = rollback_pf.addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.addr >> shamt) == match;});
              pref.oracle.oracle_pf.emplace(oracle_pf_back_pos, rollback_pf);

              // Update metric
              pref.oracle.unhandled_misses_replaced++;

              // Allow updates to cache_state.
              found_in_pending_queue = true;

              if (pref.debug_print) 
                std::cout << "Unhandled miss set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << " replaced with accesses = " << pref.oracle.cache_state[rollback_cache_state_entry_index].pending_accesses << std::endl;

              auto rollback_pf_pos = std::find_if(std::begin(pref.context_switch_issue_queue), std::end(pref.context_switch_issue_queue),
                                 [match = pref.oracle.cache_state[rollback_cache_state_entry_index].addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (std::get<0>(entry) >> shamt) == match; 
                                 });

              // Try to find and erase the rollback prefetch in the ready to issue prefetch queue.
              if (rollback_pf_pos != pref.context_switch_issue_queue.end()) 
                pref.context_switch_issue_queue.erase(rollback_pf_pos);
            }
          }
          else {
            pref.oracle.unhandled_misses_not_found++;
            this->do_not_fill_address.push_back(base_addr);

            if (pref.debug_print) 
               std::cout << "Unhandled miss set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << " not found in oracle_pf" << std::endl;
          }
        }
      }
    }

    if (found_in_pending_queue) {
      useful_prefetch = true; 
      cache_hit = true;
      pref.oracle.runahead_hits++;
      found_in_MSHR = true;

      if (pref.debug_print) 
        std::cout << "Hit in pending queues ? " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;
    }
  }

  if (useful_prefetch && type != 2) 
    pref.oracle.update_demand(this->current_cycle, base_addr, 0, 0, type);
  else if (type != 2) 
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

  uint64_t set = this->get_set_index(base_addr);
  uint64_t way = this->get_way(base_addr, set);

  if (pref.oracle.ORACLE_ACTIVE && type != 2 && type != 3 && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if ((remaining_acc == 0) && evict) {  
      auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                   [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                     return (entry.address >> shamt) == match; 
                                   });
      pref.call_poll();
      int updated_remaining_acc = pref.oracle.check_pf_status(base_addr);

      if (updated_remaining_acc == -1) {
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
      }
      else 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    }
    else if (remaining_acc > 0) {
      if (way < NUM_WAY) 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    }
  }
  else if (pref.oracle.ORACLE_ACTIVE && type != 2 && type == 3 && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int current_acc = pref.oracle.check_pf_status(base_addr);

    if (pref.debug_print) {
      std::cout << "set " << set << " addr " << base_addr << " current_acc " << current_acc << std::endl; 
    }

    // Last access to the prefetched block used.
    if (current_acc == 1 && evict) {  
      if (found_in_MSHR && !original_hit) {
        pref.pending_write_fills.insert(base_addr); 

        if (pref.debug_print) 
          std::cout << "set " << set << " addr " << base_addr << " pushed to wait fill set" << std::endl; 
      }
      else if (original_hit) {
        pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);
        pref.call_poll();
        int updated_remaining_acc = pref.oracle.check_pf_status(base_addr);

        if (updated_remaining_acc == -1) {
          if (way < NUM_WAY && original_hit) {
            if (pref.debug_print) 
              std::cout << "set " << set << " addr " << base_addr << " cleared LRU bits" << std::endl; 

           champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0)); 
          } 
        }
      }
    }
    else if (current_acc > 1) {
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
      pref.pending_write_fills.erase(search);
      pref.oracle.update_pf_avail(addr, current_cycle - pref.oracle.interval_start_cycle);
      pref.call_poll();
      int updated_remaining_acc = pref.oracle.check_pf_status(addr);

      if (pref.debug_print) 
        std::cout << "addr " << addr << " set " << this->get_set_index(addr) << " cleared WRITE " << addr << std::endl;

      if (updated_remaining_acc == -1) 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
    }
    else 
      champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  }
  else {
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
    pref.call_poll();

    if (pref.debug_print) 
      std::cout << "set " << this->get_set_index(addr) << " addr " << addr << " cleared LRU bits in cache fill" << std::endl; 
  }

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {
  auto &pref = ::SPP_L3[{this, cpu}];
  uint64_t res = pref.issue(this);

  if (res != 0) {
    uint64_t set = this->get_set_index(res);
    uint64_t way = this->get_way(res, set);
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  }
}

void CACHE::prefetcher_final_stats() {
  auto &pref = ::SPP_L3[{this, cpu}];
  std::cout << "Oracle STATISTICS" << std::endl;
  std::cout << "Oracle prefetch accuracy: " << pref.issued_cs_pf_hit << "/" << pref.total_issued_cs_pf << "." << std::endl;

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.finish();
}


