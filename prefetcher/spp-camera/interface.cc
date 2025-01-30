#include "cache.h"
#include "spp.h"

#include <map>

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, spp::prefetcher> SPP;
}

void CACHE::prefetcher_initialize()
{
  std::cout << std::endl;
  std::cout << "Signature Path Prefetcher SPP-Camera" << std::endl;
  std::cout << "Signature table" << " sets: " << spp::SIGNATURE_TABLE::SET << " ways: " << spp::SIGNATURE_TABLE::WAY << std::endl;
  std::cout << "Pattern table" << " sets: " << spp::PATTERN_TABLE::SET << " ways: " << spp::PATTERN_TABLE::WAY << std::endl;
  std::cout << "Prefetch filter" << " sets: " << spp::SPP_PREFETCH_FILTER::SET << " ways: " << spp::SPP_PREFETCH_FILTER::WAY << std::endl;
  std::cout << std::endl;

  // WL 
  auto &pref = ::SPP[{this, cpu}];
  pref.prefetcher_state_file.open("prefetcher_states.txt", std::ios::out);
  pref.page_bitmap.init();
  pref.oracle.init();

  // Testing the no context switch case.
  pref.context_switch_gather_prefetches(this);
  // WL 
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];

  //if (pref.context_switch_queue_empty())
  {
    pref.update_demand(base_addr,this->get_set_index(base_addr));
    pref.initiate_lookahead(base_addr);
  }

  if (pref.oracle.ORACLE_ACTIVE && pref.oracle.RECORD_OR_REPLAY) {
    pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit);
  }

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY) {
    if (useful_prefetch) 
     pref.oracle.update_demand(this->current_cycle, base_addr, 0);
    else 
     pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit);
  }

  /*
  if (!pref.oracle.first_round && !pref.oracle.oracle_pf.empty()) 
  {
    uint64_t set = this->get_set_index(base_addr);
    //std::cout << "addr " << ((base_addr >> 6) << 6) << " hit " << (unsigned)cache_hit << " useful_prefetch " << useful_prefetch << " type " << (unsigned)type << " set " << set  << std::endl; 
  }
  */

  if (!pref.oracle.oracle_pf.empty()) 
  {
    bool require_evict = true;
    int before_acc = pref.oracle.check_pf_status(base_addr);
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle, require_evict);

    // Last access to the prefetched block used.
    if ((before_acc > remaining_acc) && (remaining_acc == 0)) {

      //if (require_evict) 
      {
        uint64_t set = this->get_set_index(base_addr);
        uint64_t way = this->get_way((base_addr >> 6) << 6, set);

        if (way < NUM_WAY) // && require_evict)
        {
          champsim::operable::lru_states.push_back(std::make_pair(set, way));
        }
      }

      pref.oracle.available_pf++;
    }
  }

  /*
  if (!useful_prefetch && !cache_hit)
  {
    pref.oracle.available_pf++;  
    std::cout << "available_pf = " << pref.oracle.available_pf << std::endl;
  }
  */

  if (cache_hit) 
  {
    pref.page_bitmap.update(base_addr);
    //pref.page_bitmap.update(ip);
  }

  if ((pref.issued_cs_pf.find((base_addr >> 6) << 6) != pref.issued_cs_pf.end()) && useful_prefetch) // ||
     //(pref.issued_cs_pf.find((ip>> 6) << 6) != pref.issued_cs_pf.end()))
  {
    pref.issued_cs_pf_hit++; 
    pref.issued_cs_pf.erase((base_addr >> 6) << 6);
    //pref.issued_cs_pf.erase((ip >> 6) << 6);
  }

  uint64_t page_addr = base_addr >> 12;
  std::pair<uint64_t, bool> demand_itself = std::make_pair(0, false);

  for(auto var : pref.available_prefetches) {

    uint64_t var_blk_no = (var.first >> 6) & 0x3F;
    uint64_t blk_no = (base_addr >> 6) & 0x3F;

    if (((var.first >> 12) == page_addr) && var_blk_no != blk_no)
      pref.context_switch_issue_queue.push_back(var); 
    else if (((var.first >> 12) == page_addr) && ((var.first >> 6) == (base_addr >> 6))) 
      demand_itself = var;
  }

  for(auto var : pref.context_switch_issue_queue) {
    pref.available_prefetches.erase(var); 
  }

  if (demand_itself.first != 0) {
    pref.available_prefetches.erase(demand_itself);
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  //uint32_t dirty_blk_evicted = (metadata_in >> 3) & 0x1;
  uint32_t blk_asid_match = (metadata_in >> 2) & 0x1; 
  //uint32_t blk_pfed = (metadata_in >> 1 & 0x1); 
  uint32_t pkt_pfed = metadata_in & 0x1;

  auto &pref = ::SPP[{this, cpu}];

  if ((!pkt_pfed) && (addr != 0))
    pref.page_bitmap.update(addr);

  if (blk_asid_match)// && !blk_pfed 
  {
    pref.page_bitmap.evict(evicted_addr);
  }

  if (blk_asid_match && !pref.oracle.oracle_pf.empty() && !pref.oracle.first_round)
    pref.oracle.update_fill(block[set * NUM_WAY + way].address);

  if (!pref.oracle.oracle_pf.empty()) //!pref.oracle.first_round && 
  {
    //std::cout << "Filled block addr " << addr << " set " << set << " way " << way << " evicting " << evicted_addr << " prefetch? " << (unsigned)prefetch << std::endl;
  }

  /*
  if (!prefetch)
  {
    pref.oracle.available_pf++;  
  }
  */
 
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::SPP[{this, cpu}];
  //pref.warmup = warmup; 

  //pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?

  // Gather and issue prefetches after a context switch.
  if (champsim::operable::context_switch_mode && !champsim::operable::L2C_have_issued_context_switch_prefetches)
  {
    // Gather prefetches via the signature and pattern tables.
    if (!pref.context_switch_prefetch_gathered)
    {
      std::cout << "remaining cs pf size " << pref.oracle.oracle_pf.size() << std::endl;

      for(auto var : pref.oracle.oracle_pf) {
        std::cout << "Addr " << var.addr << " pending accesses " << var.miss_or_hit << std::endl; 
      }

      pref.context_switch_gather_prefetches(this);
      pref.context_switch_prefetch_gathered = true;
    }
   
    if (!champsim::operable::have_cleared_BTB
        && !champsim::operable::have_cleared_BP
        && !champsim::operable::have_cleared_prefetcher
        && champsim::operable::cpu_side_reset_ready
        && champsim::operable::cache_clear_counter == 7) {
      champsim::operable::context_switch_mode = false;
      champsim::operable::cpu_side_reset_ready = false;
      champsim::operable::L2C_have_issued_context_switch_prefetches = true;
      champsim::operable::cache_clear_counter = 0;
      pref.context_switch_prefetch_gathered = false;
      pref.page_bitmap.update_bitmap_store();
      champsim::operable::emptied_cache.clear();
      pref.issued_cs_pf.clear();
      pref.oracle.can_write = true;
      //pref.clear_states();
      std::cout << "SPP states not cleared." << std::endl;
      reset_misc::can_record_after_access = true;
      std::cout << NAME << " stalled " << current_cycle - context_switch_start_cycle << " cycle(s)" << " done at cycle " << current_cycle << std::endl;
      pref.oracle.first_round = false;
      pref.oracle.access.clear();
      pref.oracle.refresh_cache_state();
      pref.oracle.interval_start_cycle = current_cycle;
    }
  }
  // Normal operation.
  // No prefetch gathering via the signature and pattern tables.
  else
  {
    if (pref.oracle.oracle_pf.size() > 0 && pref.oracle.available_pf > 0) 
    {
      uint64_t potential_cs_pf = pref.oracle.poll(current_cycle);
    
      if (potential_cs_pf != 0)
        pref.context_switch_issue_queue.push_back(std::make_pair(potential_cs_pf, 1));
    }

    pref.issue(this);
    pref.step_lookahead();
  }
}

void CACHE::prefetcher_final_stats()
{
  std::cout << "SPP STATISTICS" << std::endl;
  std::cout << std::endl;

  ::SPP[{this, cpu}].print_stats(std::cout);

  // WL 
  std::cout << "Context switch prefetch accuracy: " << ::SPP[{this, cpu}].issued_cs_pf_hit << "/" << ::SPP[{this, cpu}].total_issued_cs_pf << "." << std::endl;
  ::SPP[{this, cpu}].oracle.finish();
}

// WL
void CACHE::reset_spp_camera_prefetcher()
{
  std::cout << "=> Prefetcher cleared at CACHE " << NAME << " at cycle " << current_cycle << std::endl;
  auto &pref = ::SPP[{this, cpu}];
  pref.clear_states();
}

// WL 
void CACHE::record_spp_camera_states()
{
  std::cout << "Recording SPP states at CACHE " << NAME << std::endl;
  
  auto &pref = ::SPP[{this, cpu}];
  pref.cache_cycle = current_cycle;
  pref.record_spp_states();
}
