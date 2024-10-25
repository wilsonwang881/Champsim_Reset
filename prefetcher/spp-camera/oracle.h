#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <deque>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>

namespace spp 
{
  class SPP_ORACLE
  {
    constexpr static bool RECORD_OR_REPLAY = true;
    constexpr static uint64_t ACCESS_LEN = 1000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";

    std::fstream rec_file;
    std::set<uint64_t> dup_check;

    public:

    std::deque<uint64_t> access;

    void init();
    void update(uint64_t addr);
    void file_write();
    std::vector<std::pair<uint64_t, bool>> file_read();
    void finish();
  };
}

