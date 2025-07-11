#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <iomanip>
#include <deque>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include "champsim_constants.h"

namespace spp {
  class SPP_PAGE_BITMAP {
    constexpr static uint64_t TABLE_SET = 1;
    constexpr static uint64_t TABLE_WAY = 1024;
    constexpr static std::size_t TABLE_SIZE = TABLE_SET * TABLE_WAY;
    constexpr static std::size_t BITMAP_SIZE = 64;
    constexpr static uint64_t FILTER_WAY = 512;
    constexpr static std::size_t FILTER_SIZE = TABLE_SET * FILTER_WAY;
    constexpr static bool PAGE_BITMAP_DEBUG_PRINT = false;
    constexpr static bool RECORD_PAGE_ACCESS = true;
    constexpr static bool READ_PAGE_ACCESS = false;
    std::size_t FILTER_THRESHOLD = 10;
    std::string PAGE_BITMAP_ACCESS = "pb_acc.txt";
    std::fstream pb_file;

    struct PAGE_R {
      bool valid = false;
      uint16_t lru_bits;
      uint64_t page_no;
      uint64_t page_no_store;
      bool bitmap[BITMAP_SIZE] = {false};
      bool bitmap_store[BITMAP_SIZE] = {false};
      uint8_t row_counter[BITMAP_SIZE / 8] = {0};
      uint64_t acc_counter = 0;
    };

    struct PAGE_ACCESS {
      uint64_t total_access;
      uint64_t row_access[BITMAP_SIZE / 8];
      uint64_t col_access[BITMAP_SIZE / 8];
      std::array<bool, BITMAP_SIZE> block;
    };

    public:

    uint64_t pf_metadata = 0;
    uint64_t pf_metadata_limit = 35 * 1024;

    std::map<uint64_t, PAGE_ACCESS> last_round_pg_acc;
    std::map<uint64_t, PAGE_ACCESS> this_round_pg_acc;

    std::map<uint64_t, std::map<uint64_t, std::array<bool, BITMAP_SIZE>>> pb_acc;

    std::vector<PAGE_R> tb = std::vector<PAGE_R>(TABLE_SIZE);
    std::vector<PAGE_R> filter = std::vector<PAGE_R>(FILTER_SIZE);

    std::deque<std::pair<uint64_t, bool>> cs_pf;
    std::set<uint64_t> issued_cs_pf;
    uint64_t issued_cs_pf_hit;
    uint64_t total_issued_cs_pf;

    void pb_file_read();
    void pb_file_write(uint64_t asid);
    void lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way);
    void update(uint64_t addr);
    void evict(uint64_t addr);
    void update_bitmap_store();
    std::vector<std::pair<uint64_t, bool>> gather_pf(uint64_t asid);
    bool filter_operate(uint64_t addr);
    void update_usefulness(uint64_t addr);
    uint64_t calc_set(uint64_t addr);
    void print_page_access();
  };
}

