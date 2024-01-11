#include "cache.h"

// WL 

void CACHE::prefetcher_initialize()
{

  std::cout << "Prefetcher initialized @ " << current_cycle << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
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
      std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
      //context_switch_cycles_stalled = 0;
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

void CACHE::prefetcher_final_stats() {}

// WL 
