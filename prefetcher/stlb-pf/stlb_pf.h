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
    static const uint64_t CACHED_TRANSLATIONS;

    struct translation
    {
      bool valid;
      uint64_t page_no;
      uint8_t block_no;
      uint8_t lru_bits;
    };


    public:

    int pf_count = 0;
    int pf_useful = 0;
    uint64_t threshold_cycle = 0;

    std::set<uint64_t> pf_blks;
    std::set<uint64_t> hit_blks;

    // Context switch prefetch queue.
    std::deque<uint64_t> translations; 
    std::deque<uint64_t> cs_q;

  };
}

#endif
