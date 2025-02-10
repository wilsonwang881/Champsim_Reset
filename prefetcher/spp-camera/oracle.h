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
#include <unordered_set>
#include <algorithm>
#include "champsim.h"
#include "champsim_constants.h"

namespace spp {

  class SPP_ORACLE {
    constexpr static uint64_t ACCESS_LEN = 100000000;
    std::string L2C_PHY_ACC_FILE_NAME = "L2C_phy_acc.txt";
    std::string L2C_PHY_ACC_WRITE_FILE_NAME = "L2C_phy_acc_write.txt";

    std::fstream rec_file;
    //std::fstream rec_file_write;

    public:

    std::map<uint64_t, uint64_t> set_availability;

    constexpr static bool ORACLE_ACTIVE = true;
    constexpr static bool RECORD_OR_REPLAY = false;

    uint64_t lru_counter;

    struct acc_timestamp {
      uint64_t cycle_demanded;
      uint64_t addr;
      uint64_t miss_or_hit;
      bool require_eviction;
    };

    bool can_write;
    bool first_round;
    std::vector<acc_timestamp> access;
    uint64_t interval_start_cycle;
    uint64_t pf_issued_last_round = 0;
    uint64_t pf_issued = 0;
    const static uint64_t SET_NUM = 1024;
    const static uint64_t WAY_NUM = 8;
    uint64_t available_pf = SET_NUM * WAY_NUM;
    uint64_t hit_address = 0;
    uint64_t initial_fill = SET_NUM * WAY_NUM;
    uint64_t previous_miss_addr;

    struct blk_state {
      uint64_t addr;
      int pending_accesses;
      uint64_t timestamp;
      bool require_eviction;
    };

    blk_state cache_state[SET_NUM * WAY_NUM];
    uint8_t set_kill_counter[SET_NUM * WAY_NUM];
    std::deque<acc_timestamp> oracle_pf;
    std::deque<acc_timestamp> readin;
    uint64_t readin_index = 0;

    void init();
    uint64_t update_demand(uint64_t cycle, uint64_t addr, bool hit, bool prefetch);
    void refresh_cache_state();
    void file_write();
    void file_read();
    uint64_t check_set_pf_avail(uint64_t addr);
    int check_pf_status(uint64_t addr);
    int update_pf_avail(uint64_t addr, uint64_t cycle);
    bool check_require_eviction(uint64_t addr);
    uint64_t poll(uint64_t addr);
    void kill_simulation(uint64_t cycle, uint64_t addr, bool hit);
    void finish();
  };
}

#endif 
