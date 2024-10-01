#ifndef PAGE_BITMAP_H
#define PAGE_BITMAP_H

#include "ppf.h"

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

namespace page_bitmap 
{

  class prefetcher
  {
    constexpr static std::size_t TABLE_SIZE = 1024;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static std::size_t FILTER_SIZE = 256;
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

    PAGE_BITMAP_PPF ppf;

    public:

    int pf_count = 0;
    int pf_useful = 0;
    uint64_t threshold_cycle = 0;

    std::set<uint64_t> pf_blks;
    std::set<uint64_t> hit_blks;

    // Context switch prefetch queue.
    std::deque<uint64_t> cs_pf; 
    std::deque<uint64_t> cs_weight;

    void init();
    void update_lru(std::size_t i);
    void update(uint64_t addr);
    void update_bitmap(uint64_t addr);
    void invalidate_bitmap(uint64_t addr);
    void update_bitmap_store();
    void clear_pg_access_status();
    void gather_pf();
    bool pf_q_empty();
    void filter_update_lru(std::size_t i);
    bool filter_operate(uint64_t addr);
    int perceptron_check(uint64_t addr); 
    void perceptron_update(uint64_t addr, bool useful);
    void invalidate_p_tb(bool rj_or_pf_tb, uint64_t); 
    void update_p_tb(bool rj_or_pf_tb, uint64_t); 
    bool check_p_tb(bool rj_or_pf_tb, uint64_t addr);
    int check_set_overlap();
  };
}

#endif
