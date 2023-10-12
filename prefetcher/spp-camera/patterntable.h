#include <array>
#include <cstdint>
#include <optional>
#include <tuple>

// WL
#include <iostream>

namespace spp
{
  class PATTERN_TABLE
  {
    public:
      static constexpr std::size_t SET = 1 << 10;
      static constexpr std::size_t WAY = 4;
      static constexpr std::size_t C_SIG_BIT = 4;
      static constexpr std::size_t C_DELTA_BIT = 4;

    private:
      struct pattable_entry_t
      {
        bool valid;
        int delta;
        unsigned int c_delta;

        pattable_entry_t()          : valid(false), delta(0),     c_delta(1) {}
        explicit pattable_entry_t(int delta) : valid(true),  delta(delta), c_delta(1) {}
      };

      struct pattable_set_t
      {
        std::array<pattable_entry_t, WAY> ways;
        unsigned int c_sig = 0;
      };

      std::array<pattable_set_t, SET> pattable;

    public:
      double global_accuracy = 0.9;
      int fill_threshold = 25;

      void update_pattern(uint32_t last_sig, int curr_delta);
      std::optional<std::pair<int, int>> lookahead_step(uint32_t sig, int confidence, uint32_t depth);
      void clear(); // WL
      std::optional<std::pair<unsigned int, unsigned int>> query_pt(uint32_t sig, int delta); // WL
      int get_prefetch_range(uint32_t sig); // WL
  };
}
