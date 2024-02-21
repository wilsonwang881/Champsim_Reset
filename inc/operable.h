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

#include <string>
// WL

// WL 
namespace reset_misc {

  struct on_demand_access_record{
    uint64_t cycle;
    uint64_t ip;
  };

  extern on_demand_access_record on_demand_access_records[1000];
  extern size_t on_demand_access_record_index;

  extern on_demand_access_record before_reset_on_demand_access_records[1000];
  extern size_t before_reset_on_demand_access_record_index;
}
// WL

namespace champsim
{

class operable
{
public:
  const double CLOCK_SCALE;

  // WL
  static bool context_switch_mode;
  static bool have_recorded_on_demand_accesses;
  static bool have_recorded_before_reset_on_demand_accesses;
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

  static uint8_t currently_active_thread_ID;
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
