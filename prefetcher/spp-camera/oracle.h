#ifndef ORACLE_H 
#define ORACLE_H

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
    constexpr static uint64_t ACCESS_LEN = 8000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";

    std::fstream rec_file;

    public:

    constexpr static bool ORACLE_ACTIVE = false;
    constexpr static bool RECORD_OR_REPLAY = false;

    struct acc_timestamp {
      uint64_t cycle_diff;
      uint64_t addr;
    };

    bool can_write;
    bool first_round = true;
    std::deque<acc_timestamp> access;
    uint64_t interval_start_cycle;
    uint64_t pf_issued_last_round = 0;
    uint64_t pf_issued = 0;

    void init();
    void update(uint64_t cycle, uint64_t addr);
    void file_write();
    std::vector<std::pair<uint64_t, bool>>file_read();
    void finish();
  };
}

#endif 
