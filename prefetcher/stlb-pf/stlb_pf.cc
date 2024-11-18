#include "stlb_pf.h"
#include "cache.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, stlb_pf::prefetcher> STLB_PF; 
}

void stlb_pf::prefetcher::update(uint64_t addr, uint64_t ip)
{
  uint64_t page_num = addr >> 12;

  auto el = std::find(translations.begin(), translations.end(), page_num);

  if (el == translations.end()) 
    translations.push_back(page_num);
  else
  {
    translations.erase(el);
    translations.push_back(page_num);
  }

  if (translations.size() > DQ_SIZE) 
    translations.pop_front();

  // Avoid duplicates in translations and translations_ip deques.
  if (addr == ip)
    return; 

  page_num = ip >> 12;

  el = std::find(translations_ip.begin(), translations_ip.end(), page_num);

  if (el == translations_ip.end()) 
    translations_ip.push_back(page_num);
  else
  {
    translations_ip.erase(el);
    translations_ip.push_back(page_num);
  }

  if (translations_ip.size() > DQ_IP_SIZE) 
    translations_ip.pop_front();
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

  int limit = 0;

  if (accuracy <= 0.4) 
    limit = std::round(translations.size() * (1 - accuracy));

  int ip_translation_counter = translations_ip.size() - 1;

  for(int i = translations.size() - 1; i >= limit; i--)
  {
    cs_q.push_back(translations[i] << 12); 

    if (ip_translation_counter >= 0) 
    {
      cs_q.push_back(translations_ip[ip_translation_counter] << 12); 
      ip_translation_counter--;
      i--;
    }
  }

  translations.clear();
  translations_ip.clear();
}

void stlb_pf::prefetcher::issue(CACHE* cache)
{
  bool pf_res = cache->prefetch_line(cs_q.front(), true, 0); 
  
  if (pf_res) 
  {
    cs_q.pop_front(); 
    pf_issued++;
  }
}

void stlb_pf::prefetcher::update_pf_stats()
{
  accuracy = (pf_hit - pf_hit_last_round + 1.0) / (pf_issued - pf_issued_last_round + 1.0) * 1.0;

  printf("STLB PF accuracy = %f\n", accuracy);

  pf_hit_last_round = pf_hit;
  pf_issued_last_round = pf_issued;
}

