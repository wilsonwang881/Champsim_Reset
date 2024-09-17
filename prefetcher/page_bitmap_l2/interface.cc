// WL 
#include "cache.h"
#include "page_bitmap.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, page_bitmap_l2::prefetcher> PAGE_BITMAP_L2;
}

void CACHE::prefetcher_initialize()
{
  auto &pref = ::PAGE_BITMAP_L2[{this, cpu}];
  pref.init();

  std::cout << NAME << "-> Prefetcher Page Bitmap initialized @ cycle " << current_cycle << "." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::PAGE_BITMAP_L2[{this, cpu}];

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
  auto &pref = ::PAGE_BITMAP_L2[{this, cpu}];

  if (reset_misc::can_record_after_access) {
    
    std::cout << NAME;
    pref.gather_pf();
    pref.clear_pg_access_status();
    pref.update_bitmap_store();
    reset_misc::can_record_after_access = false;
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

