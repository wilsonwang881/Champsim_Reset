#ifndef _SPP_PAGE_BITMAP
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

  class SPP_PAGE_BITMAP 
  {
    constexpr static std::size_t TABLE_SIZE = 1024;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static std::size_t FILTER_SIZE = 128;
    constexpr static std::size_t COUNTER_SIZE = 2048;
    constexpr static bool PAGE_BITMAP_DEBUG_PRINT = false;

    // Page bitmap entry.
    struct page_r
    {
      bool valid;
      uint64_t page_no;
      uint64_t page_no_store;
      bool bitmap[BITMAP_SIZE];
      bool bitmap_store[BITMAP_SIZE];
      uint16_t lru_bits;
      uint64_t perc_sum;
      bool aft_cs_acc;
    };

    page_r tb[TABLE_SIZE];

    // Filter.
    // Make sure each page has 1 access before putting into tb.
    struct page_filter_r
    {
      bool valid;
      uint64_t page_no;
      uint8_t block_no;
      uint8_t lru_bits;
    };

    page_filter_r filter[FILTER_SIZE];

    // Counter for each block access.
    struct counter_r
    {
      bool valid;
      uint64_t addr;
      uint8_t counter;
      uint16_t lru_bits;
    };

    counter_r ct[COUNTER_SIZE];

    public:

    // Context switch prefetch queue.
    std::deque<uint64_t> cs_pf; 

    void init();
    void update_lru(std::size_t i);
    void update(uint64_t addr);
    void update_bitmap(uint64_t addr);
    void update_bitmap_store();
    void clear_pg_access_status();
    std::vector<uint64_t> gather_pf();
    bool pf_q_empty();
    void filter_update_lru(std::size_t i);
    bool filter_operate(uint64_t addr);
    void counter_update_lru(std::size_t i);
    void counter_update(uint64_t addr);
  };
}
#endif
