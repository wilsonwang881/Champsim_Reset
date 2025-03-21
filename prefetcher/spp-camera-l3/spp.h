#ifndef SPP_H
#define SPP_H

#include "oracle.h" // WL 

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <vector>
#include <algorithm> // WL: fixing error while compiling related to any_of

#include "champsim_constants.h"
#include "msl/bits.h"

// WL
#include <iostream>
#include <fstream>
#include <set>
#include <bitset>
#include <cassert>
// WL 

class CACHE;

namespace spp_l3 {
  class prefetcher
  {
    public:

    std::set<std::pair<uint64_t, bool>> available_prefetches;
    std::deque<std::tuple<uint64_t, bool, uint64_t, bool>> context_switch_issue_queue; // addr, pf_to_this_level, cycle, RFO/WRITE
    SPP_ORACLE oracle;
    bool debug_print = false;
    uint64_t last_handled_addr;
    uint64_t cache_cycle;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;
    std::set<uint64_t> rfo_write_addr;
    std::set<uint64_t> pending_write_fills;

    void issue(CACHE* cache);
    bool call_poll();
  };
} // namespace spp_l3

#endif

