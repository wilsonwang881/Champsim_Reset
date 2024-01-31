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

#include "champsim.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <vector>

#include "environment.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "phase_info.h"
#include "tracereader.h"
#include <fmt/chrono.h>
#include <fmt/core.h>

// WL
#include <iostream>

constexpr int DEADLOCK_CYCLE{500};

auto start_time = std::chrono::steady_clock::now();

// WL
#define RESET_INTERVAL 4000000
uint64_t next_reset_moment = 0;

std::chrono::seconds elapsed_time() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time); }

namespace champsim
{
phase_stats do_phase(phase_info phase, environment& env, std::vector<tracereader>& traces, std::vector<uint64_t>& reset_ins_count)
{
  // WL
  int reset_ins_count_readin_index = 0;
  int num_resets = reset_ins_count.size();

  if (DUMP_INS_NUMBER_EVERY_4M_CYCLES > 0)
    next_reset_moment = 4000000; // dummy value, will be overwritten after warmup is completed
  else 
  {
    next_reset_moment = reset_ins_count[0];
    reset_ins_count_readin_index++; 
  }
  // WL

  auto [phase_name, is_warmup, length, trace_index, trace_names] = phase;
  auto operables = env.operable_view();

  // Initialize phase
  for (champsim::operable& op : operables) {
    op.warmup = is_warmup;
    op.begin_phase();
  }

  //WL
  O3_CPU& cpu_0 = env.cpu_view()[0];

  if (DUMP_INS_NUMBER_EVERY_4M_CYCLES > 0)
  {
    next_reset_moment = cpu_0.current_cycle + 4000000;
  }

  std::cout << "Resetting start at cycle " << next_reset_moment << std::endl;
  // WL

  // Perform phase
  int stalled_cycle{0};
  std::vector<bool> phase_complete(std::size(env.cpu_view()), false);
  while (!std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and{})) {
    auto next_phase_complete = phase_complete;

    // Operate
    long progress{0};
    for (champsim::operable& op : operables) {
      progress += op._operate();
    }

    if (progress == 0) {
      ++stalled_cycle;
    } else {
      stalled_cycle = 0;
    }

    if (stalled_cycle >= DEADLOCK_CYCLE) {
      std::for_each(std::begin(operables), std::end(operables), [](champsim::operable& c) { c.print_deadlock(); });
      abort();
    }

    std::sort(std::begin(operables), std::end(operables),
              [](const champsim::operable& lhs, const champsim::operable& rhs) { return lhs.leap_operation < rhs.leap_operation; });

    // WL
    if (DUMP_INS_NUMBER_EVERY_4M_CYCLES > 0)
    {
      if (cpu_0.current_cycle >= next_reset_moment)
      {
        reset_ins_count.push_back(cpu_0.num_retired);
        next_reset_moment += RESET_INTERVAL;

        std::cout << std::endl << "Recording @ins. count = " << cpu_0.num_retired << " at cycle " << cpu_0.current_cycle << std::endl;
        champsim::operable::have_recorded_on_demand_accesses = true;
        champsim::operable::have_recorded_before_reset_on_demand_accesses = true;
        champsim::operable::have_recorded_before_reset_hit_miss_number_L1I = true;
        champsim::operable::have_recorded_before_reset_hit_miss_number_L1D = true;
        champsim::operable::have_recorded_before_reset_hit_miss_number_L2C = true;
        champsim::operable::have_recorded_before_reset_hit_miss_number_LLC = true;
        champsim::operable::have_recorded_prefetcher_states = true;
        champsim::operable::have_recorded_L1I_states = true;
        champsim::operable::have_recorded_L1D_states = true;
        champsim::operable::context_switch_mode = false;
        champsim::operable::have_cleaned_L1I = false;
        champsim::operable::have_cleaned_L1D = false;
        champsim::operable::have_cleaned_L2C = false;
        champsim::operable::have_cleaned_LLC = false;
        champsim::operable::have_cleaned_ITLB = false;
        champsim::operable::have_cleaned_DTLB = false;
        champsim::operable::have_cleaned_STLB = false;
      }
    }
    else
    {
    if ((cpu_0.num_retired + cpu_0.input_queue.size())>= next_reset_moment && 
        reset_ins_count_readin_index <= num_resets) {

      // Assume the overhead is 1 microscrond.
      // During the overhead, CPU does not take in instructions.
      //ooo_cpu[0]->context_switch_stall = CONTEXT_SWITCH_OVERHEAD_CYCLES;

      std::cout << std::endl << "Resetting @ins. count = " << cpu_0.num_retired << " at cycle " << cpu_0.current_cycle << std::endl;

      champsim::operable::context_switch_mode = true;
      champsim::operable::have_recorded_on_demand_accesses = true;
      champsim::operable::have_recorded_before_reset_on_demand_accesses = true;
      champsim::operable::have_recorded_before_reset_hit_miss_number_L1I = true;
      champsim::operable::have_recorded_before_reset_hit_miss_number_L1D = true;
      champsim::operable::have_recorded_before_reset_hit_miss_number_L2C = true;
      champsim::operable::have_recorded_before_reset_hit_miss_number_LLC = true;
      champsim::operable::have_recorded_prefetcher_states = true;
      champsim::operable::have_recorded_L1I_states = true;
      champsim::operable::have_recorded_L1D_states = true;
      champsim::operable::have_cleared_L1I = true;
      champsim::operable::have_cleared_L1D = true;
      champsim::operable::have_cleared_L2C = true;
      champsim::operable::have_cleared_LLC = true;
      champsim::operable::have_cleared_ITLB = true;
      champsim::operable::have_cleared_DTLB = true;
      champsim::operable::have_cleared_STLB = true;
      champsim::operable::have_cleared_prefetcher = true;
      champsim::operable::have_cleared_BTB = true;
      champsim::operable::have_cleared_BP = true;
      champsim::operable::cache_clear_counter = 0;

      cpu_0.reset_ins_count = next_reset_moment;

      // prevent out of range index
      if (reset_ins_count_readin_index < num_resets)
        next_reset_moment = reset_ins_count[reset_ins_count_readin_index];
            
      reset_ins_count_readin_index++;
    }    
    // WL
   }

    // Read from trace
    // WL: added condition to check if the simulator is in context switch mode
    if (!champsim::operable::context_switch_mode)
    {
	    // WL: original code
	    for (O3_CPU& cpu : env.cpu_view()) {
	      auto& trace = traces.at(trace_index.at(cpu.cpu));
	      for (auto pkt_count = cpu.IN_QUEUE_SIZE - static_cast<long>(std::size(cpu.input_queue)); !trace.eof() && pkt_count > 0; --pkt_count)
		cpu.input_queue.push_back(trace());

	      // If any trace reaches EOF, terminate all phases
	      if (trace.eof())
		std::fill(std::begin(next_phase_complete), std::end(next_phase_complete), true);
	    }
	    // WL: end of original code
    }
    // WL

    // Check for phase finish
    for (O3_CPU& cpu : env.cpu_view()) {
      // Phase complete
      next_phase_complete[cpu.cpu] = next_phase_complete[cpu.cpu] || (cpu.sim_instr() >= length);
    }

    for (O3_CPU& cpu : env.cpu_view()) {
      if (next_phase_complete[cpu.cpu] != phase_complete[cpu.cpu]) {
        for (champsim::operable& op : operables)
          op.end_phase(cpu.cpu);

        fmt::print("{} finished CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
                   cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
      }
    }

    phase_complete = next_phase_complete;
  }

  for (O3_CPU& cpu : env.cpu_view()) {
    fmt::print("{} complete CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", phase_name, cpu.cpu,
               cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
  }

  phase_stats stats;
  stats.name = phase.name;

  for (std::size_t i = 0; i < std::size(trace_index); ++i)
    stats.trace_names.push_back(trace_names.at(trace_index.at(i)));

  auto cpus = env.cpu_view();
  std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [](const O3_CPU& cpu) { return cpu.sim_stats; });
  std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [](const O3_CPU& cpu) { return cpu.roi_stats; });

  auto caches = env.cache_view();
  std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.sim_cache_stats), [](const CACHE& cache) { return cache.sim_stats; });
  std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.roi_cache_stats), [](const CACHE& cache) { return cache.roi_stats; });

  auto dram = env.dram_view();
  std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
                 [](const DRAM_CHANNEL& chan) { return chan.sim_stats; });
  std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
                 [](const DRAM_CHANNEL& chan) { return chan.roi_stats; });

  return stats;
}

// simulation entry point
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces, std::vector<uint64_t>& reset_ins_count)
{
  for (champsim::operable& op : env.operable_view())
    op.initialize();

  std::vector<phase_stats> results;
  for (auto phase : phases) {
    auto stats = do_phase(phase, env, traces, reset_ins_count);
    if (!phase.is_warmup)
      results.push_back(stats);
  }

  return results;
}
} // namespace champsim
