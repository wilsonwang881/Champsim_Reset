#include "oracle.h"


void oracle::SPP_ORACLE::init() {

  // Clear the access file if in recording mode.
  if (RECORD_OR_REPLAY) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++) {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0;
    cache_state[i].timestamp = 0;
    cache_state[i].require_eviction = true;
  }

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i] = WAY_NUM;
  }

  not_ready_queue = std::vector<std::deque<acc_timestamp>>(SET_NUM);

  file_read();
}