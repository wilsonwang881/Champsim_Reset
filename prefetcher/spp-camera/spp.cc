#include "spp.h"
#include "cache.h"

#include <array>
#include <iostream>
#include <map>
#include <numeric>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

namespace {
  template <typename R>
    void increment_at(R& range, std::size_t idx)
    {
      while (std::size(range) <= idx)
        range.push_back(0);
      range.at(idx)++;
    }

  // Signature calculation parameters
  constexpr std::size_t SIG_SHIFT = 3;
  constexpr std::size_t SIG_BIT = 12;
  constexpr std::size_t SIG_DELTA_BIT = 2;

  uint32_t generate_signature(uint32_t last_sig, int32_t delta)
  {
    if (delta == 0)
      return last_sig;

    // Sign-magnitude representation
    int32_t pos_delta = (delta < 0) ? -1*delta : delta;
    int32_t sig_delta = ((pos_delta << 1) & ~champsim::bitmask(SIG_DELTA_BIT+1)) | (pos_delta & champsim::bitmask(SIG_DELTA_BIT)) | (((delta < 0) ? 1 : 0) << SIG_DELTA_BIT);

    return ((last_sig << SIG_SHIFT) ^ sig_delta) & champsim::bitmask(SIG_BIT);
  }
}

void spp::prefetcher::issue(CACHE* cache)
{
  // Issue eligible outstanding prefetches
  if (!std::empty(issue_queue)) {
    auto [addr, priority] = issue_queue.front();

    // If this fails, the queue was full.
    bool prefetched = cache->prefetch_line(addr, priority, 0);
    if (prefetched) {
      filter.update_issue(addr, cache->get_set(addr));
      issue_queue.pop_front();
    }
  }
}

void spp::prefetcher::update_demand(uint64_t addr, uint32_t set)
{
  filter.update_demand(addr, set);
}

void spp::prefetcher::step_lookahead()
{
  pattern_table.global_accuracy = warmup ? 0.95 : 1.0 * filter.pf_useful/filter.pf_issued;

  // Operate the pattern table
  if (active_lookahead.has_value()) {
    auto current_page = active_lookahead->address >> LOG2_PAGE_SIZE;
    auto current_depth = active_lookahead->depth;
    auto pattable_result = pattern_table.lookahead_step(active_lookahead->sig, active_lookahead->confidence, active_lookahead->depth);

    std::optional<pfqueue_entry_t> next_step;
    if (pattable_result.has_value()) {
      auto [next_delta, next_conf] = pattable_result.value();
      next_step = {::generate_signature(active_lookahead->sig, next_delta), next_delta, active_lookahead->depth + 1, next_conf, active_lookahead->address + (next_delta << LOG2_BLOCK_SIZE)};
    }

    if (!next_step.has_value()) {
      //STATS
      if (!warmup) {
        ::increment_at(depth_ptmiss_tracker, current_depth);
      }
    } else if (next_step->confidence < pattern_table.fill_threshold) {
      // The path has become unconfident
      //STATS
      if (!warmup) {
        ::increment_at(depth_confidence_tracker, current_depth);
      }

      next_step.reset();
    } else if ((next_step->address >> LOG2_PAGE_SIZE) != current_page) {
      // Prefetch request is crossing the physical page boundary
      bootstrap_table.update(next_step->address, next_step->sig, next_step->confidence, next_step->delta);

      //STATS
      if (!warmup) {
        ::increment_at(depth_offpage_tracker, current_depth);
      }

      next_step.reset(); // TODO should this kill the lookahead?
    } else if (std::size(issue_queue) < ISSUE_QUEUE_SIZE) {
      // Check the filter
      if (auto filter_check_result = filter.check(next_step->address); filter_check_result != spp::REJECT) {
        issue_queue.push_back({next_step->address, filter_check_result == spp::STRONGLY_ACCEPT});

        //STATS
        if (!warmup)
          sig_count_tracker[next_step->sig]++;
      }
      // continue even if filter rejects
    } else {
      // If the queue has no capacity, try again next cycle
      next_step = active_lookahead;
    }

    active_lookahead = next_step;
  }
}

void spp::prefetcher::initiate_lookahead(uint64_t base_addr)
{
  int confidence = (1<<ACCURACY_BITS) - 1;

  // Stage 1: Read and update a sig stored in ST
  auto sig_and_delta = signature_table.read(base_addr);
  if (!sig_and_delta.has_value()) {
    if (auto bst_returnval = bootstrap_table.check(base_addr); bst_returnval.has_value()) {
      auto [sig, conf, del] = *bst_returnval;
      sig_and_delta = {sig, del};
      confidence = conf;
    }
  }
  auto [last_sig, delta] = sig_and_delta.value_or(std::pair{0,0});

  // Stage 2: Update delta patterns stored in PT
  // If we miss the ST and BST, we skip this update, because the pattern won't be present
  if (sig_and_delta.has_value()) pattern_table.update_pattern(last_sig, delta);

  auto curr_sig = ::generate_signature(last_sig, delta);
  signature_table.update(base_addr, curr_sig);

  // Stage 3: Start prefetching
  if (sig_and_delta.has_value()) {
    //STATS
    if (!warmup) {
      if (active_lookahead.has_value()) {
        ::increment_at(depth_interrupt_tracker, active_lookahead->depth);
      }
    }

    active_lookahead = {curr_sig, delta, 0, confidence, base_addr & ~champsim::bitmask(LOG2_BLOCK_SIZE)};
  }
}

void spp::prefetcher::print_stats(std::ostream& ostr)
{
  /*
   * Histogram of signature counts
   */
  std::vector<unsigned> counts, acc_counts;
  std::for_each(std::begin(sig_count_tracker), std::end(sig_count_tracker), [&counts](auto x){
      ::increment_at(counts, x.second);
      });

  auto total = std::accumulate(std::begin(counts), std::end(counts), 0u);
  ostr << "Signature histogram:" << std::endl;
  ostr << "  total: " << total << std::endl;

  auto partial_sum = 0;
  for (auto c_it = std::begin(counts); c_it != std::end(counts); ++c_it) {
    partial_sum += *c_it;
    if (partial_sum > 0.1*total) {
      ostr << std::distance(std::begin(counts), c_it) << "\t:\t" << partial_sum << std::endl;
      partial_sum = 0;
    }
  }
  ostr << std::endl;

  /*
   * List of depths
   */
  ostr << "DEPTH (confidence):" << std::endl;
  unsigned i = 0;
  for (auto val : depth_confidence_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (interrupt):" << std::endl;
  i = 0;
  for (auto val : depth_interrupt_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (offpage):" << std::endl;
  i = 0;
  for (auto val : depth_offpage_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;

  ostr << std::endl;
  ostr << "DEPTH (pattern miss):" << std::endl;
  i = 0;
  for (auto val : depth_ptmiss_tracker)
    ostr << i++ << "\t:\t" << val << std::endl;
}

// WL
void spp::prefetcher::clear_states()
{
  signature_table.clear();
  bootstrap_table.clear();
  pattern_table.clear();
  filter.clear();
  issue_queue.clear();
  active_lookahead.reset();
  //std::cout << "Cleared signature_table, bootstrap_table, pattern_table, filter, issue_queue, active_lookahead" << std::endl;
}

// WL
void spp::prefetcher::context_switch_gather_prefetches()
{
  // Gather unique page addresses.
  std::vector<uint64_t> uniq_page_address;

  for(auto var : reset_misc::before_reset_on_demand_ins_access) {
    uint64_t page_addr = var.ip >> 11;
    
    for (size_t i = 0; i < uniq_page_address.size(); i++) {
      if (uniq_page_address.at(i) == page_addr) {
        uniq_page_address.erase(uniq_page_address.begin() + i); 
      } 
    }

    uniq_page_address.push_back(page_addr);
  }

  int num_page_addr = uniq_page_address.size();

  if (num_page_addr > 12) {
    for (size_t i = 0; i < num_page_addr - 12; i++) {
      uniq_page_address.erase(uniq_page_address.begin());
  }
   
  }
  if (uniq_page_address.size() <= 12) {
    for(auto var : uniq_page_address) {
      for (size_t page_offset = 0; page_offset < (2048 - 64); page_offset += 64) {
        context_switch_issue_queue.push_back({(var + page_offset), true});
      }
    }

    std::cout << "Ready to issue prefetches for " << uniq_page_address.size() << " half page(s)" << std::endl;
  }
  else {
    std::cout << "Too many pages for context switch prefetching" << std::endl;
  }
  /*
  // Walk the signature table.
  for (size_t index = 0; index < SIGNATURE_TABLE::SET * SIGNATURE_TABLE::WAY; index++)
  {
    bool st_entry_valid = false;
    uint32_t el_last_offset = 0;
    uint32_t el_sig = 0;
    uint64_t el_last_accessed_page_num = 0;
    int el_page_offset_diff = 0;

    st_entry_valid = signature_table.get_st_entry(index, el_last_offset, el_sig, el_last_accessed_page_num, el_page_offset_diff);

    if (st_entry_valid)
    {
      
      // Use the signature and offset to index into the pattern table.
      auto pt_query_res = pattern_table.query_pt(el_sig, el_page_offset_diff);

      if (pt_query_res.has_value())
      {
	// Push to the context switch prefetch queue after checking in the filter.
	// If c_delta / c_sig >= 1/2
	//if (auto filter_check_result = filter.check((el_last_accessed_page_num << LOG2_PAGE_SIZE) + el_last_offset); filter_check_result != spp::REJECT) {
	if ((1.0 * pt_query_res->first / pt_query_res->second) >= 0.80) 
	  context_switch_issue_queue.push_back({(el_last_accessed_page_num << LOG2_PAGE_SIZE) + el_last_offset, true}); 
	else
	  context_switch_issue_queue.push_back({(el_last_accessed_page_num << LOG2_PAGE_SIZE) + el_last_offset, false}); 
      }
      

      int prefetch_range = pattern_table.get_prefetch_range(el_sig);
      int max_page_limit = std::min(4096 - 64, (signed)el_last_offset + prefetch_range);
      int min_page_limit = std::max(0, (signed)el_last_offset - prefetch_range);
      //cout << "Page coverage: " << 1.0 * (max_page_limit - min_page_limit) / 4096 << endl;

      for (int i = min_page_limit; i <= max_page_limit; i += BLOCK_SIZE)
      {
	context_switch_issue_queue.push_back({(el_last_accessed_page_num << LOG2_PAGE_SIZE) + i, false});
      }
    }
  }

  // Debug print.
  //printf("Addresses to be prefetched after a context switch\n");

  //std::deque<std::pair<uint64_t, bool>>::iterator it;

  
  for (it = context_switch_issue_queue.begin(); it != context_switch_issue_queue.end(); ++it)
  {
    printf("0x%012lx %d\n", it->first, it->second);
  }
  

  // WL 
  // For testing purposes, temporarily not issue any context switch prefetches.
  // context_switch_issue_queue.clear();
 */
}

// WL
bool spp::prefetcher::context_switch_queue_empty()
{
  return context_switch_issue_queue.empty();
}

// WL
void spp::prefetcher::context_switch_issue(CACHE* cache)
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

// WL 
void spp::prefetcher::record_spp_states()
{
  std::cout << "Recording BT, PT, ST" << std::endl;
  std::string bootstrap_table_content = bootstrap_table.record_Bootstrap_Table();
  std::string pattern_table_content = pattern_table.record_Pattern_Table();
  std::string signature_table_content = signature_table.record_Signature_Table();

  prefetcher_state_file << "=================================" << std::endl;
  prefetcher_state_file << "Current cycle = " << (unsigned)cache_cycle << std::endl;
  prefetcher_state_file << bootstrap_table_content;
  prefetcher_state_file << pattern_table_content;
  prefetcher_state_file << signature_table_content;
}
