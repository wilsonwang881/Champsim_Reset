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
    constexpr static bool RECORD_OR_REPLAY = true;
    constexpr static uint64_t ACCESS_LEN = 88000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";

    std::fstream rec_file;

    public:

    constexpr static uint64_t PF_DEPTH = 2500;

    struct acc_timestamp {
      uint64_t cycle_diff;
      uint64_t addr;
    };

    bool can_write;
    bool first_round = true;
    std::deque<acc_timestamp> access;
    std::deque<acc_timestamp> progress_q;
    std::deque<acc_timestamp> cs_pf;
    uint64_t interval_start_cycle = 0;
    uint64_t cycles_speedup = 0;
    uint64_t allowed_pf;
    uint64_t pf_issued_last_round = 0;
    uint64_t pf_issued = 0;

    void init();
    void update(uint64_t cycle, uint64_t addr);
    void check_progress(uint64_t cycle, uint64_t addr);
    void file_write();
    void file_read();
    void finish();
  };
}

#endif 
