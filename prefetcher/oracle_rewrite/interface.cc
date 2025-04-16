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

    auto &pref = ::SPP_L3[{this,cpu}];

    base_addr = (base_addr >> 6) << 6;
    bool original_hit = cache_hit;

    if (base_addr != pref.last_handled_addr)
        pref.last_handled_addr = base_addr;
    else
        return metadata_in;

    auto found_in_MSHR = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                return (entry.address >> shamt) == match; 
                                });
    
    bool found_pending = false;
    if(cache_hit == 0) {
        if(found_in_MSHR != this->MSHR.end()) {
            found_pending = true;
            pref.oracle.MSHR_hits++;
        } else {
            auto found_in_pq = std::find_if(std::begin(this->internal_PQ), std::end(this->internal_PQ),
                                   [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                     return (entry.address >> shamt) == match; 
                                   });
            if(found_in_pq != this->internal_PQ.end()) {
                found_pending = true;
                pref.oracle.internal_PQ_hits++;
            } else {
                auto found_in_rq = std::find_if(std::begin(pref.ready_queue), std::end(pref.ready_queue),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (std::get<0>(entry) >> shamt) == match; 
                                 });
                if (found_in_rq != pref.ready_queue.end()) {
                    found_pending = true;
                    //erase from ready queue
                    pref.ready_queue.erase(found_in_rq);

                    pref.oracle.q_hits++;
                }
            }
        }


        //couldn't find pending, check not ready queues
        if(!found_pending) {
            auto set = this->get_set_index(base_addr);

            auto search_oracle_pq = std::find_if(std::begin(pref.oracle.not_ready_queue[set]), std::end(pref.oracle.not_ready_queue[set]),
                                  [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                    return (entry.addr >> shamt) == match; 
                                  });
            if(search_oracle_pq != pref.oracle.not_ready_queue[set].end()) {
                auto way = pref.oracle.check_set_pf_avail(search_oracle_pq->addr);

                if( way < NUM_WAY) {
                    if (pref.oracle.cache_state[set * NUM_WAY + way].addr != search_oracle_pq->addr) 
                        pref.oracle.set_availability[set]--;

                    pref.oracle.cache_state[set * NUM_WAY + way].pending_accesses += (int)(search_oracle_pq->miss_or_hit);
                    pref.oracle.cache_state[set * NUM_WAY + way].addr = search_oracle_pq->addr;
                    pref.oracle.cache_state[set * NUM_WAY + way].require_eviction = search_oracle_pq->require_eviction;
                    pref.oracle.cache_state[set * NUM_WAY + way].timestamp = search_oracle_pq->cycle_demanded;
                    pref.oracle.cache_state[set * NUM_WAY + way].type = search_oracle_pq->type;
                    assert(pref.oracle.set_availability[set] >= 0);
                    pref.oracle.oracle_pf.erase(search_oracle_pq); 
                    found_pending = true;
                    pref.oracle.oracle_pf_hits++;
                } else {
                    if (search_oracle_pq->miss_or_hit == 1) {
                        this->do_not_fill_address.push_back(search_oracle_pq->addr);

                        // Erase the moved ahead prefetch in oracle_pf
                        pref.oracle.oracle_pf.erase(search_oracle_pq);
                    }
                }
                

            } else {
                pref.oracle.unhandled_misses_not_found++;
                this->do_not_fill_address.push_back(base_addr);
            }
        }

        if (found_pending) {
            useful_prefetch = true;
            cache_hit = true;
            pref.oracle.runahead_hits++;
            found_in_MSHR = true;
        }
    }
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

