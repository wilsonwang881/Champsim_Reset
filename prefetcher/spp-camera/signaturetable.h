#include <array>
#include <cstdint>
#include <optional>
#include <tuple>

namespace spp
{
  class SIGNATURE_TABLE
  {
    public:
      static constexpr std::size_t SET = 1 << 6;
      static constexpr std::size_t WAY = 4;
      static constexpr std::size_t TAG_BIT = 16;

    private:
      struct sigtable_entry_t
      {
        bool     valid = false;
        uint64_t partial_page = 0;
        uint32_t last_offset = 0;
        uint32_t sig = 0;
        uint64_t last_used = 0;
	uint64_t last_accessed_page_num = 0; // WL: added the page number
      };

      std::array<sigtable_entry_t, WAY * SET> sigtable;

      uint64_t access_count = 0;

    public:
      std::optional<std::pair<uint32_t, int32_t>> read(uint64_t addr);
      void update(uint64_t base_addr, uint32_t sig);    
      void clear(); // WL  
      bool get_st_entry(int index, uint32_t &el_last_offet, uint32_t &el_sig, uint64_t &el_last_accessed_page_num, int &el_page_offset_diff);
  };
}


