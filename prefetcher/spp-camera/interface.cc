#include "cache.h"
#include "spp.h"

#include <algorithm>
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

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.init();

  // Testing the no context switch case.
  pref.context_switch_gather_prefetches(this);
  // WL 
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t base_addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::SPP[{this, cpu}];

  // Return if a demand misses and cannot merge in MSHR and MSHR is full.
  /*
  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit) && !cache_hit) {

    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr == this->MSHR.end() && this->get_mshr_occupancy() == this->get_mshr_size()) {
      return metadata_in; 
    }
  }
  */

  pref.oracle.access_counter++;

  //if (pref.context_switch_queue_empty())
  {
    pref.update_demand(base_addr,this->get_set_index(base_addr));
    pref.initiate_lookahead(base_addr);
  }

  /*
  if (pref.oracle.ORACLE_ACTIVE && pref.oracle.RECORD_OR_REPLAY) {
    uint64_t evict_candidate = pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit, 0);

    if (evict_candidate != 0)
    {
      this->do_not_fill_address.push_back(evict_candidate);
    }    
  }
  */

  //std::cout << "Hit/miss " << (unsigned)cache_hit << " set " << this->get_set_index(base_addr) << " addr " << base_addr << " at cycle " << this->current_cycle << " type " << (unsigned)type << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && !pref.oracle.RECORD_OR_REPLAY && !(type == 2 && cache_hit)) {

    auto search_mshr = std::find_if(std::begin(this->MSHR), std::end(this->MSHR),
                                 [match = base_addr >> this->OFFSET_BITS, shamt = this->OFFSET_BITS](const auto& entry) {
                                   return (entry.address >> shamt) == match; 
                                 });

    if (search_mshr != this->MSHR.end() && champsim::to_underlying(search_mshr->type) == 2) {
      useful_prefetch = true; 
      cache_hit = true;
      pref.oracle.hit_in_MSHR++;
      //std::cout << "Hit in MSHR" << std::endl;
    }

    if (useful_prefetch) {

      uint64_t res = pref.oracle.update_demand(this->current_cycle, base_addr, 0, 0);
      /*
      if (res != 0)
        this->do_not_fill_address.push_back(res);
        */
    }
    else {
      uint64_t res = pref.oracle.update_demand(this->current_cycle, base_addr, cache_hit, 1);
      /*
      if (res != 0)
        this->do_not_fill_address.push_back(res);
        */
    }      

    /*
    if (!cache_hit && this->get_mshr_occupancy() >= this->get_mshr_size()) {
      std::cout << "Simulation killed due to no available MSHR." << std::endl;
      pref.oracle.kill_simulation(this->current_cycle, base_addr, cache_hit);
    }
    */
    /*
    if (pref.oracle.oracle_pf.empty() && cache_hit) {
      pref.oracle.update_persistent_lru_addr(base_addr, true); 
    }
    */
  }

  pref.oracle.hit_address = (base_addr >> 6) << 6;

  if (pref.oracle.ORACLE_ACTIVE && cache_hit && !pref.oracle.RECORD_OR_REPLAY) //!pref.oracle.oracle_pf.empty() && 
  {
    int before_acc = pref.oracle.check_pf_status(base_addr);
    bool evict = pref.oracle.check_require_eviction(base_addr);
    int remaining_acc = pref.oracle.update_pf_avail(base_addr, current_cycle - pref.oracle.interval_start_cycle);

    // Last access to the prefetched block used.
    if ((before_acc > remaining_acc) && (remaining_acc == 0) && evict) {  

      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way((base_addr >> 6) << 6, set);

      if (way < NUM_WAY) {
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 0));
      }
    }
    else if (remaining_acc > 0) {
      uint64_t set = this->get_set_index(base_addr);
      uint64_t way = this->get_way((base_addr >> 6) << 6, set);

      if (way < NUM_WAY) {
        champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
        //std::cout << "Updated LRU for addr " << base_addr << " at set " << set << std::endl;
        //pref.oracle.update_persistent_lru_addr(base_addr, false); 
      } 
    }
  }

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

  //uint64_t page_addr = base_addr >> 12;
  //std::pair<uint64_t, bool> demand_itself = std::make_pair(0, false);

  // for(auto var : pref.available_prefetches) {

  //   uint64_t var_blk_no = (var.first >> 6) & 0x3F;
  //   uint64_t blk_no = (base_addr >> 6) & 0x3F;

  //   if (((var.first >> 12) == page_addr) && var_blk_no != blk_no)
  //     pref.context_switch_issue_queue.push_back({var.first, 0, var.second}); 
  //   else if (((var.first >> 12) == page_addr) && ((var.first >> 6) == (base_addr >> 6))) 
  //     demand_itself = var;
  // }

  // for(auto var : pref.context_switch_issue_queue) {
  //   pref.available_prefetches.erase(var); 
  // }

  // if (demand_itself.first != 0) {
  //   pref.available_prefetches.erase(demand_itself);
  // }

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
    pref.page_bitmap.evict(evicted_addr);

  //std::cout << "Filled addr " << addr << " set " << set << " way " << way << " prefetch " << (unsigned)prefetch << " evicted_addr " << evicted_addr << " at cycle " << this->current_cycle << std::endl;

  if (pref.oracle.ORACLE_ACTIVE && prefetch) {
    champsim::operable::lru_states.push_back(std::make_tuple(set, way, 1));
    //pref.oracle.update_persistent_lru_addr(addr, false);
  } 

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  auto &pref = ::SPP[{this, cpu}];

  if (pref.oracle.done) {
    champsim::operable::kill_simulation_l2 = true; 
  }
  if (champsim::operable::kill_simulation_l2 && champsim::operable::kill_simulation_l3) {
    exit(0); 
  }

  //pref.warmup = warmup; 

  //pref.warmup = warmup_complete[cpu];
  // TODO: should this be pref.warmup = warmup_complete[cpu]; instead of pref.warmup = warmup; ?

  // Gather and issue prefetches after a context switch.
  if (pref.oracle.ORACLE_ACTIVE && champsim::operable::context_switch_mode && !champsim::operable::L2C_have_issued_context_switch_prefetches)
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
    if (pref.oracle.ORACLE_ACTIVE && ((pref.oracle.oracle_pf.size() > 0))) // && pref.oracle.available_pf > 0 && pref.oracle.hit_address != 0)
    {
      std::tuple<uint64_t, uint64_t, bool> potential_cs_pf = pref.oracle.poll(pref.oracle.hit_address);
    
      if (std::get<0>(potential_cs_pf) != 0) {
         pref.context_switch_issue_queue.push_back({std::get<0>(potential_cs_pf), std::get<2>(potential_cs_pf), std::get<1>(potential_cs_pf)});
      }
      else {
        //std::cout << "Poll failed at cycle " << this->current_cycle << " MSHR " << this->get_mshr_occupancy() << std::endl;
      }
        
      /*
      uint64_t index = pref.oracle.access_counter + 1000;

      if (index < pref.oracle.readin.size() && pref.oracle.last_access_counter < pref.oracle.access_counter) {
        pref.context_switch_issue_queue.push_back(std::make_pair(pref.oracle.readin.at(index).addr, 0));
        pref.oracle.last_access_counter = pref.oracle.access_counter;
      }
      */
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

  auto &pref = ::SPP[{this, cpu}];

  if (pref.oracle.ORACLE_ACTIVE)
    pref.oracle.finish();
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
