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
    pref.hit_blks.insert(addr);
  }

  if (cache_hit) 
  {
    if (pref.check_p_tb(true, addr) ||
        pref.check_p_tb(false, addr)) 
      pref.perceptron_update(addr, true);
  }
  else
  {
     if (pref.check_p_tb(true, addr) ||
        pref.check_p_tb(false, addr)) 
      pref.perceptron_update(addr, false);

  }

  /*
  if (pref.check_p_tb(false, addr) || pref.check_p_tb))
  {
    pref.perceptron_update(addr, true);  
  }
  */

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  // metadata_in is used to indicate if the block is prefetched or not and
  // whether it is brought in during this context switch interval.
  uint32_t blk_asid_match = metadata_in >> 2; 
  uint32_t blk_pfed = (metadata_in >> 1 & 0x1); 
  uint32_t pkt_pfed = metadata_in & 0x1;

  auto &pref = ::PAGE_BITMAP[{this, cpu}];

  if (blk_asid_match) 
  {
    /*
    if (!blk_pfed) 
      pref.update(evicted_addr); 
      */

    if (!prefetch)
    {
      //assert(addr != 0);
      //assert(evicted_addr != 0);

      if (addr != 0)
        pref.update(addr);

      /*
      if (evicted_addr != 0)
        pref.update(evicted_addr);
        */
    }
  }
  /*
  else 
  {
    if (!blk_pfed) 
      pref.update(evicted_addr); 

    if (!pkt_pfed)
      pref.update(addr); 
  }
  */

  if (blk_asid_match)
  {
    if (pref.check_p_tb(false, evicted_addr))
    {
      pref.perceptron_update(evicted_addr, false); 
    }  
  }
  // If the block is brought in by prefetch,
  // and now is evicted,
  // move the address from prefetch table to reject table,
  // and update the perceptron layer.
  /*
  if (pf_this_interval && pf_replaced)
  {
    pref.invalidate_p_tb(false, evicted_addr);
    pref.update_p_tb(true, evicted_addr);
    pref.perceptron_update(evicted_addr, false);
  }
  */

  /*
  if (pf_this_interval && pref.check_p_tb(true, evicted_addr))
  {
    pref.perceptron_update(evicted_addr, false);  
  }
  */

  // Update the bitmap
  // If the block is not brought in by prefetch and now is evicted,
  // then the block is brought in by on demand access.
  /*
  if (pf_this_interval && !pf_replaced) {
    pref.update(evicted_addr); 
    //pref.invalidate_p_tb(true, evicted_addr);
  }

  if (pf_this_interval && pf_replaced) {
    pref.invalidate_bitmap(evicted_addr); 
  }
  */

  /*
  if (pf_this_interval && !prefetch)
  {
    pref.update(addr);
  }
  */

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
    pref.pf_useful += pref.check_set_overlap();
    pref.threshold_cycle = current_cycle + 3600000;
  }
  else 
  {
    uint64_t addr = pref.cs_pf.front();
    uint64_t priority = pref.cs_weight.front();

    if (!pref.cs_pf.empty() && current_cycle < pref.threshold_cycle) 
    {
      int weight = pref.perceptron_check(addr);

      // Check the rejection table.
      if (true) //weight >= 10)
      {
        // If not rejected, prefetch.
        bool prefetched = prefetch_line(addr, priority, 0);

        if (prefetched) 
        {
          pref.update_p_tb(false, addr);
          pref.cs_pf.pop_front();
          pref.cs_weight.pop_front();
          pref.pf_blks.insert(addr);
          pref.pf_count++;
        }  
      }
      /*
      else{
         // If not rejected, prefetch.
        bool prefetched = prefetch_line(pref.cs_pf.front(), 1, 0);

        if (prefetched) 
        {
          pref.update_p_tb(false, addr);
          pref.cs_pf.pop_front();
          pref.pf_blks.insert(addr);
          pref.pf_count++;
        }  
      }
      */
      /*
      else 
      {
        // If rejected, pop the address without prefetch.
        pref.update_p_tb(true, addr);
        pref.cs_pf.pop_front();
      }
      */
    }
  }
}

void CACHE::prefetcher_final_stats() 
{
  auto &pref = ::PAGE_BITMAP[{this, cpu}];
  std::cout << "Page bitmap correctly prefetched " << pref.pf_useful<< " blocks." << std::endl;
  std::cout << "Page bitmap prefetched " << pref.pf_count << " blocks." << std::endl; 
}

