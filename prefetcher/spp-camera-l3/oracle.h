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

namespace spp_l3 {

  class SPP_ORACLE {
    constexpr static uint64_t ACCESS_LEN = 100000;
    constexpr static bool ORACLE_DEBUG_PRINT = false;
    constexpr static bool BELADY_CACHE_REPLACEMENT_POLICY_ACTIVE = true;
    std::string L2C_PHY_ACC_FILE_NAME = "L3C_phy_acc.txt";
    std::fstream rec_file;

    public:

    const static int SET_NUM = 2048;
    const static int WAY_NUM = 10;
    bool ORACLE_ACTIVE = true;
    bool RECORD_OR_REPLAY = false;
    bool done;
    uint64_t new_misses = 0;
    uint64_t runahead_hits = 0;
    std::set<uint64_t> heartbeat_printed;
    std::map<int, int> set_availability;
    uint64_t MSHR_hits = 0;
    uint64_t internal_PQ_hits = 0;
    uint64_t cs_q_hits = 0;
    uint64_t oracle_pf_hits = 0;
    uint64_t unhandled_misses_replaced = 0;
    uint64_t unhandled_misses_not_found = 0;

#pragma pack(push,1)
    struct acc_timestamp {
      uint64_t cycle_demanded;
      uint16_t set;
      uint64_t addr;
      uint32_t miss_or_hit;
      bool require_eviction;
      uint8_t type;
      uint64_t reuse_distance;
    };
#pragma pack(pop)

    std::vector<acc_timestamp> access;
    uint64_t interval_start_cycle;
    uint64_t pf_issued_last_round = 0;
    uint64_t pf_issued = 0;

    struct blk_state {
      uint64_t addr;
      int pending_accesses;
      uint64_t timestamp;
      bool require_eviction;
      uint8_t type;
      bool accessed;
    };

    blk_state cache_state[SET_NUM * WAY_NUM];
    std::array<std::set<uint64_t>, SET_NUM> set_kill_counter;
    std::deque<acc_timestamp> oracle_pf;

    void init();
    void update_demand(uint64_t cycle, uint64_t addr, bool hit, bool prefetch, uint64_t type);
    void file_write();
    void file_read();
    uint64_t check_set_pf_avail(uint64_t addr);
    int check_pf_status(uint64_t addr);
    int update_pf_avail(uint64_t addr, uint64_t cycle);
    bool check_require_eviction(uint64_t addr);
    std::tuple<uint64_t, uint64_t, bool, bool> poll();
    uint64_t rollback_prefetch(uint64_t addr);
    void clear_addr(uint64_t addr);
    void kill_simulation();
    void finish();
  };
}

#endif 
