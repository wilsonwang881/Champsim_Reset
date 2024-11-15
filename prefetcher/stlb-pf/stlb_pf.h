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

class CACHE;

namespace stlb_pf
{
  class prefetcher
  {
    constexpr static std::size_t DQ_SIZE = 100;

    public:

    bool hit_this_round = false;

    uint64_t pf_issued = 0;
    uint64_t pf_hit = 0;
    uint64_t to_be_pf_blks = 0;
    uint64_t filled_blks = 0;
    std::set<uint64_t> pf_blks;
    std::set<uint64_t> hit_blks;

    // Context switch prefetch queue.
    std::deque<uint64_t> translations; 
    std::deque<uint64_t> cs_q;

    void update(uint64_t addr);
    void pop_pf(uint64_t addr);
    void evict(uint64_t addr);
    void gather_pf();
    void issue(CACHE* cache);
    void check_hit(uint64_t addr);
    void update_pf_stats();
  };
}

#endif
