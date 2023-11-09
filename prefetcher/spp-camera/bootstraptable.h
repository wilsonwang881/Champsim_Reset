#include <array>
#include <cstdint>
#include <optional>

// WL
#include <iostream>

namespace spp
{
  class BOOTSTRAP_TABLE
  {
    static constexpr std::size_t MAX_GHR_ENTRY = 8;
    struct ghr_entry_t
    {
      bool     valid = false;
      uint32_t sig = 0;
      int      confidence = 0;
      uint64_t offset = 0; // WL: changed uint32_t to uint64_t
      int      delta = 0;
      uint64_t last_accessed_address = 0; // WL
    };

    // Global History Register (GHR) entries
    std::array<ghr_entry_t, MAX_GHR_ENTRY> page_bootstrap_table;

    public:
    void update(uint64_t addr, uint32_t sig, int confidence, int delta);
    std::optional<std::tuple<uint32_t, int, int>> check(uint64_t addr);

    // WL 
    void clear();
    std::string record_Bootstrap_Table();
    // WL 
  };
}

