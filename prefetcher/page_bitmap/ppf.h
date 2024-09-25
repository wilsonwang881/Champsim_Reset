#include <cstdint>

namespace page_bitmap 
{
  class PAGE_BITMAP_PPF
  {
    public:

    constexpr static std::size_t TAG_COUNTER_SIZE = 1024;
    constexpr static std::size_t RJ_PF_SIZE = 1024;

    enum PPF_TB 
    {
      REJECTION_TABLE,
      PREFETCH_TABLE
    };

    private:

    // Counter for each block access.
    struct tag_counter_r
    {
      bool valid;
      uint8_t counter;
    };
      
    // Direcctly mapped table.
    tag_counter_r ct[TAG_COUNTER_SIZE];
    tag_counter_r upt[TAG_COUNTER_SIZE];

    struct rj_pf_r
    {
      bool valid;
      uint16_t tag;
    };

    public: 

    // Directly mapped table.
    rj_pf_r rj_tb[RJ_PF_SIZE];
    rj_pf_r pf_tb[RJ_PF_SIZE];

    void init();
    uint8_t saturating_counter(uint8_t val, bool increment);
    void tag_counter_update(uint64_t addr, bool useful);
    bool tag_counter_check(uint64_t addr);
    void invalidate_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
    void update_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
    bool check_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr);
  };
}

