#include "cache.h"
#include "stlb_pf.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF;
}

void CACHE::prefetcher_initialize() 
{
  std::cout << "Initialized STLB Prefetcher in " << NAME << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::STLB_PF[{this, cpu}];

  if (cache_hit) // && (metadata_in == 1)) 
  {
    pref.update(addr);
    pref.update(ip);
    pref.hit_this_round = true;
    pref.hits++;
  }
  else
  {
    pref.hit_this_round = false;
  }

  pref.pop_pf(addr);
  pref.pop_pf(ip);

  if (useful_prefetch)
  {
    pref.pf_hit++;
    pref.last_issued_pf_moment -= pref.wait_interval;
  }

  pref.accesses++;

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  auto &pref = ::STLB_PF[{this, cpu}];

  uint32_t blk_asid_match = metadata_in >> 2; 
  /*
  uint32_t blk_pfed = (metadata_in >> 1 & 0x1); 
  uint32_t pkt_pfed = metadata_in & 0x1;

  if (!pkt_pfed && addr != 0)
    pref.update(addr);

  if (blk_asid_match && !blk_pfed) 
      */

  if (blk_asid_match) 
    pref.evict(evicted_addr);

  pref.pop_pf(addr);

  //if (!prefetch) 
    pref.filled_blks++;

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::STLB_PF[{this, cpu}];

  if (reset_misc::can_record_after_access) 
  {
    pref.update_pf_stats();
    pref.gather_pf();
    reset_misc::can_record_after_access = false;
    pref.hit_this_round = true;
    pref.filled_blks = 0;
    pref.to_be_pf_blks = pref.cs_q.size();
    std::cout << NAME << " STLB Prefetcher gathered " << pref.cs_q.size() << " prefetches." << std::endl;
    printf("%s overall prefetch accuracy = %ld / %ld = %f\n", NAME.c_str(), pref.pf_hit, pref.pf_issued, 1.0 * pref.pf_hit / (pref.pf_issued + 1.0));
    pref.last_issued_pf_moment = this->current_cycle - pref.wait_interval;
  }

  if (!pref.cs_q.empty() && (pref.filled_blks <= pref.to_be_pf_blks)) // && pref.hit_this_round 
    pref.issue(this);
}

void CACHE::prefetcher_final_stats() 
{
  auto &pref = ::STLB_PF[{this, cpu}];

  std::cout << "STLB prefetcher stats: " << pref.pf_hit << " / " << pref.pf_issued << std::endl;
}
