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

  uint64_t page_num = addr >> 12;

  if (!cache_hit) 
  {
    bool found = false;

    for(auto var : pref.translations) 
    {
      if (var == page_num) 
      {
        found = true;
        break;
      } 
    }

    if (!found) 
      pref.translations.push_back(page_num);

    if (pref.translations.size() > 35) 
      pref.translations.pop_front();
    
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  //auto &pref = ::STLB_PF[{this, cpu}];

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::STLB_PF[{this, cpu}];

  if (reset_misc::can_record_after_access) 
  {
    pref.cs_q.clear();

    for(int i = pref.translations.size() - 1; i >= 0; i--) {
      pref.cs_q.push_back(pref.translations[i] << 12); 
    }

    pref.translations.clear();

    reset_misc::can_record_after_access = false;

    std::cout << NAME << " STLB Prefetcher gathered " << pref.cs_q.size() << " prefetches." << std::endl;
  }

  if (!pref.cs_q.empty()) 
  {
    bool pf_res = this->prefetch_line(pref.cs_q.front(), true, 0); 
    
    if (pf_res) 
    {
      pref.cs_q.pop_front(); 
    }
  }
}

void CACHE::prefetcher_final_stats() {}
