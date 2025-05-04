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

    std::deque<std::tuple<uint64_t, uint64_t, bool, bool>> context_switch_issue_queue; // addr, cycle, pf_to_this_level, RFO/WRITE
    SPP_ORACLE oracle;
    bool debug_print = false;
    uint64_t last_handled_addr;
    uint64_t cache_cycle;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;

    uint64_t issue(CACHE* cache);
    void call_poll(CACHE* cache);
    void erase_duplicate_entry_in_ready_queue(CACHE* cache, uint64_t addr);
    void update_do_not_fill_queue(std::deque<uint64_t> &dq, uint64_t addr, bool erase, CACHE* cache, std::string q_name);
  };
} // namespace spp_l3

#endif

