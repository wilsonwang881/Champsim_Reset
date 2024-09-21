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

  if (cache_hit) 
  {
    pref.update(addr);
  }

  if (pref.check_p_tb(true, addr) ||
      pref.check_p_tb(false, addr))
  {
    pref.invalidate_p_tb(true, addr);
    pref.tag_counter_update(addr, true);
  }
  /*
  if (cache_hit && useful_prefetch)
    pref.tag_counter_update(addr, useful_prefetch);
    */

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];

  // If the block is brought in by prefetch,
  // and now is evicted,
  // move the address from prefetch table to reject table,
  // and update the perceptron layer.
  if (prefetch && pref.check_p_tb(false, evicted_addr))
  {
    pref.invalidate_p_tb(false, evicted_addr);
    pref.update_p_tb(true, evicted_addr);
    pref.tag_counter_update(evicted_addr, false);
  }

  // Update the bitmap.
  // If the block is not brought in by prefetch and now is evicted,
  // then the block is brought in by on demand access.
  if (!prefetch) {
    pref.update_bitmap(addr); 
  }

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];

  if (champsim::operable::context_switch_mode
      && !champsim::operable::have_cleared_BTB
      && !champsim::operable::have_cleared_BP
      && !champsim::operable::have_cleared_prefetcher
      && champsim::operable::cpu_side_reset_ready) 
  {
    std::cout << NAME;
    pref.gather_pf();
    pref.clear_pg_access_status();
    pref.update_bitmap_store();
    champsim::operable::context_switch_mode = false;
    reset_misc::can_record_after_access = true;
  }
  else 
  {
    uint64_t addr = pref.cs_pf.front();

    if (!pref.cs_pf.empty()) 
    {
      // Check the rejection table.
      if (true) //!pref.check_p_tb(true, addr) &&
//          pref.tag_counter_check(addr))
      {
        // If not rejected, prefetch.
        bool prefetched = prefetch_line(pref.cs_pf.front(), 1, 0);

        if (prefetched) 
        {
          pref.update_p_tb(false, addr);
          pref.cs_pf.pop_front();
        }  
      }
      else 
      {
        // If rejected, pop the address without prefetch.
        pref.update_p_tb(true, addr);
        pref.cs_pf.pop_front();
        pref.rejected_count++;
      }
    }
  }
}

void CACHE::prefetcher_final_stats() 
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];
  std::cout << "Page bitmap rejected " << pref.rejected_count << " prefetches." << std::endl; 
}

