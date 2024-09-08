#ifndef PAGE_BITMAP_L2_H
#define PAGE_BITMAP_L2_H

#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <deque>
#include <algorithm>
#include <vector>

class CACHE;

namespace page_bitmap_l2 
{
  class prefetcher
  {
    constexpr static std::size_t TABLE_SIZE = 1024;
    constexpr static std::size_t BITMAP_SIZE = 64;

    struct page_r
    {
      bool valid;
      uint64_t page_no;
      bool bitmap[BITMAP_SIZE];
      uint16_t lru_bits;
    };

    page_r tb[TABLE_SIZE];
 
    public:

    std::deque<uint64_t> cs_pf; 

    void init();
    void update_lru(std::size_t i);
    void update(uint64_t addr);
    void gather_pf();
    bool pf_q_empty();
  };
}

#endif
