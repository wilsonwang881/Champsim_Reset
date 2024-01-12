#include "cache.h"
#include "spp.h"

#include <map>

#define L1D_PREFETCHER_IN_USE 1

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

void CACHE::prefetcher_initialize()
{
  std::cout << std::endl;
  std::cout << "Signature Path Prefetcher SPP-Camera" << std::endl;
  std::cout << "Signature table" << " sets: " << spp::SIGNATURE_TABLE::SET << " ways: " << spp::SIGNATURE_TABLE::WAY << std::endl;
  std::cout << "Pattern table" << " sets: " << spp::PATTERN_TABLE::SET << " ways: " << spp::PATTERN_TABLE::WAY << std::endl;
  std::cout << "Prefetch filter" << " sets: " << spp::SPP_PREFETCH_FILTER::SET << " ways: " << spp::SPP_PREFETCH_FILTER::WAY << std::endl;
  std::cout << std::endl;

  // WL 
  auto &pref = ::SPP[{this, cpu}];
  pref.prefetcher_state_file.open("prefetcher_states.txt", std::ios::out);
  // WL 
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];

  pref.update_demand(base_addr,get_set_index(base_addr));
  pref.initiate_lookahead(base_addr);

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::SPP[{this, cpu}];
  pref.warmup = warmup; 
  // pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?

  // Gather and issue prefetches after a context switch.
  if (!L1D_PREFETCHER_IN_USE) {
    if (champsim::operable::context_switch_mode)
    {
      // Gather prefetches via the signature and pattern tables.
      if (!pref.context_switch_prefetch_gathered)
      {
        pref.context_switch_gather_prefetches();
        pref.context_switch_prefetch_gathered = true;
      }
     
      // Issue prefetches until the queue is empty.
      if (!pref.context_switch_queue_empty())
      {
        pref.context_switch_issue(this);
      }
      // Toggle switches after all prefetches are issued.
      else
      {
        champsim::operable::context_switch_mode = false;
        pref.context_switch_prefetch_gathered = false;
        std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
      }
    }
    // Normal operation.
    // No prefetch gathering via the signature and pattern tables.
    else
    {
      pref.issue(this);
      pref.step_lookahead();
    }
  }
  else {
    if (!champsim::operable::context_switch_mode) {
      pref.issue(this);
      pref.step_lookahead();
    }
  }
}

void CACHE::prefetcher_final_stats()
{
  std::cout << "SPP STATISTICS" << std::endl;
  std::cout << std::endl;

  ::SPP[{this, cpu}].print_stats(std::cout);
}

// WL
void CACHE::reset_spp_camera_prefetcher()
{
  std::cout << "=> Prefetcher cleared at CACHE " << NAME << " at cycle " << current_cycle << std::endl;
  auto &pref = ::SPP[{this, cpu}];
  pref.clear_states();
}

// WL 
void CACHE::record_spp_camera_states()
{
  std::cout << "Recording SPP states at CACHE " << NAME << std::endl;
  
  auto &pref = ::SPP[{this, cpu}];
  pref.cache_cycle = current_cycle;
  pref.record_spp_states();
}
