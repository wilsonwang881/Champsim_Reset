#include "stlb_pf.h"
#include "cache.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF; 
}

void stlb_pf::prefetcher::update(uint64_t addr)
{
  uint64_t page_num = addr >> 12;
  bool found = false;

  for(auto var : translations) 
  {
    if (var == page_num) 
    {
      found = true;
      break;
    } 
  }

  if (!found) 
    translations.push_back(page_num);

  if (translations.size() > DQ_SIZE) 
    translations.pop_front();
}

void stlb_pf::prefetcher::gather_pf()
{
  cs_q.clear();
  available_pf.clear();

  for(int i = translations.size() - 1; i >= 0; i--) {
    cs_q.push_back(translations[i] << 12); 
    available_pf.insert(translations[i] << 12);
  }
}

void stlb_pf::prefetcher::issue(CACHE* cache)
{
  bool pf_res = cache->prefetch_line(cs_q.front(), true, 0); 
  
  if (pf_res) 
    cs_q.pop_front(); 
}

