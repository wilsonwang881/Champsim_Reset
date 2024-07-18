#include "cache.h"
#include <unordered_set>
#include <map>
#include <cassert>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 64
#define NUMBER_OF_PREFETCH_UNIT 400
#define OBSERVATION_WINDOW 10

namespace {

  struct tracker {

    std::unordered_set<uint64_t> uniq_prefetch_address;
    
  };

  std::map<CACHE*, tracker> trackers;
}

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " -> Prefetcher L1D recorder initialized @ " << current_cycle << " cycles." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  reset_misc::on_demand_data_access acc;
  acc.cycle = current_cycle;
  acc.ip = ip;
  acc.occurance = 1;
  acc.load_or_store = (type == 0) ? true : false;
  reset_misc::addr_occr addr_obj;
  uint64_t block_addr = (addr >> LOG2_BLOCK_SIZE) << LOG2_BLOCK_SIZE;
  addr_obj.addr = block_addr;
  addr_obj.occr = 1;
  acc.addr_rec.push_back(addr_obj);

  if (reset_misc::can_record_after_access) {
    
    // Check if deque empty. 
    if (reset_misc::dq_pf_data_access.empty()) {
      reset_misc::dq_after_data_access.push_back(acc);
    }
    // Deque not empty.
    else {
      if (reset_misc::dq_pf_data_access.back().ip == ip) {
        reset_misc::dq_after_data_access.back().addr.insert(block_addr);
        reset_misc::dq_after_data_access.back().occurance++;
      } 
      else {
        reset_misc::dq_after_data_access.push_back(acc);
      }
    }

    // Dequeue full.
    // Analysis.
    if (reset_misc::dq_after_data_access.size() > DEQUE_ON_DEMAND_ACCESS_RECORD_SIZE) {
      reset_misc::dq_after_data_access.pop_front(); 
      reset_misc::can_record_after_access = false;

      std::cout << "Feedback:" << reset_misc::dq_after_data_access.size() << " " << reset_misc::dq_pf_data_access.size() << std::endl;
      for(auto pf: reset_misc::dq_pf_data_access) {

        bool printable = false;

        for(auto after: reset_misc::dq_after_data_access) {

          for(auto data : pf.addr_rec) {
            if (after.addr.find(data.addr) != after.addr.end()) {
              printable = true;
            } 
          }
          if (pf.ip == after.ip) {
            printable = true;
          }
        }
        if (printable) {
          std::cout << "Match: Occurance (pf, after) = " << pf.occurance << " " << " Data size (pf, after) = " << pf.addr_rec.size() << " " << std::endl;
        }
      }
    }
  }

  // Check if deque empty
  if (reset_misc::dq_before_data_access.size() == 0) {
     reset_misc::dq_before_data_access.push_back(acc); 
     return metadata_in;  
  }

  // Check the past n accesses
  size_t limit = (reset_misc::dq_before_data_access.size() > OBSERVATION_WINDOW) ? (reset_misc::dq_before_data_access.size() - OBSERVATION_WINDOW) : 0;
  for (size_t i = reset_misc::dq_before_data_access.size() - 1; i > limit ; i--) {
    if (reset_misc::dq_before_data_access[i].ip == ip) {

      reset_misc::dq_before_data_access[i].occurance++;
      bool found = false;

      for(reset_misc::addr_occr var : reset_misc::dq_before_data_access[i].addr_rec) {
        if (var.addr == block_addr) {
          var.occr++;
          found = true;
        }
      }

      if (!found) {
        reset_misc::addr_occr tmpp_addr;
        tmpp_addr.addr = block_addr;
        tmpp_addr.occr = 1;
        reset_misc::dq_before_data_access[i].addr_rec.push_back(tmpp_addr);
      }
      return metadata_in;
    }
  }

  // Check if addr same as the last one.
  if (reset_misc::dq_before_data_access.back().ip == addr) {
    reset_misc::dq_before_data_access.back().occurance++;
  }
  else {
    reset_misc::dq_before_data_access.push_back(acc); 
  }

  // Check if length exceeds the limit
  if (reset_misc::dq_before_data_access.size() > DEQUE_ON_DEMAND_ACCESS_RECORD_SIZE) {
    reset_misc::dq_before_data_access.pop_front();
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


