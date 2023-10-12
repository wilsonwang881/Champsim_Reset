#include "cache.h"
#include "spp.h"

#include <map>

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
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];

  pref.update_demand(base_addr, this->get_set(base_addr));
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
  // pref.warmup = warmup; 
  pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?

  // Gather and issue prefetches after a context switch.
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
      cout << NAME << " stalled " << context_switch_cycles_stalled << " cycles" << " done at cycle " << current_cycle << endl;
      context_switch_cycles_stalled = 0;
      //this->reset_spp_camera_prefetcher();
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

void CACHE::prefetcher_final_stats()
{
  std::cout << "SPP STATISTICS" << std::endl;
  std::cout << std::endl;

  ::SPP[{this, cpu}].print_stats(std::cout);
}

void CACHE::reset_spp_camera_prefetcher()
{
  cout << "Reset spp camera prefetcher at CACHE " << NAME << endl;
  auto &pref = ::SPP[{this, cpu}];
  pref.clear_states();
}
