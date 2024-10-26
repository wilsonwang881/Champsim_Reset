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

class CACHE;

namespace oracle 
{

  class prefetcher
  {
    constexpr static bool RECORD_OR_REPLAY = false;
    constexpr static uint64_t ACCESS_LEN = 1000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";

    std::fstream rec_file;
    std::set<uint64_t> dup_check;

    public:

    bool can_write = false;
    std::deque<uint64_t> access;
    std::deque<uint64_t> cs_pf;

    void init();
    void update(uint64_t addr);
    void file_write();
    void file_read();
    void finish();
  };
}

#endif 
