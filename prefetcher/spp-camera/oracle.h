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
#include <cassert>
#include "champsim.h"
#include "champsim_constants.h"

namespace spp
{

  class SPP_ORACLE
  {
    constexpr static uint64_t ACCESS_LEN = 10000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";

    std::fstream rec_file;

    public:

    constexpr static bool ORACLE_ACTIVE = true;
    constexpr static bool RECORD_OR_REPLAY = true;

    struct acc_timestamp 
    {
      uint64_t cycle_demanded;
      uint64_t addr;
      uint64_t miss_or_hit;
    };

    bool can_write;
    bool first_round;
    std::deque<acc_timestamp> access;
    uint64_t interval_start_cycle;
    uint64_t pf_issued_last_round = 0;
    uint64_t pf_issued = 0;
    const static uint64_t SET_NUM = 1024;
    const static uint64_t WAY_NUM = 8;
    int available_pf = 1024 * 8;

    struct blk_state 
    {
      uint64_t addr;
      int pending_accesses;
      uint64_t timestamp;
    };

    blk_state cache_state[SET_NUM * WAY_NUM];
    std::deque<acc_timestamp> oracle_pf;

    void init();
    void update_demand(uint64_t cycle, uint64_t addr, bool hit);
    void create_new_entry(uint64_t addr, uint64_t cycle, bool& success, uint64_t& evict_addr);
    void update_fill(uint64_t addr);
    void refresh_cache_state();
    void file_write();
    void file_read();
    uint64_t check_set_pf_avail(uint64_t addr);
    int check_pf_status(uint64_t addr);
    int update_pf_avail(uint64_t addr, uint64_t cycle);
    uint64_t poll(uint64_t cycle);
    void finish();
  };
}

#endif 
