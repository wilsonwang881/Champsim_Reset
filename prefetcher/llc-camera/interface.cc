#include "cache.h"
#include "camera.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, camera::prefetcher> CAMERA;
}

void CACHE::prefetcher_initialize()
{
  auto &pref = ::CAMERA[{this, cpu}];
  pref.init();

  std::cout << NAME << "-> Prefetcher LLC Camera initialized @ cycle " << current_cycle << "." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::CAMERA[{this, cpu}];

  if (type == champsim::to_underlying(access_type::LOAD) || type == champsim::to_underlying(access_type::WRITE))
  {
    pref.acc_operate(addr);
  }

  uint64_t pg_no = addr >> 12;

  for(auto var : pref.cs_pf) 
  {
    if ((var >> 12) == pg_no && (var >> 6) != (addr >> 6))
    {
      pref.cs_q.push_back(var); 
    } 
  }

  for(auto var : pref.cs_q) {
    pref.cs_pf.erase(var); 
  }
  
  pref.cs_pf.erase((addr >> 6) << 6);
  
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::CAMERA[{this, cpu}];

  if (reset_misc::can_record_after_access) 
  {
    pref.cs_pf.clear();
    pref.cs_q.clear();

    // Walk the cache to gather prefetches.
    for (size_t i = 0; i < NUM_SET; i++){
      
      for (size_t j = 0; j < 5; j++) {
      
        auto begin = std::next(std::begin(champsim::operable::lru_states), i * NUM_WAY);
        auto end = std::next(begin, NUM_WAY);

        // Find the way whose last use cycle is most distant
        auto target = std::min_element(begin, end);
        assert(begin <= target);
        assert(target < end);

        int index = target - std::begin(champsim::operable::lru_states);

        if (block[index].valid &&
            block[index].address != 0 &&
            block[index].asid == (champsim::operable::currently_active_thread_ID - 1)) 
        {
          pref.cs_pf.insert((block[index].address >> 6) << 6); 
        }

        *target = std::numeric_limits<uint64_t>::max();
      }
    }

    /*
    for(auto var : pref.cs_pf) {
      pref.cs_q.push_back(var); 
    }
    */

    pref.acc.clear();
    pref.issued_cs_pf.clear();
    reset_misc::can_record_after_access = false; 
    std::cout << "LLC Camera gathered " << pref.cs_pf.size() << " prefetches." << std::endl;
  }

  if (!pref.cs_q.empty()) 
  {
    auto pq = this->get_pq_occupancy();

    /*
    for(auto var : pq) {
      if (pref.cs_q.front() == var.address) {
        pref.cs_q.pop_front(); 
      } 
    }
    */

    //if(pq[2] <= 20)
    {
      bool res = this->prefetch_line(pref.cs_q.front(), false, 0);

      if (res)
      {
        pref.issued++;
        pref.issued_cs_pf.insert(pref.cs_q.front());
        pref.cs_q.pop_front();
      }
    }
  }
}

void CACHE::prefetcher_final_stats() 
{
  auto &pref = ::CAMERA[{this, cpu}];

  std::cout << "LLC Camera issued " << pref.issued << " prefetches." << std::endl;
}

