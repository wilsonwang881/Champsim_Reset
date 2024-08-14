// WL 
#include "cache.h"
#include <unordered_set>
#include <set>
#include <map>
#include <cassert>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 64
#define INS_PREFETCH_UNIT_SIZE 64
#define NUMBER_OF_PREFETCH_UNIT 8000
#define HISTORY_SIZE 9000
#define CUTOFF 1
#define READ_ON_DEMAND_ACCESS_L1D 0
#define RETRY_LIMIT 5

namespace {

  struct tracker {

    std::set<uint64_t> uniq_page_address;
    std::set<uint64_t> uniq_ins_page_address;
    std::unordered_set<uint64_t> uniq_prefetched_page_address;
    std::deque<std::pair<uint64_t, uint64_t>> past_accesses;
    uint64_t issued_context_switch_prefetches = 0;
    uint64_t not_issued_context_switch_prefetches = 0;
    int prefetch_attempt = 0;
    int waited = 0;
    int runahead_prefetch_limit = 50;
 
    public:

    bool context_switch_prefetch_gathered;

    std::ifstream L1D_access_file;

    std::deque<std::pair<uint64_t, bool>> context_switch_issue_queue;
    std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> context_switch_prefetching_timing; 

    bool check_within_block(uint64_t ref, uint64_t addr)
    {

      if (addr >= ref &&
          addr < (ref + PREFETCH_UNIT_SIZE)) {

          return true;
      }

      return false;
    }

    void gather_context_switch_prefetches()
    {
      uniq_page_address.clear();
      context_switch_issue_queue.clear();

      std::deque<reset_misc::on_demand_data_access> dq_cpy(reset_misc::dq_before_data_access.size());
      std::copy(reset_misc::dq_before_data_access.begin(), reset_misc::dq_before_data_access.end(), dq_cpy.begin());

      std::deque<reset_misc::on_demand_ins_access> dq_cpy_ins(reset_misc::dq_before_ins_access.size());
      std::copy(reset_misc::dq_before_ins_access.begin(), reset_misc::dq_before_ins_access.end(), dq_cpy_ins.begin());

      for (size_t i = 0; i < HISTORY_SIZE; i++) {
         if (uniq_page_address.size() <= NUMBER_OF_PREFETCH_UNIT - 1 &&
             dq_cpy.size() > 0) {
           if (dq_cpy.back().load_or_store && 
               dq_cpy.back().occurance > 1) {

             reset_misc::dq_pf_data_access.push_back(dq_cpy.back());
             reset_misc::dq_pf_data_access.back().addr_rec.clear();

             for(auto var : dq_cpy.back().addr_rec) {
               //std::cout << "addr = " << var.addr << " occr = " << var.occr << std::endl;
               if (var.occr > 1) {
                 uniq_page_address.insert(var.addr >> PREFETCH_UNIT_SHIFT); 
                 reset_misc::dq_pf_data_access.back().addr_rec.push_back(var);
                 //std::cout << "pushing" << std::endl;
               }
             }

             //std::cout << "Occurance = " << dq_cpy.back().occurance << " Data size = " << dq_cpy.back().addr.size() << std::endl;
             uniq_ins_page_address.insert(dq_cpy.back().ip >> PREFETCH_UNIT_SHIFT); 
           }
           dq_cpy.pop_back();
         }

        /*
        if (uniq_ins_page_address.size() <= NUMBER_OF_PREFETCH_UNIT - 1 &&
            dq_cpy_ins.size() > 0) {
           if (dq_cpy_ins.back().occurance > 0) {
            uniq_ins_page_address.insert(dq_cpy_ins.back().ip >> PREFETCH_UNIT_SHIFT); 
           }

           dq_cpy_ins.pop_back();
         }
         */
      }

      if (uniq_page_address.size() <= NUMBER_OF_PREFETCH_UNIT) {
        for(auto var : uniq_page_address) {
          for (int page_offset = 0; page_offset < PREFETCH_UNIT_SIZE; page_offset = (page_offset + 64)) // Half page prefetching
          {
            auto prefetch_target = std::make_pair((var << PREFETCH_UNIT_SHIFT) + page_offset, true);
            if (std::find(context_switch_issue_queue.begin(), context_switch_issue_queue.end(), prefetch_target) == context_switch_issue_queue.end() 
                && prefetch_target.first < (((prefetch_target.first >> LOG2_PAGE_SIZE) << LOG2_PAGE_SIZE) + 4096)
                && prefetch_target.first >= ((prefetch_target.first >> LOG2_PAGE_SIZE) << LOG2_PAGE_SIZE)) {
              context_switch_issue_queue.push_back(prefetch_target);
            }
          }
        }
      }

      if (uniq_ins_page_address.size() <= NUMBER_OF_PREFETCH_UNIT) {
        for(auto var : uniq_ins_page_address) {
          for (size_t page_offset = 0; page_offset < INS_PREFETCH_UNIT_SIZE; page_offset = (page_offset + 64)) // Half page prefetching
          {
            auto prefetch_target = std::make_pair((var << PREFETCH_UNIT_SHIFT) + page_offset, true);
            if (std::find(context_switch_issue_queue.begin(), context_switch_issue_queue.end(), prefetch_target) == context_switch_issue_queue.end() &&
                prefetch_target.first < (((prefetch_target.first >> LOG2_PAGE_SIZE) << LOG2_PAGE_SIZE) + 4096)) {
              context_switch_issue_queue.push_back(prefetch_target);
        }
          }
        }
      }

      reset_misc::dq_prefetch_communicate.clear();
      std::vector<uint64_t> sort_container_;

      for(auto var : context_switch_issue_queue) {
        sort_container_.push_back(var.first); 
      }

      sort(sort_container_.begin(), sort_container_.end());

      for(auto var : sort_container_) {
        reset_misc::dq_prefetch_communicate.push_back(std::make_pair(var, true));
      }

      std::cout << "Size = " << reset_misc::dq_prefetch_communicate.size() << std::endl;

      if (READ_ON_DEMAND_ACCESS_L1D) {

        context_switch_issue_queue.clear();

        //uint64_t separator, r_cycle, r_ip, r_occurance;
        //L1D_access_file >> separator >> r_cycle >> r_ip >> r_occurance;

        uint64_t r_access, r_occurance;
        int readin_count = 0;
        reset_misc::dq_prefetch_communicate.clear();
        std::vector<uint64_t> sort_container;

        while (L1D_access_file >> r_access >> r_occurance) {
          if (r_access == 99999) {
            break; 
          } 

          //std::cout << "Access = " << r_access << std::endl;

          bool found = false;

          for(auto var : sort_container) {
            if (var == r_access) {
              found = true; 
            }
          }

          readin_count++;

          if (!found) {
            sort_container.push_back(r_access);
            //reset_misc::dq_prefetch_communicate.push_back(std::make_pair(r_access, true));
            //context_switch_issue_queue.push_back(std::make_pair(r_access, true));
          }
        }

        sort(sort_container.begin(), sort_container.end());

        for(auto var : sort_container) {
          reset_misc::dq_prefetch_communicate.push_back(std::make_pair(var, true));
        }

        std::cout << "Read in count = " << readin_count << std::endl;
      }

      std::cout << "PREFETCH_UNIT_SHIFT = " << PREFETCH_UNIT_SHIFT << " PREFETCH_UNIT_SIZE = " << PREFETCH_UNIT_SIZE << " NUMBER_OF_PREFETCH_UNIT = " << NUMBER_OF_PREFETCH_UNIT << std::endl; 

      /*
      for(auto var : uniq_page_address) {
        //std::cout << "Base address of page to be prefetched: " << std::hex << (var << PREFETCH_UNIT_SHIFT) << std::dec << std::endl;  
      }
      for(auto var : context_switch_issue_queue) {
        std::cout << std::hex << var.first << std::dec << std::endl; 
      }
      */ 

      std::cout << "LLC Prefetcher: ready to issue prefetches for " << uniq_page_address.size() << " + " << uniq_ins_page_address.size() << " page(s) and " << context_switch_issue_queue.size() << " " << reset_misc::dq_prefetch_communicate.size() << " prefetch(es)" << std::endl;
    }

    bool context_switch_queue_empty()
    {
      return context_switch_issue_queue.empty();
    }

    bool context_switch_issue(CACHE* cache)
    {
      bool prefetched = false;

      // Issue eligible outstanding prefetches
      if (!std::empty(reset_misc::dq_prefetch_communicate)) {//&& champsim::operable::cache_clear_counter == 7
        auto [addr, priority] = reset_misc::dq_prefetch_communicate.front();

        // If this fails, the queue was full.
        assert(priority);

        if (waited >= RETRY_LIMIT) {
          
          prefetched = cache->prefetch_line(addr, priority, 0);

          if (prefetched) {
            reset_misc::dq_prefetch_communicate.pop_front();
            context_switch_prefetching_timing.push_back({addr, cache->current_cycle, 0});
            issued_context_switch_prefetches++;

            if (issued_context_switch_prefetches % 500 == 0 ||
                reset_misc::dq_prefetch_communicate.size() == 0) {
              std::cout << "LLC Have prefetched " << issued_context_switch_prefetches << " blocks" << std::endl; 
            }

            //std::cout << "Prefetched " << addr << " at " << std::dec << cache->current_cycle << std::endl;

            /*
            if (uniq_prefetched_page_address.find(addr >> 12) ==  uniq_prefetched_page_address.end()) {
              //std::cout << "First prefetch in page " << std::hex << addr << " prefetched at cycle " << std::dec << cache->current_cycle << std::endl;
              uniq_prefetched_page_address.insert(addr >> 12); 
            }
            */
          }

          waited = 0;
        }
        else {
          waited++;
        }
        /*
        else {
          //std::cout << "Failed prefetch " << addr << std::endl;
          prefetch_attempt++;
          if (prefetch_attempt > RETRY_LIMIT) {
            context_switch_issue_queue.pop_front(); 
            prefetch_attempt = 0;
          }
        }
        */
      }

    return prefetched;
    }
  };

  std::map<CACHE*, tracker> trackers;
}

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " -> Prefetcher LLC Prefetcher initialized @ " << current_cycle << " cycles." << std::endl;

  if (READ_ON_DEMAND_ACCESS_L1D) {
    ::trackers[this].L1D_access_file.open("L1D_on_demand_access.txt", std::ios::in); 
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  if (::trackers[this].past_accesses.size() >= HISTORY_SIZE) {
    if (::trackers[this].check_within_block(::trackers[this].past_accesses.back().first, addr)) {
      ::trackers[this].past_accesses.back().second++; 
    }
    else {
      ::trackers[this].past_accesses.pop_front();
      ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
    }
  }
  else if (::trackers[this].past_accesses.size() == 0) {
    ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
  }
  else {
    if (::trackers[this].check_within_block(::trackers[this].past_accesses.back().first, addr)) {
      ::trackers[this].past_accesses.back().second++; 
    }
    else {
      ::trackers[this].past_accesses.push_back(std::make_pair(addr, 1));
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  //std::cout << "Page prefetcher operate()" << std::endl;
  // Gather and issue prefetches after a context switch.
  if (champsim::operable::context_switch_mode)
  {
    // Gather prefetches
    if (!::trackers[this].context_switch_prefetch_gathered)
    {
      this->clear_internal_PQ();
      reset_misc::dq_pf_data_access.clear();
      ::trackers[this].runahead_prefetch_limit = 0;
      ::trackers[this].gather_context_switch_prefetches(); 
      ::trackers[this].context_switch_prefetch_gathered = true;
      ::trackers[this].context_switch_prefetching_timing.clear();
      ::trackers[this].uniq_prefetched_page_address.clear();
    }
   
    // Issue prefetches until the queue is empty.
    //if (!reset_misc::dq_prefetch_communicate.empty()) //!::trackers[this].context_switch_queue_empty())
    /*
    if (::trackers[this].runahead_prefetch_limit < 50) 
    {
      bool attempt = false;

      if (champsim::operable::cpu_side_reset_ready) {
       attempt = ::trackers[this].context_switch_issue(this);

       
        for(auto &[addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
          if (received_at == 0) {
            for(auto var : block) {
              if (var.valid && var.address == addr) {
                received_at = this->current_cycle; 
              }
            } 
          } 
        }
      }

      if (attempt) {
        ::trackers[this].runahead_prefetch_limit++; 
      }
    }
    */
    // Toggle switches after all prefetches are issued.
    else
    //if (::trackers[this].context_switch_prefetch_gathered) 
    {
      //std::unordered_set<uint64_t> printed_page_addresses;
      
      /*
      for(auto [addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
        if (printed_page_addresses.find(addr >> 12) == printed_page_addresses.end()) {
          
          //std::cout << "Page with base address " << std::hex << addr << " issued at cycle " << std::dec << issued_at << " received at cycle " << received_at << std::endl; 
          printed_page_addresses.insert(addr >> 12);
        }
      }
      */

      if (!champsim::operable::have_cleared_BTB
          && !champsim::operable::have_cleared_BP
          && champsim::operable::cpu_side_reset_ready
          //&& !champsim::operable::have_cleared_prefetcher
          //&& champsim::operable::L2C_have_issued_context_switch_prefetches
          ) {//&& champsim::operable::cache_clear_counter == 7
        champsim::operable::context_switch_mode = false;
        champsim::operable::cpu_side_reset_ready = false;
        champsim::operable::cache_clear_counter = 0;
        ::trackers[this].context_switch_prefetch_gathered = false;
        std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycles" << " done at cycle " << current_cycle << std::endl;
        reset_misc::can_record_after_access = true;
        reset_misc::dq_after_data_access.clear();
        ::trackers[this].issued_context_switch_prefetches = 0;
      }
    }
  }
  else {
    //if (!::trackers[this].context_switch_queue_empty())
    if (!reset_misc::dq_prefetch_communicate.empty()) 
    {
      //if (champsim::operable::cpu_side_reset_ready) 
      if (::trackers[this].waited == RETRY_LIMIT) 
      {
       ::trackers[this].context_switch_issue(this);

        for(auto &[addr, issued_at, received_at] : ::trackers[this].context_switch_prefetching_timing) {
          if (received_at == 0) {
            for(auto var : block) {
              if (var.valid && var.address == addr) {
                received_at = this->current_cycle; 
              }
            } 
          } 
        }
         ::trackers[this].waited = 0;
      }
      else {
        ::trackers[this].waited++; 
      }
    }
  }
}

void CACHE::prefetcher_final_stats() {}

// WL 
