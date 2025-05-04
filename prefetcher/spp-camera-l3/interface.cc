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
    pref.call_poll(this);
    std::cout << "Oracle: try to prefetch " 
      << pref.context_switch_issue_queue.size() 
      << "/" << (NUM_WAY * NUM_SET) 
      << " blocks at the beginning." << std::endl;
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];
  base_addr = (base_addr>> 6) << 6;

  if (type == 2) 
    return metadata_in; 

  // Return if a demand misses and cannot merge in MSHR and MSHR is full.
  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !cache_hit && type != 3) {
    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                       [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS]
                       (const auto& entry) {
                         return (entry.address >> shamt) == match; 
                       });

    if (search_mshr == this->MSHR.end() 
        && this->get_mshr_occupancy() == this->get_mshr_size()) 
      return metadata_in; 
  }

  uint64_t set = this->get_set_index(base_addr);
  bool original_hit = cache_hit;

  if (pref.debug_print) 
    std::cout << "Hit/miss " << (unsigned)cache_hit << " set " << set << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << " MSHR usage " << this->get_mshr_occupancy() << std::endl;

  bool found_in_MSHR = false;
  bool found_in_inflight_writes = false;
  bool found_in_ready_queue = false;
  bool found_in_not_ready_queue = false;
  bool not_found_hit = false;

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !cache_hit) {
    bool found_in_pending_queue = false;
    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                       [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS]
                       (const auto& entry) {
                         return (entry.address >> shamt) == match; 
                       });

    auto search_inflight_writes = std::find_if(std::begin(this->inflight_writes), std::end(this->inflight_writes),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS]
                                 (const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr != this->MSHR.end() || search_inflight_writes != this->inflight_writes.end()) {
      if (search_mshr != this->MSHR.end()) {
        if (pref.debug_print) 
         std::cout << "Hit in MSHR set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;

        found_in_pending_queue = true; 
        found_in_MSHR = true;
        pref.oracle.MSHR_hits++;
      }

      if (search_inflight_writes != this->inflight_writes.end()) {
        if (pref.debug_print) 
        std::cout << "Hit in inflight_writes set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << std::endl;

        found_in_pending_queue = true; 
        found_in_inflight_writes = true;
        pref.oracle.inflight_write_hits++;
      }
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
          found_in_ready_queue = true;
          pref.oracle.cs_q_hits++;
        }
        else {
          auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf[set]), std::end(pref.oracle.oracle_pf[set]),
                                  [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.addr >> shamt) == match; 
                                  });

          if (search_oracle_pq != pref.oracle.oracle_pf[set].end()) {
            uint64_t way = pref.oracle.check_set_pf_avail(search_oracle_pq->addr);
            found_in_not_ready_queue = true;
            found_in_pending_queue = true;

            if (pref.debug_print) {
              std::cout << "Hit in oracle_pf set " << set << " addr " << base_addr << " type " << (unsigned)type << std::endl;
              std::cout << "Found addr " << search_oracle_pq->addr << " set " << search_oracle_pq->set << " counter " << search_oracle_pq->miss_or_hit << " set_availability " << pref.oracle.set_availability[search_oracle_pq->set] << " found way " << way << std::endl;
            }

            if (search_oracle_pq->miss_or_hit == 1) {
              // Do not fill the missed address. 
              pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                            base_addr, 
                                            false,
                                            this,
                                            type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
              pref.oracle.oracle_pf[set].erase(search_oracle_pq);
              pref.oracle.oracle_pf_size--;
              pref.oracle.unhandled_misses_not_replaced++;
            }
            else if(way < NUM_WAY) {
              assert(pref.oracle.cache_state[set * NUM_WAY + way].addr == 0 
                  || pref.oracle.cache_state[set * NUM_WAY + way].addr == search_oracle_pq->addr);

              if (pref.oracle.cache_state[set * NUM_WAY + way].addr != search_oracle_pq->addr) 
                pref.oracle.set_availability[set]--;

              pref.oracle.cache_state[set * NUM_WAY + way].pending_accesses += (int)(search_oracle_pq->miss_or_hit);
              pref.oracle.cache_state[set * NUM_WAY + way].addr = search_oracle_pq->addr;
              pref.oracle.cache_state[set * NUM_WAY + way].require_eviction = search_oracle_pq->require_eviction;
              pref.oracle.cache_state[set * NUM_WAY + way].timestamp = search_oracle_pq->cycle_demanded;
              pref.oracle.cache_state[set * NUM_WAY + way].type = search_oracle_pq->type;
              assert(pref.oracle.set_availability[set] >= 0);
              pref.oracle.oracle_pf[set].erase(search_oracle_pq); 
              pref.oracle.oracle_pf_size--;
              pref.oracle.oracle_pf_hits++;
            }
            else {
              // Rollback prefetch.
              // Find the counter with the missed address.
              // If the counter is 1, do not replace.
              // If the counter > 1, replace.
              if (pref.oracle.ROLLBACK_ENABLED){
                // Find the target to replace.
                uint64_t rollback_cache_state_index = pref.oracle.rollback_prefetch(base_addr); 

                // Generate a rollback prefetch.
                spp_l3::SPP_ORACLE::acc_timestamp rollback_pf;
                rollback_pf.cycle_demanded = pref.oracle.cache_state[rollback_cache_state_index].timestamp;
                rollback_pf.set = set;
                rollback_pf.addr = pref.oracle.cache_state[rollback_cache_state_index].addr;
                rollback_pf.miss_or_hit = pref.oracle.cache_state[rollback_cache_state_index].pending_accesses;
                rollback_pf.require_eviction = pref.oracle.cache_state[rollback_cache_state_index].require_eviction;
                rollback_pf.type = pref.oracle.cache_state[rollback_cache_state_index].type;

                // Update cache_state.
                pref.oracle.cache_state[rollback_cache_state_index].addr = base_addr;
                pref.oracle.cache_state[rollback_cache_state_index].pending_accesses = search_oracle_pq->miss_or_hit;
                pref.oracle.cache_state[rollback_cache_state_index].timestamp = search_oracle_pq->cycle_demanded;
                pref.oracle.cache_state[rollback_cache_state_index].require_eviction = search_oracle_pq->require_eviction;
                pref.oracle.cache_state[rollback_cache_state_index].type = search_oracle_pq->type;
                pref.oracle.cache_state[rollback_cache_state_index].accessed = false;

                // Erase the moved ahead prefetch in not ready queue. 
                pref.oracle.oracle_pf[set].erase(search_oracle_pq); 

                // Put back the rollback prefetch to not ready queue.
                pref.oracle.oracle_pf[set].push_back(rollback_pf);

                // Erase the rollback prefetch in the ready queue if it is in that queue.
                pref.erase_duplicate_entry_in_ready_queue(this, rollback_pf.addr);

                if (pref.debug_print) 
                  std::cout << "Replaced prefetch in set " << this->get_set_index(rollback_pf.addr) << " addr " << rollback_pf.addr << " with counter " << rollback_pf.miss_or_hit << std::endl;

                // If the rollback prefetch is in MSHR, push to do not fill.
                auto search_mshr_rollback = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                            [match = rollback_pf.addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                              return (entry.address >> shamt) == match; 
                                            });

                if (search_mshr_rollback != this->MSHR.end()) 
                  pref.update_do_not_fill_queue(this->do_not_fill_address, 
                                                rollback_pf.addr, 
                                                false, 
                                                this, 
                                                " do_not_fill_address rollbacl_pf in MSHR");

                // If the rollback prefetch is in inflight_writes, push to do not fill.
                auto search_writes_rollback = std::find_if(std::begin(this->inflight_writes), std::end(this->inflight_writes),
                                            [match = rollback_pf.addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                              return (entry.address >> shamt) == match; 
                                            });

                if (search_writes_rollback != this->inflight_writes.end()) 
                  pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                                rollback_pf.addr, 
                                                false, 
                                                this, 
                                                "do_not_fill_write_address rollbacl_pf in inflight_writes");

                // If the rollback prefetch is already in cache, set LRU to 0.
                uint64_t rollback_set = this->get_set_index(rollback_pf.addr);
                uint64_t rollback_way = this->get_way(rollback_pf.addr, rollback_set);

                if (rollback_way < this->NUM_WAY) 
                  champsim::operable::lru_states.push_back(std::make_tuple(rollback_set, rollback_way, 0));

                // Update metric
                pref.oracle.unhandled_misses_replaced++;

                if (pref.debug_print) 
                  std::cout << "Replaced miss set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << " replaced with accesses = " << pref.oracle.cache_state[rollback_cache_state_index].pending_accesses << " replaced addr " << rollback_pf.addr << std::endl;
              }
              else {
                assert(!pref.oracle.ROLLBACK_ENABLED);
                search_oracle_pq->miss_or_hit--;

                pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                              base_addr, 
                                              false,
                                              this,
                                              type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
              }
            }
          }
          else {
            if (pref.context_switch_issue_queue.size() != 0 || pref.oracle.oracle_pf_size != 0) {
              pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                            base_addr, 
                                            false,
                                            this,
                                            type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");

              if (type != 3) 
                pref.oracle.unhandled_non_write_misses_not_filled++;
              else 
                pref.oracle.unhandled_write_misses_not_filled++;
            }

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

      if (pref.debug_print) 
        std::cout << "Hit in pending queues ? " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;
    }
  }
  else if (pref.oracle.check_pf_status(base_addr) < 0)  {
    auto search_oracle_pq = std::find_if(std::begin(pref.oracle.oracle_pf[set]), std::end(pref.oracle.oracle_pf[set]),
                            [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                              return (entry.addr >> shamt) == match; 
                            });

    if (search_oracle_pq != pref.oracle.oracle_pf[set].end()) {
      // Find the target to replace.
      uint64_t rollback_cache_state_index = pref.oracle.rollback_prefetch(base_addr); 

      // Generate a rollback prefetch.
      spp_l3::SPP_ORACLE::acc_timestamp rollback_pf;
      rollback_pf.cycle_demanded = pref.oracle.cache_state[rollback_cache_state_index].timestamp;
      rollback_pf.set = set;
      rollback_pf.addr = pref.oracle.cache_state[rollback_cache_state_index].addr;
      rollback_pf.miss_or_hit = pref.oracle.cache_state[rollback_cache_state_index].pending_accesses;
      rollback_pf.require_eviction = pref.oracle.cache_state[rollback_cache_state_index].require_eviction;
      rollback_pf.type = pref.oracle.cache_state[rollback_cache_state_index].type;

      // Update cache_state.
      pref.oracle.cache_state[rollback_cache_state_index].addr = base_addr;
      pref.oracle.cache_state[rollback_cache_state_index].pending_accesses = search_oracle_pq->miss_or_hit;
      pref.oracle.cache_state[rollback_cache_state_index].timestamp = search_oracle_pq->cycle_demanded;
      pref.oracle.cache_state[rollback_cache_state_index].require_eviction = search_oracle_pq->require_eviction;
      pref.oracle.cache_state[rollback_cache_state_index].type = search_oracle_pq->type;
      pref.oracle.cache_state[rollback_cache_state_index].accessed = false;

      // Erase the moved ahead prefetch in not ready queue. 
      pref.oracle.oracle_pf[set].erase(search_oracle_pq); 

      // Put back the rollback prefetch to not ready queue.
      pref.oracle.oracle_pf[set].push_front(rollback_pf);

      // Erase the rollback prefetch in the ready queue if it is in that queue.
      pref.erase_duplicate_entry_in_ready_queue(this, rollback_pf.addr);

      if (pref.debug_print) 
        std::cout << "Replaced prefetch in set " << this->get_set_index(rollback_pf.addr) << " addr " << rollback_pf.addr << " with counter " << rollback_pf.miss_or_hit << std::endl;

      // If the rollback prefetch is in MSHR, push to do not fill.
      auto search_mshr_rollback = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                  [match = rollback_pf.addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.address >> shamt) == match; 
                                  });

      if (search_mshr_rollback != this->MSHR.end()) {
        pref.update_do_not_fill_queue(this->do_not_fill_address,
                                      rollback_pf.addr, 
                                      false,
                                      this,
                                      "do_not_fill_address");

        if (pref.debug_print) 
          std::cout << "Rollback prefetch in set " << this->get_set_index(rollback_pf.addr) << " addr " << rollback_pf.addr << " pushed to do not fill in mshr" << std::endl;
      }

      // If the rollback prefetch is in inflight_writes, push to do not fill.
      auto search_writes_rollback = std::find_if(std::begin(this->inflight_writes), std::end(this->inflight_writes),
                                  [match = rollback_pf.addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.address >> shamt) == match; 
                                  });

      if (search_writes_rollback != this->inflight_writes.end()) 
        pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                      rollback_pf.addr, 
                                      false,
                                      this,
                                      "do_not_fill_write_address");

      // If the rollback prefetch is already in cache, set LRU to 0.
      uint64_t rollback_set = this->get_set_index(rollback_pf.addr);
      uint64_t rollback_way = this->get_way(rollback_pf.addr, rollback_set);

      if (rollback_way < this->NUM_WAY) 
        champsim::operable::lru_states.push_back(std::make_tuple(rollback_set, rollback_way, 0));

      // Update metric
      pref.oracle.unhandled_misses_replaced++;

      if (pref.debug_print) 
        std::cout << "Replaced miss set " << this->get_set_index(base_addr) << " addr " << base_addr << " type " << (unsigned)type << " replaced with accesses = " << pref.oracle.cache_state[rollback_cache_state_index].pending_accesses << " replaced addr " << rollback_pf.addr << std::endl;

      assert(pref.oracle.check_pf_status(base_addr) > 0);
    }
    else {
      uint64_t lru_zero_set = this->get_set_index(base_addr);
      uint64_t lru_zero_way = this->get_way(base_addr, lru_zero_set);
      champsim::operable::lru_states.push_back(std::make_tuple(lru_zero_set, lru_zero_way, 0));
      not_found_hit = true;
    }
  }

  if (useful_prefetch) 
    pref.oracle.update_demand(this->current_cycle, base_addr, 0, 0, type);
  else
    pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit, 1, type);

  uint64_t way = this->get_way(base_addr, set);
  bool evict = pref.oracle.check_require_eviction(base_addr);

  if (not_found_hit) 
    cache_hit = false; 

  if (pref.oracle.ORACLE_ACTIVE && cache_hit && !pref.oracle.RECORD_OR_REPLAY) {
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if (remaining_acc == 0) {   
      pref.call_poll(this);
      int updated_remaining_acc = pref.oracle.check_pf_status(base_addr);

      if (updated_remaining_acc == -1) {
        if (found_in_MSHR || found_in_ready_queue || found_in_not_ready_queue || found_in_inflight_writes) {
          
          if (std::find(this->do_not_fill_address.begin(), this->do_not_fill_address.end(), base_addr) == this->do_not_fill_address.end() && type != 3) {
            pref.update_do_not_fill_queue(this->do_not_fill_address,
                                          base_addr, 
                                          false,
                                          this,
                                          "do_not_fill_address");

            if (found_in_inflight_writes)  {
              pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                            base_addr, 
                                            false,
                                            this,
                                            "do_not_fill_write_address");
            }
          }

          if (type == 3) 
            pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                          base_addr, 
                                          false,
                                          this,
                                          "do_not_fill_write_address");
        } 

        if (way < NUM_WAY) {
          if (pref.debug_print) 
            std::cout << "set " << set << " addr " << base_addr << " cleared LRU bits" << std::endl; 

          pref.erase_duplicate_entry_in_ready_queue(this, base_addr);

          //if (evict) 
            champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
        }
      }
      else 
        pref.update_do_not_fill_queue(type == 3 ? this->do_not_fill_write_address : this->do_not_fill_address,
                                      base_addr, 
                                      true,
                                      this,
                                      type == 3 ? "do_not_fill_write_address" : "do_not_fill_address");
      /*
      else if (way < NUM_WAY) 
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
        */
    }
    else if (remaining_acc > 0 && way < NUM_WAY) 
      champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    /*
    else {
      assert(false);
    }
    */
  }
  
  if ((pref.issued_cs_pf.find(base_addr) != pref.issued_cs_pf.end()) && useful_prefetch) {
    pref.issued_cs_pf_hit++; 
    pref.issued_cs_pf.erase(base_addr);
  }

  // Champsim does not check MSHRs for write misses.
  if (type == 3 && !original_hit && found_in_MSHR) {
    auto search = std::find(this->do_not_fill_address.begin(), this->do_not_fill_address.end(), base_addr);

    if (search == this->do_not_fill_address.end()) {
      this->do_not_fill_address.push_back(base_addr);

      if (pref.debug_print) 
        std::cout << "addr " << base_addr << " set " << this->get_set_index(base_addr) << " pushed to do_not_fill_address" << std::endl; 
    }
    else {
      pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                    base_addr, 
                                    false,
                                    this,
                                    "do_not_fill_write_address");
    }
  }

  if (type != 3 && !original_hit && found_in_inflight_writes) {
    auto search = std::find(this->do_not_fill_address.begin(), this->do_not_fill_address.end(), base_addr);

    if (search == this->do_not_fill_address.end()) {
      this->do_not_fill_address.push_back(base_addr);

      if (pref.debug_print) 
        std::cout << "type != 3 addr " << base_addr << " set " << this->get_set_index(base_addr) << " pushed to do_not_fill_address" << std::endl; 
    }
    else {
      pref.update_do_not_fill_queue(this->do_not_fill_write_address,
                                    base_addr, 
                                    false,
                                    this,
                                    "do_not_fill_write_address");
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
  auto &pref = ::SPP_L3[{this, cpu}];
  addr = (addr >> 6) << 6;

  if (pref.debug_print) 
    std::cout << "Filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << " remaining access " << pref.oracle.check_pf_status(addr) << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && (pref.context_switch_issue_queue.size() != 0 || pref.oracle.oracle_pf.size() != 0)) {
    int filled_addr_counter = pref.oracle.check_pf_status(addr);
    int evicted_addr_counter = pref.oracle.check_pf_status(evicted_addr);

    if (filled_addr_counter < 0 && addr != evicted_addr && evicted_addr_counter > 0) {
       std::cout << "Error filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " fill addr counter " << filled_addr_counter << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << " evicted address remaining access " << evicted_addr_counter << std::endl;
       assert(filled_addr_counter > 0);
    }

    if (evicted_addr_counter > 0 && addr != evicted_addr) {
       std::cout << "Error filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " fill addr counter " << filled_addr_counter << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << " evicted address remaining access " << evicted_addr_counter << std::endl;
       assert(evicted_addr_counter < 0);
    }
  }

  if (pref.oracle.ORACLE_ACTIVE && pref.oracle.check_pf_status(addr) > 0) 
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
  else {
    pref.erase_duplicate_entry_in_ready_queue(this, addr);
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
    pref.call_poll(this);

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

    if (way < NUM_WAY) 
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

