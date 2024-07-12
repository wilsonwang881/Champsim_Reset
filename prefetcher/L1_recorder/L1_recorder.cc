#include "cache.h"
#include <unordered_set>
#include <map>
#include <cassert>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 64
#define NUMBER_OF_PREFETCH_UNIT 400

namespace {

  struct tracker {

    std::unordered_set<uint64_t> uniq_prefetch_address;
    
  };

  std::map<CACHE*, tracker> trackers;
}

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " -> Prefetcher L1 recorder initialized @ " << current_cycle << " cycles." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  reset_misc::on_demand_data_access acc;
  acc.cycle = current_cycle;
  acc.ip = addr;
  acc.load_or_store = (type == 0) ? true : false;

  if (reset_misc::dq_before_data_access.size() < DEQUE_ON_DEMAND_ACCESS_RECORD_SIZE) {
    reset_misc::dq_before_data_access.push_back(acc); 
  }
  else {
    reset_misc::dq_before_data_access.pop_front();
    reset_misc::dq_before_data_access.push_back(acc);
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  
}

void CACHE::prefetcher_final_stats() {}


