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

void stlb_pf::prefetcher::evict(uint64_t addr)
{
  uint64_t page_num = addr >> 12;

}

void stlb_pf::prefetcher::gather_pf()
{
  cs_q.clear();

  for(int i = translations.size() - 1; i >= 0; i--) {
    cs_q.push_back(translations[i] << 12); 
  }
}

void stlb_pf::prefetcher::issue(CACHE* cache)
{
  bool pf_res = cache->prefetch_line(cs_q.front(), true, 0); 
  
  if (pf_res) 
  {
    pf_blks.insert(cs_q.front());
    cs_q.pop_front(); 
  }
}

void stlb_pf::prefetcher::check_hit(uint64_t addr)
{
  addr = addr & 0xFFF;

  if (pf_blks.find(addr) != pf_blks.end())
  {
    std::cout << "Hit" << std::endl;
    hit_blks.insert(addr); 
  }
}

void stlb_pf::prefetcher::update_pf_stats()
{
  pf_issued += pf_blks.size();
  pf_blks.clear();
  pf_hit += hit_blks.size();
  hit_blks.clear();
}

