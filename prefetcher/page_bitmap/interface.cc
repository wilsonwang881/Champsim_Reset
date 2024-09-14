// WL 
#include "cache.h"
#include "page_bitmap.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, page_bitmap::prefetcher> PAGE_BITMAP;
}

void CACHE::prefetcher_initialize()
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];
  pref.init();

  std::cout << NAME << "-> Prefetcher Page Bitmap initialized @ cycle " << current_cycle << "." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];

  if (cache_hit) {
    pref.update(addr);
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];

  if (champsim::operable::context_switch_mode
      && !champsim::operable::have_cleared_BTB
      && !champsim::operable::have_cleared_BP
      && !champsim::operable::have_cleared_prefetcher
      && champsim::operable::cpu_side_reset_ready) {
    
    std::cout << NAME;
    pref.gather_pf();
    pref.clear_pg_access_status();
    pref.update_bitmap_store();
    champsim::operable::context_switch_mode = false;
    reset_misc::can_record_after_access = true;
  }
  else 
  {
    if (!pref.cs_pf.empty()) {
      bool prefetched = prefetch_line(pref.cs_pf.front(), 1, 0);

      if (prefetched) 
      {
        pref.cs_pf.pop_front(); 
      }
    }
  }
}

void CACHE::prefetcher_final_stats() {}

