#include "spp.h"
#include "msl/bits.h"

#include <algorithm>

std::optional<std::pair<uint32_t, int32_t>> spp::SIGNATURE_TABLE::read(uint64_t addr)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_idx = page % SET;
  auto tag = (page / SET) & champsim::bitmask(TAG_BIT);

  auto set_begin = std::next(std::begin(sigtable), WAY * set_idx);
  auto set_end   = std::next(set_begin, WAY);

  // Try to find a hit in the set
  auto match_way = std::find_if(set_begin, set_end, [tag](const auto& x){ return x.valid && x.partial_page == tag; });

  if (match_way == set_end)
    return std::nullopt;

  match_way->last_used = ++access_count;
  match_way->last_accessed_page_num = page; // WL: recorded address
  return std::pair{match_way->sig, (signed)page_offset - (signed)match_way->last_offset};
}

void spp::SIGNATURE_TABLE::update(uint64_t addr, uint32_t sig)
{
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset = (addr & champsim::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;

  auto set_idx = page % SET;
  auto tag = (page / SET) & champsim::bitmask(TAG_BIT);

  auto set_begin = std::next(std::begin(sigtable), WAY * set_idx);
  auto set_end   = std::next(set_begin, WAY);

  // Try to find a hit in the set
  auto match_way = std::find_if(set_begin, set_end, [tag](const auto& x){ return x.valid && x.partial_page == tag; });

  if (match_way == set_end) {
    match_way = std::find_if_not(set_begin, set_end, [](const auto& x){ return x.valid; });
  }
  if (match_way == set_end) {
    match_way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.last_used < y.last_used; });
  }

  *match_way = {true, tag, page_offset, sig, ++access_count, page}; // WL: added the page number 
}

// WL
void spp::SIGNATURE_TABLE::clear()
{
	// bool toggle_valid = false;
	// bool toggle_partial_page = false;
	// bool toggle_last_offset = false;
	// bool toggle_sig = false;
	// bool toggle_last_used = false;

	for(size_t i = 0; i < WAY * SET; i++)
	{
		// if (sigtable[i].valid)
		// 	toggle_valid = true;
		// if (sigtable[i].partial_page != 0)
		// 	toggle_partial_page = true;
		// if (sigtable[i].last_offset != 0)
		// 	toggle_last_offset = true;
		// if (sigtable[i].sig != 0)
		// 	toggle_sig = true;
		// if (sigtable[i].last_used != 0)
		// 	toggle_last_used = true;

		sigtable[i].valid = false;
		sigtable[i].partial_page = 0;
		sigtable[i].last_offset = 0;
		sigtable[i].sig = 0;
		sigtable[i].last_used = 0;
		sigtable[i].last_accessed_page_num = 0;
	}

	/*
	std::cout << "Cleared SIGNATURE_TABLE " \
		<< toggle_valid << " " << toggle_partial_page << " " \
		<< toggle_last_offset << " " << toggle_sig << " " \
		<< toggle_last_used << " " << access_count << std::endl;
	*/

	access_count = 0;
}

// WL
bool spp::SIGNATURE_TABLE::get_st_entry(int index, uint32_t &el_last_offset, uint32_t &el_sig, uint64_t &el_last_accessed_page_num, int &el_page_offset_diff)
{
  el_last_offset = sigtable[index].last_offset;
  el_sig = sigtable[index].sig;
  el_last_accessed_page_num = sigtable[index].last_accessed_page_num;
  //el_page_offset_diff = sigtable[index].page_offset_diff;

  return sigtable[index].valid;
}

// WL
std::string spp::SIGNATURE_TABLE::record_Signature_Table()
{
  std::string content("Signature Table\n");

  for(auto var : sigtable) {
    content = content + (var.valid ? "1" : "0") + " " + \
              std::to_string(var.partial_page) + " " + \
              std::to_string(var.last_offset) + " " + \
              std::to_string(var.sig) + " " + \
              std::to_string(var.last_used) + " " + \
              std::to_string(var.last_accessed_page_num) + "\n";
  }

  return content;
}
