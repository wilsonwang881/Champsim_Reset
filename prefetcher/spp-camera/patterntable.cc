#include "spp.h"

void spp::PATTERN_TABLE::update_pattern(uint32_t last_sig, int curr_delta)
{
  auto set_begin = std::begin(pattable[last_sig % SET].ways);
  auto set_end   = std::end(pattable[last_sig % SET].ways);
  auto &c_sig    = pattable[last_sig % SET].c_sig;

  constexpr auto c_sig_max = (1u << C_SIG_BIT)-1;
  constexpr auto c_delta_max = (1u << C_DELTA_BIT)-1;

  // If the signature counter overflows, divide all counters by 2
  if (c_sig >= c_sig_max || std::any_of(set_begin, set_end, [c_delta_max](auto x){ return x.c_delta >= c_delta_max; })) {
    for (auto it = set_begin; it != set_end; ++it)
      it->c_delta /= 2;
    c_sig /= 2;
  }

  // Check for a hit
  if (auto way = std::find_if(set_begin, set_end, [curr_delta](auto x){ return x.valid && (x.delta == curr_delta); }); way != set_end) {
    way->c_delta++;
  } else {
    // Find an invalid way
    way = std::find_if_not(set_begin, set_end, [](auto x){ return x.valid; });
    if (way == set_end)
      way = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.c_delta < y.c_delta; }); // Find the smallest delta counter
    *way = pattable_entry_t(curr_delta);
  }

  c_sig++;
}

int mult_conf(double alpha, int lastconf, unsigned c_delta, unsigned c_sig, unsigned depth)
{
  return alpha * lastconf * c_delta / (c_sig + depth);
}

std::optional<std::pair<int, int>> spp::PATTERN_TABLE::lookahead_step(uint32_t sig, int confidence, uint32_t depth)
{
  auto set_begin = std::cbegin(pattable[sig % SET].ways);
  auto set_end   = std::cend(pattable[sig % SET].ways);
  auto c_sig     = pattable[sig % SET].c_sig;

  // Abort if the signature count is zero
  if (c_sig == 0)
    return std::nullopt;

  auto max_conf_way = std::max_element(set_begin, set_end, [](auto x, auto y){ return !x.valid || (y.valid && x.c_delta < y.c_delta); });

  if (!max_conf_way->valid)
    return std::nullopt;

  return std::pair{max_conf_way->delta, mult_conf(global_accuracy, confidence, max_conf_way->c_delta, c_sig, depth)};
}

// WL
void spp::PATTERN_TABLE::clear()
{
	// bool toggle_valid = false;
	// bool toggle_delta = false;
	// bool toggle_c_delta = false;
	// bool toggle_c_sig = false;

	for(size_t i = 0; i < SET; i++)
	{
		for(size_t j = 0; j < WAY; j++)
		{
			// if (pattable[i].ways[j].valid)
			// 	toggle_valid = true;
			// if (pattable[i].ways[j].delta != 0)
			// 	toggle_delta = true;
			// if (pattable[i].ways[j].c_delta != 1)
			// 	toggle_c_delta = true;

			pattable[i].ways[j].valid = false;
			pattable[i].ways[j].delta = 0;
			pattable[i].ways[j].c_delta = 1;
		}

		// if (pattable[i].c_sig != 0)
		// 	toggle_c_sig = true;

		pattable[i].c_sig = 0;
	}

	global_accuracy = 0.9;
	fill_threshold = 25;

  /*
	std::cout << "Cleared PATTERN_TABLE " \
		<< toggle_valid << " " << toggle_delta << " " \
		<< toggle_c_delta << " " << toggle_c_sig << \
		std::endl;
  */
}

// WL
std::optional<int> spp::PATTERN_TABLE::query_pt(uint32_t sig, unsigned int &_c_delta, unsigned int &_c_sig)
{
  auto set_begin = std::begin(pattable[sig % SET].ways);
  auto set_end   = std::end(pattable[sig % SET].ways);
  auto &c_sig    = pattable[sig % SET].c_sig;

  // Look for the max delta.
  auto way = std::max_element(set_begin, set_end, [](auto x, auto y){ return (y.valid && x.c_delta < y.c_delta); });

#if DEBUG_PRINT_PATTERN_TABLE
  std::cout << "==================" << std::endl;

  for (auto i = set_begin; i != set_end; ++i) 
    std::cout << i->valid << std::setw(4) << i->delta << std::setw(4) << (unsigned)i->c_delta << std::setw(4) << (unsigned)c_sig << std::endl;
  
  std::cout << "==================" << std::endl;

#endif // DEBUG
  
  if (way != set_end && way->valid) {
    _c_delta = way->c_delta;
    _c_sig = c_sig;
    return way->delta;
  }

#if DEBUG_PRINT_PATTERN_TABLE
  std::cout << "!!!" << std::endl;
#endif

  return std::nullopt;
}

// WL
int spp::PATTERN_TABLE::get_prefetch_range(uint32_t sig)
{
  auto set_begin = std::begin(pattable[sig % SET].ways);
  auto set_end   = std::end(pattable[sig % SET].ways);
  //auto &c_sig    = pattable[sig % SET].c_sig;

  // Check for a hit
  auto max_delta_way = std::max_element(set_begin, set_end, [](auto x, auto y){ return !x.valid || (y.valid && (abs(x.delta) < abs(y.delta))); });

  return 4 * abs(max_delta_way->delta); 
}

// WL
std::string spp::PATTERN_TABLE::record_Pattern_Table()
{
  std::string content("Pattern Table\n");

  for(auto pattern_table_set : pattable) {

    content = content + std::to_string(pattern_table_set.c_sig) + "\n";

    for(auto pattern_table_entry : pattern_table_set.ways)
    {
      content = content + (pattern_table_entry.valid ? "1" : "0") + " " + \
                std::to_string(pattern_table_entry.delta) + " " + \
                std::to_string(pattern_table_entry.c_delta) + "\n";
    }
  }

  return content;
}
