#ifndef STLB_PF_H 
#define STLB_PF_H

#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <vector>
#include <set>
#include <cassert>
#include <cmath>

class CACHE;

namespace stlb_pf
{
  class prefetcher
  {
    public:

    uint64_t DQ_IP_SIZE = 300;
    uint64_t DQ_SIZE = 1024;
    bool hit_this_round = false;

    uint64_t pf_hit_last_round = 0;
    uint64_t pf_issued_last_round = 0;
    float accuracy;
    uint64_t pf_issued = 0;
    uint64_t pf_hit = 0;
    uint64_t to_be_pf_blks = 0;
    uint64_t filled_blks = 0;

    // Context switch prefetch queue.
    std::deque<uint64_t> translations; 
    std::deque<uint64_t> translations_ip;
    std::deque<uint64_t> cs_q;

    void update(uint64_t addr, uint64_t ip);
    void pop_pf(uint64_t addr);
    void evict(uint64_t addr);
    void gather_pf();
    void issue(CACHE* cache);
    void update_pf_stats();
  };
}

#endif
