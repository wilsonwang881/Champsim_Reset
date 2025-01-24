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

#include <algorithm>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "core_inst.inc"
#include "phase_info.h"
#include "stats_printer.h"
#include "tracereader.h"
#include "vmem.h"
#include <CLI/CLI.hpp>
#include <fmt/core.h>

#include <iostream> // WL
 
namespace champsim
{
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces, std::vector<uint64_t>& reset_ins_count);
}
//KNN predict
bool champsim::operable::knn_can_predict = false;
//uint64_t champsim::operable::knn_accuracy=0;
uint64_t champsim::operable::reset_count=0;
// WL
bool champsim::operable::context_switch_mode = false;
bool champsim::operable::L2C_have_issued_context_switch_prefetches = false;
bool champsim::operable::have_recorded_on_demand_ins_accesses = false;
bool champsim::operable::have_recorded_on_demand_data_accesses = false;
bool champsim::operable::have_recorded_before_reset_on_demand_ins_accesses = false;
bool champsim::operable::have_recorded_before_reset_on_demand_data_accesses = false;
bool champsim::operable::have_recorded_before_reset_hit_miss_number_L1I = false; 
bool champsim::operable::have_recorded_before_reset_hit_miss_number_L1D = false; 
bool champsim::operable::have_recorded_before_reset_hit_miss_number_L2C = false; 
bool champsim::operable::have_recorded_before_reset_hit_miss_number_LLC = false; 
bool champsim::operable::have_recorded_after_reset_hit_miss_number_L1I = false;
bool champsim::operable::have_recorded_after_reset_hit_miss_number_L1D = false;
bool champsim::operable::have_recorded_after_reset_hit_miss_number_L2C = false;
bool champsim::operable::have_recorded_after_reset_hit_miss_number_LLC = false;
bool champsim::operable::have_recorded_prefetcher_states = false;
bool champsim::operable::have_recorded_L1I_states = false;
bool champsim::operable::have_recorded_L1D_states = false;
bool champsim::operable::have_cleared_prefetcher = false;
bool champsim::operable::have_cleared_BTB = false;
bool champsim::operable::have_cleared_BP = false;
uint64_t champsim::operable::context_switch_start_cycle = 0;
bool champsim::operable::cpu_side_reset_ready = false;
uint64_t champsim::operable::cache_clear_counter = 0;
uint16_t champsim::operable::currently_active_thread_ID = 0;
std::vector<uint64_t> champsim::operable::reset_ins_count_global;
std::vector<std::string> champsim::operable::emptied_cache;
uint64_t champsim::operable::number_of_instructions_to_skip_before_log = 0;
uint64_t champsim::operable::cpu0_num_retired = 0;
std::deque<std::pair<uint64_t, uint64_t>> champsim::operable::lru_states;
// WL

// WL 
namespace reset_misc {

  on_demand_ins_access before_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  size_t before_reset_on_demand_ins_access_index;

  on_demand_ins_access after_reset_on_demand_ins_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  size_t after_reset_on_demand_ins_access_index;

  on_demand_data_access before_reset_on_demand_data_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  size_t before_reset_on_demand_data_access_index;

  on_demand_data_access after_reset_on_demand_data_access[ON_DEMAND_ACCESS_RECORD_SIZE];
  size_t after_reset_on_demand_data_access_index;

  std::deque<on_demand_ins_access> dq_before_ins_access;
  std::deque<on_demand_ins_access> dq_after_ins_access;
  std::deque<on_demand_data_access> dq_before_data_access;
  std::deque<on_demand_data_access> dq_after_data_access;
  std::deque<on_demand_data_access> dq_pf_data_access;
  std::deque<addr_occr> dq_before_knn;
  std::deque<addr_occr> dq_after_knn;
  std::deque<std::pair<uint64_t, bool>> dq_prefetch_communicate;
  bool can_record_after_access = false;
}
// WL

int main(int argc, char** argv)
{
  // WL
  std::vector<uint64_t> reset_ins_count;
  if (DUMP_INS_NUMBER_EVERY_4M_CYCLES == 0)
  {
    std::ifstream ins_number_every_4M_cycles_file;
    ins_number_every_4M_cycles_file.open("reset_ins_number.txt", std::ios::in);

    uint64_t reset_ins_count_readin;
    //std::cout << "Reset at instruction:" << std::endl;

    while(ins_number_every_4M_cycles_file >> reset_ins_count_readin)
    {
       reset_ins_count.push_back(reset_ins_count_readin);
       champsim::operable::reset_ins_count_global.push_back(reset_ins_count_readin);
       //std::cout << (unsigned)reset_ins_count_readin << std::endl;
    }

    reset_ins_count.push_back(4000000000);
    champsim::operable::reset_ins_count_global.push_back(4000000000);

    std::cout << "Number of resets: " << reset_ins_count.size() - 1 << std::endl;
    ins_number_every_4M_cycles_file.close();
  }
  // WL

  champsim::configured::generated_environment gen_environment{};

  CLI::App app{"A microarchitecture simulator for research and education"};

  bool knob_cloudsuite{false};
  uint64_t warmup_instructions = 0;
  uint64_t simulation_instructions = std::numeric_limits<uint64_t>::max();
  std::string json_file_name;
  std::vector<std::string> trace_names;

  auto set_heartbeat_callback = [&](auto) {
    for (O3_CPU& cpu : gen_environment.cpu_view())
      cpu.show_heartbeat = false;
  };

  app.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app.add_flag("--hide-heartbeat", set_heartbeat_callback, "Hide the heartbeat output");
  auto warmup_instr_option = app.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  auto deprec_warmup_instr_option =
      app.add_option("--warmup_instructions", warmup_instructions, "[deprecated] use --warmup-instructions instead")->excludes(warmup_instr_option);
  auto sim_instr_option = app.add_option("-i,--simulation-instructions", simulation_instructions,
                                         "The number of instructions in the detailed phase. If not specified, run to the end of the trace.");
  auto deprec_sim_instr_option =
      app.add_option("--simulation_instructions", simulation_instructions, "[deprecated] use --simulation-instructions instead")->excludes(sim_instr_option);

  auto json_option =
      app.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")->expected(0, 1);

  app.add_option("traces", trace_names, "The paths to the traces")->required()->expected(NUM_CPUS)->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  const bool warmup_given = (warmup_instr_option->count() > 0) || (deprec_warmup_instr_option->count() > 0);
  const bool simulation_given = (sim_instr_option->count() > 0) || (deprec_sim_instr_option->count() > 0);

  if (deprec_warmup_instr_option->count() > 0)
    fmt::print("WARNING: option --warmup_instructions is deprecated. Use --warmup-instructions instead.\n");

  if (deprec_sim_instr_option->count() > 0)
    fmt::print("WARNING: option --simulation_instructions is deprecated. Use --simulation-instructions instead.\n");

  if (simulation_given && !warmup_given)
    warmup_instructions = simulation_instructions * 2 / 10;

  std::vector<champsim::tracereader> traces;
  std::transform(
      std::begin(trace_names), std::end(trace_names), std::back_inserter(traces),
      [knob_cloudsuite, repeat = simulation_given, i = uint16_t(0)](auto name) mutable { return get_tracereader(name, i++, knob_cloudsuite, repeat); });

  std::vector<champsim::phase_info> phases{
      {champsim::phase_info{"Warmup", true, warmup_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names},
       champsim::phase_info{"Simulation", false, simulation_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names}}};

  for (auto& p : phases)
    std::iota(std::begin(p.trace_index), std::end(p.trace_index), 0);

  fmt::print("\n*** ChampSim Multicore Out-of-Order Simulator ***\nWarmup Instructions: {}\nSimulation Instructions: {}\nNumber of CPUs: {}\nPage size: {}\n\n",
             phases.at(0).length, phases.at(1).length, std::size(gen_environment.cpu_view()), PAGE_SIZE);

  auto phase_stats = champsim::main(gen_environment, phases, traces, reset_ins_count);

  // WL
  // Write the reset instructions to files
  if (DUMP_INS_NUMBER_EVERY_4M_CYCLES > 0)
  {
    std::ofstream ins_number_every_4M_cycles_file;
    ins_number_every_4M_cycles_file.open("reset_ins_number.txt", std::ios::out);

    for (uint64_t reset_ins : reset_ins_count)
    {
      ins_number_every_4M_cycles_file << reset_ins << std::endl;
    }

    ins_number_every_4M_cycles_file.close();
  }
  // WL

  fmt::print("\nChampSim completed all CPUs\n\n");

  champsim::plain_printer{std::cout}.print(phase_stats);

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_replacement_final_stats();

  if (json_option->count() > 0) {
    if (json_file_name.empty()) {
      champsim::json_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream json_file{json_file_name};
      champsim::json_printer{json_file}.print(phase_stats);
    }
  }

  return 0;
}
