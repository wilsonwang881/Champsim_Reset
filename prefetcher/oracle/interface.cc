#include "cache.h"
#include "oracle.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, oracle::prefetcher> ORACLE;
}

void CACHE::prefetcher_initialize()
{
  auto &pref = ::ORACLE[{this, cpu}];
  pref.init();
  pref.file_read();

  std::cout << NAME << "-> Prefetcher Oracle initialized @ cycle " << current_cycle << "." << std::endl;
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::ORACLE[{this, cpu}];
 
  if (!cache_hit) 
  {
    pref.update(this->current_cycle, addr);
  }

  pref.check_progress(this->current_cycle - pref.interval_start_cycle, addr);

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::ORACLE[{this, cpu}];

  if (champsim::operable::context_switch_mode && !champsim::operable::L2C_have_issued_context_switch_prefetches)
  {
    if (!champsim::operable::have_cleared_BTB
        && !champsim::operable::have_cleared_BP
        && !champsim::operable::have_cleared_prefetcher
        && champsim::operable::cpu_side_reset_ready
        && champsim::operable::cache_clear_counter == 7) {
      champsim::operable::context_switch_mode = false;
      champsim::operable::cpu_side_reset_ready = false;
      champsim::operable::L2C_have_issued_context_switch_prefetches = true;
      champsim::operable::cache_clear_counter = 0;
      champsim::operable::emptied_cache.clear();
      reset_misc::can_record_after_access = false;
      pref.file_read();
      pref.file_write();
      pref.can_write = true;
      pref.interval_start_cycle = this->current_cycle;
      std::cout << "cycles_speedup = " << pref.cycles_speedup << std::endl;
      pref.cycles_speedup = 0;
    }
  }

  else 
  {
    if (!pref.cs_pf.empty()) 
    {
      oracle::prefetcher::acc_timestamp tmpp = pref.cs_pf.front();
      uint64_t sum_1 = tmpp.cycle_diff + pref.interval_start_cycle;
      uint64_t sum_2 = pref.cycles_speedup + this->current_cycle;
      uint64_t diff = (sum_1 < sum_2) ? 0 : (sum_1 - sum_2);
      bool pf_or_not = (sum_1 <= (sum_2 + 2000));

      //std::cout << "sum_1 = " << sum_1 << " sum_2 = " << sum_2 << " diff = " << diff << std::endl;
      //std::cout << "diff = " << diff << " another = " << (tmpp.cycle_diff - pref.cycles_speedup - (this->current_cycle - pref.interval_start_cycle)) << std::endl;
      //std::cout << "pf = " << pf_or_not << std::endl;

      //if ((tmpp.cycle_diff - pref.cycles_speedup - (this->current_cycle - pref.interval_start_cycle)) <= 2000)
      //if (pf_or_not) 
      {
        bool prefetched = prefetch_line(tmpp.addr, true, 0);

        if (prefetched) 
        {
          //std::cout << tmpp.cycle_diff << " <-> " << this->current_cycle - pref.interval_start_cycle << std::endl;
          pref.cs_pf.pop_front(); 
        }
      }
    }
  }
}

void CACHE::prefetcher_final_stats()
{
  auto &pref = ::ORACLE[{this, cpu}];
  pref.file_write();
  pref.finish();
}

