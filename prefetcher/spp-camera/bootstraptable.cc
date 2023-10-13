#include "bootstraptable.h"

#include <algorithm>
#include <tuple>

#include "champsim_constants.h"

std::optional<std::tuple<uint32_t, int, int>> spp::BOOTSTRAP_TABLE::check(uint64_t addr)
{
  uint32_t page_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  // Bootstrap the new page
  auto bst_item = std::find_if(std::begin(page_bootstrap_table), std::end(page_bootstrap_table), [page_offset](const auto& x){ return x.valid && x.offset == page_offset; });
  if (bst_item != std::end(page_bootstrap_table)) {
    bst_item->valid = false;
    return std::tuple{bst_item->sig, bst_item->confidence, bst_item->delta};
  } else {
    return std::nullopt;
  }
}

void spp::BOOTSTRAP_TABLE::update(uint64_t addr, uint32_t sig, int confidence, int delta)
{
  // Find the item in the bootstrap table
  auto begin = std::begin(page_bootstrap_table);
  auto end = std::end(page_bootstrap_table);
  auto pf_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;
  auto bst_item = std::find_if(begin, end, [pf_offset](const auto& x){ return x.valid && x.offset == pf_offset; });

  // If not found, find an invalid or lowest-confidence way to replace
  if (bst_item == end)
    bst_item = std::find_if_not(begin, end, [](auto x){ return x.valid; });
  if (bst_item == end)
    bst_item = std::min_element(begin, end, [](auto x, auto y){ return x.confidence < y.confidence; });

  // Place the information in the bootstrap table
  *bst_item = {true, sig, confidence, pf_offset, delta, addr}; // WL: added the last accessed address
}

// WL
void spp::BOOTSTRAP_TABLE::clear()
{
	// bool toggled_valid = false;
	// bool toggled_sig = false;
	// bool toggled_confidence = false;
	// bool toggled_offset = false;
	// bool toggled_delta = false;
	
	for(size_t i = 0; i < MAX_GHR_ENTRY; i++)
	{
		// if (page_bootstrap_table[i].valid)
		// 	toggled_valid = true;
		// if (page_bootstrap_table[i].sig != 0)
		// 	toggled_sig = true;
		// if (page_bootstrap_table[i].confidence != 0)
		// 	toggled_confidence = true;
		// if (page_bootstrap_table[i].offset != 0)
		// 	toggled_offset = true;
		// if (page_bootstrap_table[i].delta != 0)
		// 	toggled_delta = true;

		page_bootstrap_table[i].valid = false;
		page_bootstrap_table[i].sig = 0;
		page_bootstrap_table[i].confidence = 0;
		page_bootstrap_table[i].offset = 0;
		page_bootstrap_table[i].delta = 0;
	}

	/*
	std::cout << "Cleared BOOTSTRAP_TABLE " << toggled_valid << " " << toggled_sig << " " \
		<< toggled_confidence << " " << toggled_offset << " " \
		<< toggled_delta << std::endl;
	*/
}
