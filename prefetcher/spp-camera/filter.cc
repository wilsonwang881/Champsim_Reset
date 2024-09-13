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
std::vector<uint64_t> spp::SPP_PREFETCH_FILTER::gather_pf()
{
  std::vector<uint64_t> pf;

  std::vector<std::pair<std::size_t, uint16_t>> i_lru_vec;

  for(size_t i = 0; i < WAY * SET; i++) 
    i_lru_vec.push_back(std::make_pair(i, prefetch_table[i].last_used)); 

  std::sort(i_lru_vec.begin(), i_lru_vec.end(), [](auto &left, auto &right) {
      return left.second < right.second;
      });

  // Get the prefetches.
  for(auto var : i_lru_vec) {

    size_t i = var.first;

    uint64_t page_addr = prefetch_table[i].page_no << 12;

    for (size_t j = 0; j < 64; j++) {

      if (prefetch_table[i].used.test(j) ||
          prefetch_table[i].prefetched.test(j))
        pf.push_back(page_addr + (j << 6)); 
    } 
  }

  std::cout << "Gathered " << pf.size() << " prefetches from filter." << std::endl;

  return pf;
}

// WL
void spp::SPP_PREFETCH_FILTER::clear()
{
	for(size_t i = 0; i < WAY * SET; i++)
	{
		prefetch_table[i].page_no = 0;
		prefetch_table[i].last_used = 0;
		prefetch_table[i].prefetched.reset();
		prefetch_table[i].used.reset();
	}

	access_count = 0;
	tagless = false;
	repl_count = 0;
}
