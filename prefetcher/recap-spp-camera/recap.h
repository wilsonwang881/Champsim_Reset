#ifndef RECAP_H 
#define RECAP_H 

#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <deque>
#include <algorithm>
#include <vector>

namespace spp
{
  class SPP_RECAP
  {
    constexpr static std::size_t TABLE_SIZE = 1024;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static std::size_t FILTER_SIZE = 4;

    // Page bitmap entry.
    struct page_r
    {
      bool valid;
      uint64_t page_no;
      bool bitmap[BITMAP_SIZE];
      uint16_t lru_bits;
      bool reuse;
    };

    page_r tb[TABLE_SIZE];

    public:

    constexpr static bool BLOCK_REUSE_MODE = false;

    // Context switch prefetch queue.
    std::vector<uint64_t> cs_pf; 

    void init();
    void update_lru(std::size_t i);
    void update(uint64_t addr);
    void update_reuse(uint64_t addr);
    std::vector<uint64_t> gather_pf();
    bool pf_q_empty();
  };
}

#endif
