#include "filter.h"

#include <algorithm>
#include <cassert>

#include "msl/bits.h"

auto spp::SPP_PREFETCH_FILTER::check(uint64_t check_addr, int confidence) const -> confidence_t
{
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way != set_end && match_way->prefetched.test(line_no))
    return REJECT;

  return confidence >= highconf_threshold ? STRONGLY_ACCEPT : WEAKLY_ACCEPT;
}

void spp::SPP_PREFETCH_FILTER::update_demand(uint64_t check_addr, std::size_t)
{
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way != set_end)
  {
    if (match_way->prefetched.test(line_no) && !match_way->used[line_no])
      pf_useful++;

    if (pf_useful >= (1u << spp::GLOBAL_COUNTER_BITS)) {
      pf_useful /= 2;
      pf_issued /= 2;
    }

    match_way->used.set(line_no);

    // Update LRU
    match_way->last_used = ++access_count;
  }
}

void spp::SPP_PREFETCH_FILTER::update_issue(uint64_t check_addr, std::size_t)
{
  uint64_t page_no = check_addr >> LOG2_PAGE_SIZE;
  uint64_t line_no = (check_addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_begin = std::next(std::begin(prefetch_table), WAY * (page_no % SET));
  auto set_end   = std::next(set_begin, WAY);
  auto match_way = std::find_if(set_begin, set_end, [page_no](const auto& x){ return x.prefetched.any() && x.page_no == page_no; });

  if (match_way == set_end)
  {
    match_way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.last_used < y.last_used; });
    match_way->prefetched.reset();
    match_way->used.reset();
  }

  assert(line_no < match_way->prefetched.size());
  match_way->page_no = page_no;
  match_way->prefetched.set(line_no);
  match_way->used.reset(line_no);

  // Update LRU
  match_way->last_used = ++access_count;

  pf_issued++;

  if (pf_issued >= (1u << spp::GLOBAL_COUNTER_BITS)) {
    pf_useful /= 2;
    pf_issued /= 2;
  }
}

// WL
void spp::SPP_PREFETCH_FILTER::clear()
{
	// bool toggle_page_no = false;
	// bool toggle_last_used = false;
	// bool toggle_prefetched = false;
	// bool toggle_used = false;

	for(size_t i = 0; i < WAY * SET; i++)
	{
		// if (prefetch_table[i].page_no != 0)
		// 	toggle_page_no = true;
		// if (prefetch_table[i].last_used != 0)
		// 	toggle_last_used = true;
		// if (prefetch_table[i].prefetched.any())
		// 	toggle_prefetched = true;
		// if (prefetch_table[i].used.any())
		// 	toggle_used = true;

		prefetch_table[i].page_no = 0;
		prefetch_table[i].last_used = 0;
		prefetch_table[i].prefetched.reset();
		prefetch_table[i].used.reset();
	}

	/*
	std::cout << "Cleared SPP_PREFETCH_FILTER " << \
		toggle_page_no << " " << toggle_last_used << " " \
		<< toggle_prefetched << " " << toggle_used << " " << access_count << " " << tagless << " " << pf_useful << " " << pf_issued << " " << repl_count << std::endl;
  */

	access_count = 0;
	tagless = false;
	repl_count = 0;

}
