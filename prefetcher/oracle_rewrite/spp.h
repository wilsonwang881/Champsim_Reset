#ifndef SPP_H
#define SPP_H

#include "oracle.h" // WL 

#include <cstdlib>
#include <deque>
#include <unordered_map>
#include <vector>
#include <algorithm> 
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

    std::deque<std::tuple<uint64_t, uint64_t, bool, bool>> ready_queue; // addr, cycle, pf_to_this_level, RFO/WRITE
    SPP_ORACLE oracle;
    bool debug_print = false;
    uint64_t last_handled_addr;
    uint64_t cache_cycle;
    std::set<uint64_t> issued_pf;
    uint64_t issued_pf_hit;
    uint64_t total_issued_pf;
    std::set<uint64_t> rfo_write_addr;
    std::set<uint64_t> pending_write_fills;

    uint64_t issue(CACHE* cache);
    bool call_poll();
  };
} // namespace spp_l3

#endif
