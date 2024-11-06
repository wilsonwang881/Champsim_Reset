#include "stlb_pf.h"
#include "cache.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF; 
}

void stlb_pf::prefetcher::update(uint64_t addr)
{
  uint64_t page_num = addr >> 12;

  auto el = std::find(translations.begin(), translations.end(), page_num);

  if (el == translations.end()) 
    translations.push_back(page_num);
  /*
  else
  {
    translations.erase(el);
    translations.push_back(page_num);
  }
  */

  if (translations.size() > DQ_SIZE) 
    translations.pop_front();
}

void stlb_pf::prefetcher::pop_pf(uint64_t addr)
{
  addr = (addr >> 12) << 12;

  auto el = std::find(cs_q.begin(), cs_q.end(), addr);

  if (el != cs_q.end())
    cs_q.erase(el);
}

void stlb_pf::prefetcher::evict(uint64_t addr)
{
  uint64_t page_num = addr >> 12;

  auto evict_pos = std::find(translations.begin(), translations.end(), page_num);

  if (evict_pos != translations.end())
    translations.erase(evict_pos);
}

void stlb_pf::prefetcher::gather_pf()
{
  cs_q.clear();
  pf_blks.clear();
  hit_blks.clear();

  for(int i = translations.size() - 1; i >= 0; i--)
    cs_q.push_back(translations[i] << 12); 

  //translations.clear();
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
  uint64_t page_no = (addr >> 12) << 12;

  if (pf_blks.find(page_no) != pf_blks.end())
    hit_blks.insert(addr); 
}

void stlb_pf::prefetcher::update_pf_stats()
{
  pf_issued += pf_blks.size();
  pf_hit += hit_blks.size();
}

