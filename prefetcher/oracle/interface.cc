#include "cache.h"
#include "oracle.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, oracle::prefetcher> ORACLE;
}

void CACHE::prefetcher_initialize()
{
  auto &pref = ::ORACLE[{this, cpu}];
  pref.init();

  std::cout << NAME << "-> Prefetcher Oracle initialized @ cycle " << current_cycle << "." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::ORACLE[{this, cpu}];
 
  pref.update(addr);

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::ORACLE[{this, cpu}];

  if (reset_misc::can_record_after_access) 
  {
    pref.can_write = true;
    reset_misc::can_record_after_access = false;
    pref.file_read();
  }
  else 
  {
    if (!pref.cs_pf.empty()) {
      bool prefetched = prefetch_line(pref.cs_pf.front(), 0, 0);

      if (prefetched) 
      {
        pref.cs_pf.pop_front(); 
      }
    }
  }
}

void CACHE::prefetcher_final_stats()
{
  auto &pref = ::ORACLE[{this, cpu}];
  pref.finish();
}

