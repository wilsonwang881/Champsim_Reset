#ifndef PAGE_BITMAP_H
#define PAGE_BITMAP_H

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

class CACHE;

namespace page_bitmap 
{
  class prefetcher
  {
    constexpr static std::size_t TABLE_SIZE = 1024;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static std::size_t FILTER_SIZE = 4;
    constexpr static std::size_t TAG_COUNTER_SIZE = 1024;
    constexpr static std::size_t RJ_PF_SIZE = 1024;
    constexpr static bool DEBUG_PRINT = false;

    // Page bitmap entry.
    struct page_r
    {
      bool valid;
      uint64_t page_no;
      uint64_t page_no_store;
      bool bitmap[BITMAP_SIZE];
      bool bitmap_store[BITMAP_SIZE];
      uint16_t lru_bits;
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
    struct tag_counter_r
    {
      bool valid;
      uint8_t counter;
    };
      
    // Direcctly mapped table.
    tag_counter_r ct[TAG_COUNTER_SIZE];

    struct rj_pf_r
    {
      bool valid;
      uint16_t tag;
    };

    // Directly mapped table.
    rj_pf_r rj_tb[RJ_PF_SIZE];
    rj_pf_r pf_tb[RJ_PF_SIZE];

    public:

    int rejected_count = 0;

    // Context switch prefetch queue.
    std::deque<uint64_t> cs_pf; 

    void init();
    void update_lru(std::size_t i);
    void update(uint64_t addr);
    void update_bitmap(uint64_t addr);
    void update_bitmap_store();
    void clear_pg_access_status();
    void gather_pf();
    bool pf_q_empty();
    void filter_update_lru(std::size_t i);
    bool filter_operate(uint64_t addr);
    uint8_t saturating_counter(uint8_t val, bool increment);
    void tag_counter_update(uint64_t addr, bool useful);
    bool tag_counter_check(uint64_t addr);
    void invalidate_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
    void invalidate_p_tb(bool rj_or_pf_tb, uint64_t); // Wrapper for invalidate_rj_pf_tb().
    void update_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
    void update_p_tb(bool rj_or_pf_tb, uint64_t); // Wrapper for update_rj_pf_tb().
    bool check_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
    bool check_p_tb(bool rj_or_pf_tb, uint64_t addr); // Wrapper for check_rj_pf_tb().
  };
}

#endif
