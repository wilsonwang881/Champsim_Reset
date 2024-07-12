#ifndef SPP_H
#define SPP_H

#include "signaturetable.h"
#include "bootstraptable.h"
#include "patterntable.h"
#include "filter.h"

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
// WL 

class CACHE;

namespace spp {
  class prefetcher
  {
    constexpr static std::size_t ACCURACY_BITS = 7;
    constexpr static std::size_t ISSUE_QUEUE_SIZE = 32;

    SIGNATURE_TABLE signature_table;
    BOOTSTRAP_TABLE bootstrap_table;
    PATTERN_TABLE pattern_table;
    SPP_PREFETCH_FILTER filter;
    std::deque<std::pair<uint64_t, bool>> issue_queue;
    std::deque<std::pair<uint64_t, bool>> context_switch_issue_queue; // WL

    struct pfqueue_entry_t
    {
      uint32_t sig = 0;
      int32_t  delta = 0;
      uint32_t depth = 0;
      int confidence = 0;
      uint64_t address = 0;
    };

    std::optional<pfqueue_entry_t> active_lookahead;

    // STATS
    std::unordered_map<uint32_t, unsigned> sig_count_tracker;
    std::vector<unsigned> depth_confidence_tracker;
    std::vector<unsigned> depth_interrupt_tracker;
    std::vector<unsigned> depth_offpage_tracker;
    std::vector<unsigned> depth_ptmiss_tracker;

    public:
    bool warmup = true;
    
    void update_demand(uint64_t base_addr, uint32_t set);
    void issue(CACHE* cache);
    void step_lookahead();
    void initiate_lookahead(uint64_t base_addr);
    void print_stats(std::ostream& ostr);
    
    // WL 
    bool context_switch_prefetch_gathered = false;
    std::ofstream prefetcher_state_file;
    uint64_t cache_cycle;

    void clear_states();
    void context_switch_gather_prefetches(CACHE* cache);
    std::optional<uint64_t> context_switch_aux(uint32_t &sig, int32_t delta, float &confidence, uint64_t page_num, uint32_t &last_offset);
    bool context_switch_queue_empty();
    void context_switch_queue_clear();
    void context_switch_issue(CACHE* cache);
    void record_spp_states();
    float CUTOFF_THRESHOLD = 0.2;
    // WL 
  };
} // namespace spp

#endif

