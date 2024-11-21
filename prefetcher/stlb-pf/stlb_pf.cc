#include "stlb_pf.h"
#include "cache.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF; 
}

void stlb_pf::prefetcher::update(uint64_t addr, uint64_t ip)
{
  // Update addr.
  uint64_t page_num = addr >> 12;

  auto el = std::find(translations.begin(), translations.end(), page_num);

  if (el == translations.end())
  {
    translations.push_back(page_num);
    pushed_el++;
  }
  else
  {
    translations.erase(el);
    translations.push_back(page_num);
    pushed_el++;
  }

  if (translations.size() > DQ_SIZE) 
    translations.pop_front();

  // Update ip.
  // Check for ip number limit.
  page_num = ip >> 12;

  el = std::find(translations_ip.begin(), translations_ip.end(), page_num);

  if (el == translations_ip.end()) 
    translations_ip.push_back(page_num);
  else
  {
    translations_ip.erase(el);
    translations_ip.push_back(page_num);
  }

  el = std::find(translations.begin(), translations.end(), page_num);

  if (el == translations.end()) 
    translations.push_back(page_num);
  else
  {
    translations.erase(el);
    translations.push_back(page_num);
  }

  uint64_t pop_candidate;

  if (translations_ip.size() > DQ_IP_SIZE) 
  {
    pop_candidate = translations_ip.front();
    translations_ip.pop_front();
    el = std::find(translations.begin(), translations.end(), pop_candidate);

    if (el != translations.end()) 
      translations.erase(el);
  }
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

  evict_pos = std::find(translations_ip.begin(), translations_ip.end(), page_num);

  if (evict_pos != translations_ip.end())
    translations_ip.erase(evict_pos);
}

void stlb_pf::prefetcher::gather_pf()
{
  cs_q.clear();

  int limit = translations.size() - std::round(translations.size() * accuracy); // * (translations.size() * 1.0 / DQ_SIZE);

  std::cout << "limit = " << limit << " translations.size() = " << (unsigned)translations.size() << " translations_ip.size() = " << (unsigned)translations_ip.size() << std::endl;

  for(int i = translations.size() - 1; i >= limit; i--) 
    cs_q.push_back(translations[i] << 12); 

  translations.clear();
  translations_ip.clear();
}

void stlb_pf::prefetcher::issue(CACHE* cache)
{
  if (cache->current_cycle >= (last_issued_pf_moment + wait_interval)) 
  {
    bool pf_res = cache->prefetch_line(cs_q.front(), true, 0); 
    
    if (pf_res) 
    {
      pf_issued++;
      cs_q.pop_front(); 
      last_issued_pf_moment = cache->current_cycle;
    }
  }
}

void stlb_pf::prefetcher::update_pf_stats()
{
  printf("%s hits / accesses = %ld / %ld  = %f\n", "STLB", hits, accesses, 1.0 * hits / accesses);

  if ((pf_hit == 0 && pf_issued == 0) || (pf_issued == pf_issued_last_round))
    accuracy = 1.0 * hits / accesses;  
  else 
    accuracy = (pf_hit  - pf_hit_last_round * 1.0) / (pf_issued - pf_issued_last_round * 1.0) * 1.0;

  printf("STLB PF accuracy = %f\n", accuracy);

  pf_hit_last_round = pf_hit;
  pf_issued_last_round = pf_issued;
  wait_interval = std::round(4000000 * 0.5 / pushed_el);
  pushed_el = 0;
  printf("STLB PF issue wait cycle = %ld\n", wait_interval);
}

