#include <array>
#include <bitset>
#include <cstdint>

#include "champsim_constants.h"

// WL
#include <iostream>
#include <vector>

namespace spp
{
  constexpr std::size_t GLOBAL_COUNTER_BITS = 10;
  enum confidence_t {REJECT, WEAKLY_ACCEPT, STRONGLY_ACCEPT}; // Request type for prefetch filter check

  class SPP_PREFETCH_FILTER
  {
    public:
      static constexpr std::size_t SET = 1 << 8;
      static constexpr std::size_t WAY = 1;
      static constexpr std::size_t TAG_BITS = 27;

    private:
      struct filter_entry_t
      {
        uint64_t     page_no = 0;
        unsigned     last_used = 0;
        std::bitset<PAGE_SIZE/BLOCK_SIZE> prefetched;
        std::bitset<PAGE_SIZE/BLOCK_SIZE> used;
      };

      unsigned access_count = 0;

      std::array<filter_entry_t, WAY * SET> prefetch_table;

    public:
      // friend class is_valid<filter_entry_t>; // WL: commented out

      // Global counters to calculate global prefetching accuracy
      bool tagless = false;
      unsigned long pf_useful = 0, pf_issued = 0;
      const static int highconf_threshold = 40;

      unsigned repl_count = 0;

      confidence_t check(uint64_t pf_addr, int confidence = highconf_threshold) const;
      void update_demand(uint64_t pf_addr, std::size_t set);
      void update_issue(uint64_t pf_addr, std::size_t set);
      std::vector<uint64_t> gather_pf(); // WL 
      void clear(); // WL
  };
}

