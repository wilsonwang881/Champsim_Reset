// WL 
#include "cache.h"
#include <unordered_set>
#include <map>

namespace {

  struct tracker {

    std::unordered_set<uint64_t> uniq_page_address;
 
    public:

    bool context_switch_prefetch_gathered;

    std::deque<std::pair<uint64_t, bool>> context_switch_issue_queue;

    void gather_context_switch_prefetches()
    {
      uniq_page_address.clear();
      context_switch_issue_queue.clear();

      for(auto var : reset_misc::before_reset_on_demand_access_records) {
        uniq_page_address.insert(var.ip >> 12);
      }

      if (uniq_page_address.size() <= 6) {
        for(auto var : uniq_page_address) {
          for (size_t page_offset = 0; page_offset < (4096 - 64); page_offset += 64)
            context_switch_issue_queue.push_back({var + page_offset, true});
        }
      }

      std::cout << "Ready to issue prefetches for " << uniq_page_address.size() << " page(s)" << std::endl;
    }

    bool context_switch_queue_empty()
    {
      return context_switch_issue_queue.empty();
    }

    void context_switch_issue(CACHE* cache)
    {
    // Issue eligible outstanding prefetches
    if (!std::empty(context_switch_issue_queue)) {
      auto [addr, priority] = context_switch_issue_queue.front();

      // If this fails, the queue was full.
      bool prefetched = cache->prefetch_line(addr, priority, 0);
      if (prefetched) {
        context_switch_issue_queue.pop_front();
      }
    }
    }
  };

  std::map<CACHE*, tracker> trackers;
}

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " -> Prefetcher initialized @ " << current_cycle << " cycles." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  // Gather and issue prefetches after a context switch.
  if (champsim::operable::context_switch_mode)
  {
    // Gather prefetches
    if (!::trackers[this].context_switch_prefetch_gathered)
    {

      ::trackers[this].gather_context_switch_prefetches(); 
      ::trackers[this].context_switch_prefetch_gathered = true;
    }
   
    // Issue prefetches until the queue is empty.
    if (!::trackers[this].context_switch_queue_empty())
    {
      ::trackers[this].context_switch_issue(this);
    }
    // Toggle switches after all prefetches are issued.
    else
    {
      champsim::operable::context_switch_mode = false;
      ::trackers[this].context_switch_prefetch_gathered = false;
      std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
    }
  }
}

void CACHE::prefetcher_final_stats() {}

// WL 
