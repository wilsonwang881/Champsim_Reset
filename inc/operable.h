/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPERABLE_H
#define OPERABLE_H

// WL
#define SIMULATE_WITH_CACHE_RESET 0
#define SIMULATE_WITH_TLB_RESET 0
#define SIMULATE_WITH_L1_RESET 0
#define SIMULATE_WITH_L2_RESET 0
#define SIMULATE_WITH_LLC_RESET 0
#define SIMULATE_WITH_PREFETCHER_RESET 0
#define SIMULATE_WITH_BTB_RESET 0
#define SIMULATE_WITH_BRANCH_PREDICTOR_RESET 0
#define RESET_INTERVAL 4000000
#define ON_DEMAND_ACCESS_RECORD_SIZE 1000
#define DEQUE_ON_DEMAND_ACCESS_RECORD_SIZE 4000

#include <string>
#include <deque>
#include <unordered_set>
// WL

// WL 
namespace reset_misc {

  struct on_demand_ins_access {
    uint64_t cycle;
    uint64_t ip;
    uint64_t occurance;
  };

  struct addr_occr {
    uint64_t addr;
    uint64_t occr;
    uint64_t cycle;
  };
  
  struct on_demand_data_access : on_demand_ins_access {
    bool load_or_store; 
    std::vector<addr_occr> addr_rec;
    std::unordered_set<uint64_t> addr;
  };

  extern on_demand_ins_access before_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  extern size_t before_reset_on_demand_ins_access_index;

  extern on_demand_ins_access after_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  extern size_t after_reset_on_demand_ins_access_index;

  extern on_demand_data_access before_reset_on_demand_data_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  extern size_t before_reset_on_demand_data_access_index;

  extern on_demand_data_access after_reset_on_demand_data_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  extern size_t after_reset_on_demand_data_access_index;

  extern std::deque<on_demand_ins_access> dq_before_ins_access;
  extern std::deque<on_demand_ins_access> dq_after_ins_access;
  extern std::deque<on_demand_data_access> dq_before_data_access;
  extern std::deque<on_demand_data_access> dq_after_data_access;
  extern std::deque<on_demand_data_access> dq_pf_data_access;
  extern std::deque<addr_occr> dq_before_knn;
  extern std::deque<addr_occr> dq_after_knn;
  extern bool can_record_after_access;
}
// WL

namespace champsim
{

class operable
{
public:
  const double CLOCK_SCALE;
  //knn_predict
  static bool knn_can_predict;
  static uint64_t knn_accuracy;
  static uint64_t reset_count;
  // WL
  static bool context_switch_mode;
  static bool L2C_have_issued_context_switch_prefetches;
  static bool have_recorded_on_demand_ins_accesses;
  static bool have_recorded_on_demand_data_accesses;
  static bool have_recorded_before_reset_on_demand_ins_accesses;
  static bool have_recorded_before_reset_on_demand_data_accesses;
  static bool have_recorded_before_reset_hit_miss_number_L1I;
  static bool have_recorded_before_reset_hit_miss_number_L1D;
  static bool have_recorded_before_reset_hit_miss_number_L2C;
  static bool have_recorded_before_reset_hit_miss_number_LLC;
  static bool have_recorded_after_reset_hit_miss_number_L1I;
  static bool have_recorded_after_reset_hit_miss_number_L1D;
  static bool have_recorded_after_reset_hit_miss_number_L2C;
  static bool have_recorded_after_reset_hit_miss_number_LLC;
  static bool have_recorded_prefetcher_states;
  static bool have_recorded_L1I_states;
  static bool have_recorded_L1D_states;
  static uint64_t context_switch_start_cycle;
  static bool have_cleared_L1I;
  static bool have_cleared_L1D;
  static bool have_cleared_L2C;
  static bool have_cleared_LLC;
  static bool have_cleared_ITLB;
  static bool have_cleared_DTLB;
  static bool have_cleared_STLB;
  static bool have_cleared_BTB;
  static bool have_cleared_BP;
  static uint64_t cache_clear_counter;

  static bool have_cleaned_L1I;
  static bool have_cleaned_L1D;
  static bool have_cleaned_L2C;
  static bool have_cleaned_LLC;
  static bool have_cleaned_ITLB;
  static bool have_cleaned_DTLB;
  static bool have_cleaned_STLB;

  const std::string L1I_name = "cpu0_L1I";
  const std::string L1D_name = "cpu0_L1D";
  const std::string L2C_name = "cpu0_L2C";
  const std::string LLC_name = "LLC";
  const std::string DTLB_name = "cpu0_DTLB";
  const std::string ITLB_name = "cpu0_ITLB";
  const std::string STLB_name = "cpu0_STLB";

  static bool have_cleared_prefetcher;

  static bool cpu_side_reset_ready;

  static uint16_t currently_active_thread_ID;

  static std::vector<uint64_t> reset_ins_count_global;

  static uint64_t number_of_instructions_to_skip_before_log;

  static uint64_t cpu0_num_retired;
  // WL

  double leap_operation = 0;
  uint64_t current_cycle = 0;
  bool warmup = true;

  explicit operable(double scale) : CLOCK_SCALE(scale - 1) {}

  long _operate()
  {
    // skip periodically
    if (leap_operation >= 1) {
      leap_operation -= 1;
      return 0;
    }

    auto result = operate();

    leap_operation += CLOCK_SCALE;
    ++current_cycle;

    return result;
  }

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual long operate() = 0;
  virtual void begin_phase() {}       // LCOV_EXCL_LINE
  virtual void end_phase(unsigned) {} // LCOV_EXCL_LINE
  virtual void print_deadlock() {}    // LCOV_EXCL_LINE
};

} // namespace champsim

#endif
