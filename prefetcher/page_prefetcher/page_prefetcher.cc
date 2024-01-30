// WL 
#include "cache.h"
#include <unordered_set>
#include <map>

namespace {

  struct tracker {

    std::unordered_set<uint64_t> uniq_page_address;
    std::unordered_set<uint64_t> uniq_prefetched_page_address;
 
    public:

    bool context_switch_prefetch_gathered;

    std::deque<std::pair<uint64_t, bool>> context_switch_issue_queue;
    std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> context_switch_prefetching_timing; 

    void gather_context_switch_prefetches()
    {
      uniq_page_address.clear();
      context_switch_issue_queue.clear();

      for(auto var : reset_misc::before_reset_on_demand_access_records) {
        uniq_page_address.insert(var.ip >> 12);
        //uniq_page_address.insert((var.ip >> 12) - 1);
      }

      if (uniq_page_address.size() <= 8) {
        for(auto var : uniq_page_address) {
          for (size_t page_offset = 0; page_offset < (4096 - 64); page_offset += 64)
            context_switch_issue_queue.push_back({(var << 12) + page_offset, true});
        }
      }

      for(auto var : uniq_page_address) {
        std::cout << "Base address of page to be prefetched: " << std::hex << (var << 12) << std::dec << std::endl;  
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
      if (!std::empty(context_switch_issue_queue)
          && champsim::operable::cpu_side_reset_ready
          && champsim::operable::cache_clear_counter == 7) {
        auto [addr, priority] = context_switch_issue_queue.front();

        // If this fails, the queue was full.
        bool prefetched = cache->prefetch_line(addr, priority, 0);
        if (prefetched) {
          context_switch_issue_queue.pop_front();
          context_switch_prefetching_timing.push_back({addr, cache->current_cycle, 0});

          if (uniq_prefetched_page_address.find(addr >> 12) ==  uniq_prefetched_page_address.end()) {
            std::cout << "First prefetch in page " << std::hex << addr << " prefetched at cycle " << std::dec << cache->current_cycle << std::endl;
            uniq_prefetched_page_address.insert(addr >> 12); 
          }
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
      ::trackers[this].context_switch_prefetching_timing.clear();
      ::trackers[this].uniq_prefetched_page_address.clear();
    }
   
    // Issue prefetches until the queue is empty.
    if (!::trackers[this].context_switch_queue_empty())
    {
      ::trackers[this].context_switch_issue(this);

      for(auto [addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
        if (received_at == 0) {
          for(auto var : block) {
            if (var.valid && var.address == addr) {
              received_at == current_cycle; 
            }
          } 
        } 
      }
    }
    // Toggle switches after all prefetches are issued.
    else
    {
      std::unordered_set<uint64_t> printed_page_addresses;
      
      for(auto [addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
        if (printed_page_addresses.find(addr >> 12) == printed_page_addresses.end()) {
          
          std::cout << "Page with base address " << std::hex << addr << " issued at cycle " << std::dec << issued_at << " received at cycle " << received_at << std::endl; 
          printed_page_addresses.insert(addr >> 12);
        }
      }

      if (!champsim::operable::have_cleared_BTB
          && !champsim::operable::have_cleared_BP
          && champsim::operable::cpu_side_reset_ready
          && champsim::operable::cache_clear_counter == 7) {
        champsim::operable::context_switch_mode = false;
        champsim::operable::cpu_side_reset_ready = false;
        champsim::operable::cache_clear_counter = 0;
        ::trackers[this].context_switch_prefetch_gathered = false;
        std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
      }
    }
  }
}

void CACHE::prefetcher_final_stats() {}

// WL 
