#include "oracle.h"

void spp_l3::SPP_ORACLE::init() {

  // Clear the access file if in recording mode.
  if (RECORD_OR_REPLAY) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }

  for (size_t i = 0; i < SET_NUM; i++) {
    set_availability[i] = WAY_NUM;
   
    for (size_t j = i * WAY_NUM; j < (i + 1) * WAY_NUM; j++) {
     cache_state[j].addr = 0;
     cache_state[j].pending_accesses = 0;
     cache_state[j].timestamp = 0;
     cache_state[j].require_eviction = true;
    }
  }
  
  std::cout << "Oracle: rollback " << (ROLLBACK_ENABLED ? "enabled." : "disabled.") << std::endl;

  file_read();
}

void spp_l3::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit, bool replay, uint64_t type) {

  if (!RECORD_OR_REPLAY) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle;
    tmpp.miss_or_hit = hit;
    tmpp.type = type;
    uint64_t set_check = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

    if (!hit && replay) {
      tmpp.cycle_demanded = cycle;    
      uint64_t way_check = check_set_pf_avail(addr);   

      if (way_check == WAY_NUM) {
        if (oracle_pf_size == 0 && type != 3) { 
          set_kill_counter[set_check].insert((addr >> 6) << 6);
          new_misses++;
        }

        if (set_kill_counter[set_check].size() > WAY_NUM) {
          //std::cout << "Simulation killed at a with set " << set_check << " way " << way_check << std::endl;
          //kill_simulation();
        }
      }
      else if(way_check < WAY_NUM && 
              cache_state[set_check * WAY_NUM + way_check].addr != ((addr >> 6) << 6)) {
        assert(cache_state[set_check * WAY_NUM + way_check].addr == 0);

        if (oracle_pf_size == 0 && type != 3) {  
          set_kill_counter[set_check].insert((addr >> 6) << 6);
          new_misses++;
        }

        if (set_kill_counter[set_check].size() > WAY_NUM) {
          //std::cout << "Simulation killed at b with set " << set_check << " way " << way_check << std::endl;
          //kill_simulation();
        }
      }      
      else if(way_check < WAY_NUM && 
              cache_state[set_check * WAY_NUM + way_check].addr == ((addr >> 6) << 6)) {
        //update_pf_avail(addr, cycle);
      }
      else {
        std::cout << "Failed: way " << way_check << " set " << set_check << " addr " << ((addr >> 6) << 6) << " cache_state addr " << cache_state[set_check * WAY_NUM + way_check].addr  << std::endl;
        assert(false);
      }
    }

    tmpp.addr = (addr >> 6) << 6;    
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) 
      file_write();
  }
}

void spp_l3::SPP_ORACLE::file_write() {

  if (access.size() > 0) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : access)
      rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << " " << (unsigned)var.type << std::endl;

    rec_file.close();
    std::cout << "Oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  } 
}

void spp_l3::SPP_ORACLE::file_read() {
  acc_timestamp tmpp;
  std::cout << "Parsing memory accesses" << std::endl;

  if (BELADY_CACHE_REPLACEMENT_POLICY_ACTIVE) 
    std::cout << "Belady's cache replacement policy active." << std::endl;
  else if (REUSE_DISTANCE_REPLACEMENT_POLICY_ACTIVE) 
    std::cout << "Reuse distance based cache replacement policy active." << std::endl;
  else 
    std::cout << "LRU cache replacement policy active." << std::endl;

  for (int set_partition = 0; set_partition < MEMORY_USAGE_REDUCTION_FACTOR && !RECORD_OR_REPLAY; set_partition++) {
    int set_number_begin = SET_NUM / MEMORY_USAGE_REDUCTION_FACTOR * set_partition;
    int set_number_end = SET_NUM / MEMORY_USAGE_REDUCTION_FACTOR * (set_partition + 1);
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit;
    uint8_t type;
    std::deque<acc_timestamp> readin;

    while(!rec_file.eof()) {
      rec_file >> readin_cycle_demanded >> readin_addr >> readin_miss_or_hit >> type;
      tmpp.cycle_demanded = readin_cycle_demanded;
      tmpp.addr = (readin_addr >> 6) << 6;
      tmpp.set = (tmpp.addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));   
      tmpp.miss_or_hit = readin_miss_or_hit;
      tmpp.require_eviction = true;
      tmpp.type = type;

      if (readin_addr == 0)
        break; 

      if (tmpp.set >= set_number_begin && tmpp.set < set_number_end) 
        readin.push_back(tmpp);
    }

    rec_file.close();
    std::cout << "Oracle: read " << readin.size() << " accesses from file for set " << set_number_begin << " to set " << (set_number_end - 1) << std::endl;

    if (BELADY_CACHE_REPLACEMENT_POLICY_ACTIVE) {
      for (int set_number = set_number_begin; set_number < set_number_end; set_number++) {
        // Separate accesses into different sets.
        std::deque<acc_timestamp> set_processing;

        for(auto var : readin) {
          if (var.set == set_number) 
            set_processing.push_back(var);
        } 

        // Use the optimal cache replacement policy to work out hit/miss for each access.
        std::vector<acc_timestamp> set_container;

        for (uint64_t i = 0; i < set_processing.size(); i++) {
          bool found = false;

          for(auto &blk : set_container) {
            if (blk.addr == set_processing[i].addr) {
              found = true;
              blk.cycle_demanded = set_processing[i].cycle_demanded;
              break;
            } 
          }

          // The set has the block.
          if (found) 
            set_processing[i].miss_or_hit = 1; 
          // The set does not have the block.
          else {
            // The set has space.
            if (set_container.size() < WAY_NUM) {
              // Update the set.
              set_container.push_back(set_processing[i]);

              // Set the new block to be a miss.
              set_processing[i].miss_or_hit = 0;

              // Safety check.
              assert(set_container.size() <= WAY_NUM);
            }
            // The set has no space.
            else {
              // Calculate the re-use distance.
              for(auto &el : set_container) {
                uint64_t distance = std::numeric_limits<uint64_t>::max(); //1.0; 

                for(uint64_t j = i + 1; j < set_processing.size(); j++) { 
                  if (set_processing[j].addr == el.addr) {
                    distance = set_processing[j].cycle_demanded; 
                    break; 
                  }  
                }

                el.reuse_distance = distance;
              }

              // Evict the block with the longest reuse distance.
              uint64_t reuse_distance = set_container[0].reuse_distance;
              uint64_t eviction_candidate = 0;

              for (uint64_t j = 0; j < WAY_NUM; j++) {
                if (set_container[j].reuse_distance >= reuse_distance) {
                  reuse_distance = set_container[j].reuse_distance;
                  eviction_candidate = j; 
                }
              }

              // Evict the block.
              set_container.erase(set_container.begin() + eviction_candidate);

              // Set the new block to be a miss.
              set_processing[i].miss_or_hit = 0;

              // Update the set.
              set_container.push_back(set_processing[i]);

              // Safety check.
              assert(set_container.size() <= WAY_NUM);
            }
          }
        }

        for(auto &acc : readin) {
          if (acc.set == set_number && acc.addr == set_processing.front().addr) {
            acc.miss_or_hit = set_processing.front().miss_or_hit;
            set_processing.pop_front();
          }
        }

        assert(set_processing.size() == 0);
      }
    }
    else if (REUSE_DISTANCE_REPLACEMENT_POLICY_ACTIVE) {
      for (int set_number = set_number_begin; set_number < set_number_end; set_number++) {
        // Separate accesses into different sets.
        std::deque<acc_timestamp> set_processing;

        for(auto var : readin) {
          if (var.set == set_number) 
            set_processing.push_back(var);
        } 

        std::map<uint64_t, std::deque<uint64_t>*> not_in_cache;
        std::map<uint64_t, std::deque<uint64_t>*> in_cache;
        std::map<uint64_t, bool> accessed;

        // Gather timestamps for each address.
        for(auto el : set_processing) {
          auto search = not_in_cache.find(el.addr); 

          if (search == not_in_cache.end()) 
            not_in_cache[el.addr] = new std::deque<uint64_t>();

          not_in_cache[el.addr]->push_back(el.cycle_demanded); 
        }

        size_t fill_limit = std::min((std::size_t)WAY_NUM, not_in_cache.size());

        // Fill cache.
        for (size_t i = 0; i < fill_limit; i++) {
          auto it = std::min_element(std::begin(not_in_cache), std::end(not_in_cache),
                    [](const auto& l, const auto& r) { return l.second->front() < r.second->front(); });
          assert(not_in_cache.size() > 0);
          in_cache[it->first] = not_in_cache[it->first];
          accessed[it->first] = false;
          not_in_cache.erase(it->first);
        }

        for (uint64_t i = 0; i < set_processing.size(); i++) {
          acc_timestamp *current_acc = &set_processing[i];
          uint64_t addr = current_acc->addr;

          // Check if the block is already in cache.
          auto block_in_cache = in_cache.find(addr);

          // If the block is in cache.
          if (block_in_cache != in_cache.end()) {
            // Pop the access timestamp.
            in_cache[addr]->pop_front();

            if (accessed[addr]) 
              current_acc->miss_or_hit = 1; 
            else {
              current_acc->miss_or_hit = 0;
              accessed[addr] = true;
            }

            if (in_cache[addr]->size() == 0) {
              delete in_cache[addr];
              in_cache[addr] = nullptr;
              in_cache.erase(addr); 
              accessed.erase(addr);

              // Fill the gap.
              if (not_in_cache.size() > 0) {
                auto it_gap = std::min_element(std::begin(not_in_cache), std::end(not_in_cache),
                              [](const auto& l, const auto& r) { return l.second->front() < r.second->front(); });
                in_cache[it_gap->first] = not_in_cache[it_gap->first];
                accessed[it_gap->first] = false;
                not_in_cache.erase(it_gap->first);
              }
            }

            // Replacement.
            if (not_in_cache.size() > 0) {
              auto it = std::min_element(std::begin(not_in_cache), std::end(not_in_cache),
                        [](const auto& l, const auto& r) { return l.second->front() < r.second->front(); });

              // Space available in the set.
              if (in_cache.size() < WAY_NUM) {
                in_cache[it->first] = not_in_cache[it->first];
                accessed[it->first] = false;
                not_in_cache.erase(it->first);
              }
              // No space available.
              // Need replacement.
              else if (in_cache.size() == WAY_NUM) {
                auto it_in_cache = std::min_element(std::begin(in_cache), std::end(in_cache),
                                   [](const auto& l, const auto& r) { return l.second->front() < r.second->front(); }); 
                
                if (it->second->front() < it_in_cache->second->front()) {
                  not_in_cache[it_in_cache->first] = it_in_cache->second;
                  in_cache[it->first] = it->second;
                  accessed.erase(it_in_cache->first);
                  in_cache.erase(it_in_cache->first);
                  accessed[it->first] = false;
                  not_in_cache.erase(it->first);
                }
              }
              else 
                assert(false);

              assert(accessed.size() <= WAY_NUM);
              assert(in_cache.size() <= WAY_NUM);
            }
          }
          // The block is not in the cache.
          else 
            assert(false);
        }

        for(auto &acc : readin) {
          if (acc.set == set_number && acc.addr == set_processing.front().addr) {
            acc.miss_or_hit = set_processing.front().miss_or_hit;
            set_processing.pop_front();
          }
        }

        assert(set_processing.size() == 0);
      }
    }

    // Use the hashmap to gather accesses.
    std::map<uint64_t, uint32_t> addr_counter_map;

    for (int i = readin.size() - 1; i >= 0; i--) {
      acc_timestamp tmpp_readin = readin[i];

      if (tmpp_readin.miss_or_hit == 1) {
        if (auto search = addr_counter_map.find(tmpp_readin.addr); search != addr_counter_map.end()) 
          addr_counter_map[tmpp_readin.addr]++; 
        else 
          addr_counter_map[tmpp_readin.addr] = 1;

        tmpp_readin.miss_or_hit = 0;
        readin[i].miss_or_hit = 0;
      }
      else {
        if (auto search = addr_counter_map.find(tmpp_readin.addr); search != addr_counter_map.end()) 
          addr_counter_map[tmpp_readin.addr]++; 
        else 
          addr_counter_map[tmpp_readin.addr] = 1;

        tmpp_readin.miss_or_hit = addr_counter_map[tmpp_readin.addr];
        readin[i].miss_or_hit = addr_counter_map[tmpp_readin.addr];
        addr_counter_map.erase(tmpp_readin.addr);
      }
    }

    for(auto var : readin) {
      if (var.miss_or_hit != 0) 
        oracle_pf[var.set].push_back(var); 
    }

    std::cout << "Done updating hits/misses for set " << set_number_begin << " to set " << (set_number_end - 1) << std::endl;
  }

  if (!RECORD_OR_REPLAY) {
    oracle_pf_size = 0;

    for(auto var : oracle_pf) 
      oracle_pf_size += var.size(); 

    for(auto &set_pf: oracle_pf) {
      if (set_pf.size() >= WAY_NUM) {
        for (size_t i = set_pf.size() - 1; i > set_pf.size() - 1 - WAY_NUM; i--) 
          set_pf[i].require_eviction = false; 
      }
      else {
        for(auto &pf : set_pf) 
          pf.require_eviction = false; 
      }
    }

    uint64_t non_pf_counter = 0;

    for(auto set_pf: oracle_pf) {
      for(auto var : set_pf) {
        if (var.type == 3) 
          non_pf_counter++; 
      }
    }

    std::cout << "Oracle: pre-processing collects " << oracle_pf_size << " prefetch targets from file read." << std::endl;
    std::cout << "Oracle: skipping " << non_pf_counter << " prefetch targets because they are WRITE misses." << std::endl;
    std::cout << "Oracle: issuing " << (oracle_pf_size - non_pf_counter) << " prefetches." << std::endl;
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }
}

uint64_t spp_l3::SPP_ORACLE::check_set_pf_avail(uint64_t addr) {
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  uint64_t res = (set + 1) * WAY_NUM;
  addr = (addr >> 6) << 6;
  uint64_t i;
  bool found = false;
 
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0 && cache_state[i].require_eviction)
      assert(false); 

    if (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0 && cache_state[i].require_eviction) {
      std::cout << "pending_access = " << cache_state[i].pending_accesses << " addr " << addr << std::endl;
      assert(false); 
    }
      
    if (cache_state[i].addr == addr) {
      res = i;
      found = true;
      break;
    }
  }

  if (!found) {
    for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
      if (cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0 && cache_state[i].require_eviction)
        assert(false); 

      if (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0 && cache_state[i].require_eviction) {
        std::cout << "pending_access = " << cache_state[i].pending_accesses << " addr " << addr << std::endl;
        assert(false); 
      }
        
      if (cache_state[i].pending_accesses == 0 && cache_state[i].addr == 0) {
        res = i;
        break;
      }
    }
  }

  return res - set * WAY_NUM;
}

int spp_l3::SPP_ORACLE::check_pf_status(uint64_t addr) {
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

  // Find the "way" to update pf/block status.
  size_t i;
  addr = (addr >> 6) << 6;

  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (cache_state[i].addr == addr)
      break;  
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else 
    return -1;
}

int spp_l3::SPP_ORACLE::update_pf_avail(uint64_t addr, uint64_t cycle) {
  if (RECORD_OR_REPLAY)
    return 1; 

  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

  // Find the "way" to update pf/block status.
  size_t i;
  int same_addr_counter = 0;
  int res = -1;

  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {

    if (cache_state[i].addr == addr) {
      cache_state[i].pending_accesses--;
      res = cache_state[i].pending_accesses;
      cache_state[i].accessed = true;

      if (ORACLE_DEBUG_PRINT) 
        std::cout << "Accessed addr = " << addr << " at set " << set << " way " << i - set * WAY_NUM << " remaining accesses " << cache_state[i].pending_accesses << std::endl;

      cache_state[i].timestamp = cycle; 

      if (cache_state[i].pending_accesses == 0) {
        cache_state[i].addr = 0; 
        cache_state[i].timestamp = 0;
        set_availability[set]++;
        assert(set_availability[set] <= WAY_NUM);
      }

      same_addr_counter++;

      //break;  
    } 
  }

  assert(same_addr_counter <= 1);

  return res;
}

bool spp_l3::SPP_ORACLE::check_require_eviction(uint64_t addr) {
  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  bool need_eviction = true;

  // Find the "way" to update pf/block status.
  for (size_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (cache_state[i].addr == addr) {
      need_eviction = cache_state[i].require_eviction;
      break;  
    } 
  }

  return need_eviction;
}

std::vector<std::tuple<uint64_t, uint64_t, bool, bool>> spp_l3::SPP_ORACLE::poll(CACHE* cache) {
  std::vector<std::tuple<uint64_t, uint64_t, bool, bool>> target_v;

  if (oracle_pf_size == 0) 
    return target_v; 

  for (size_t set = 0; set < SET_NUM; set++) {
    while (set_availability[set] > 0 && oracle_pf[set].size() > 0) {
      auto ite = &oracle_pf[set].front();
      uint64_t way = check_set_pf_avail(ite->addr);

      if (way < WAY_NUM) {
        std::tuple<uint64_t, uint64_t, bool, bool> target = std::make_tuple(0, 0, 0, 0);
        std::get<0>(target) = ite->addr;
        std::get<1>(target) = ite->cycle_demanded;
        std::get<2>(target) = true;
        int before_counter = cache_state[set * WAY_NUM + way].pending_accesses;

        if (cache_state[set * WAY_NUM + way].addr != ite->addr) 
          set_availability[set]--;

        /*
        if (ite->type == 3) { // || ite->type == 1
          std::get<3>(target) = true;

          if (ORACLE_DEBUG_PRINT) 
            std::cout << "Skipping addr " << ite->addr << " type " << ite->type << std::endl;
        } 
        */

        cache_state[set * WAY_NUM + way].addr = ite->addr;
        cache_state[set * WAY_NUM + way].require_eviction = ite->require_eviction;
        cache_state[set * WAY_NUM + way].timestamp = ite->cycle_demanded;
        cache_state[set * WAY_NUM + way].type = ite->type;
        cache_state[set * WAY_NUM + way].accessed = false;

        //if (cache_state[set * WAY_NUM + way].pending_accesses == 0) 
        target_v.push_back(target);

        cache_state[set * WAY_NUM + way].pending_accesses += (int)(ite->miss_or_hit);
        oracle_pf[set].pop_front();
        oracle_pf_size--;
        assert(set_availability[set] >= 0);

        if ((((oracle_pf_size % 10000 == 0) || oracle_pf_size == 0)) && 
            heartbeat_printed.find(oracle_pf_size) == heartbeat_printed.end()) {
            std::cout << "Oracle: remaining oracle access = " << oracle_pf_size - pf_issued << " useless: " << cache->sim_stats.pf_useless << std::endl;
            heartbeat_printed.insert(oracle_pf_size);

            for(auto &set_pf : oracle_pf) 
              set_pf.shrink_to_fit();
        }

        if (ORACLE_DEBUG_PRINT) 
          std::cout << "Runahead PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " added accesses " << ite->miss_or_hit << " before accesses " << before_counter << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << " type " << ite->type << std::endl;
      }
    } 
  }

  return target_v;
}

uint64_t spp_l3::SPP_ORACLE::rollback_prefetch(uint64_t addr) {
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  uint64_t latest_cycle = cache_state[set * WAY_NUM].timestamp;
  uint64_t index = set * WAY_NUM;
  bool not_accessed_pf_found = false;

  for (uint64_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (!cache_state[i].accessed) {
      latest_cycle = cache_state[i].timestamp;
      break;
    }
  }

  for (uint64_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (!cache_state[i].accessed && cache_state[i].timestamp >= latest_cycle) {
      not_accessed_pf_found = true; 
      index = i;
      latest_cycle = cache_state[i].timestamp;
    }
  }

  latest_cycle = cache_state[set * WAY_NUM].timestamp;

  if (!not_accessed_pf_found) { 
    for (uint64_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
      if (cache_state[i].timestamp > latest_cycle) {
        index = i;
        latest_cycle = cache_state[i].timestamp;
      }
    }
  }

  if (ORACLE_DEBUG_PRINT) 
    std::cout << "rollback_prefetch addr " << addr << " set " << set << " way " << (index - set * WAY_NUM) << std::endl; 

  assert(index < ((set + 1) * WAY_NUM));

  return index;
}

void spp_l3::SPP_ORACLE::kill_simulation() {
  file_write();
  std::cout << "Updating address and hit/miss record complete in LLC" << std::endl;
  ORACLE_ACTIVE = false;
  exit(0);
}

void spp_l3::SPP_ORACLE::finish() {

  if (!RECORD_OR_REPLAY) {
    rec_file.close();
    std::cout << "Hits in runahead prefetch list: " << runahead_hits << std::endl;
    std::cout << "Hits in MSHR " << MSHR_hits << std::endl;
    std::cout << "Hits in inflight_writes" << inflight_write_hits << std::endl;
    std::cout << "Hits in internal_PQ " << internal_PQ_hits << std::endl;
    std::cout << "Hits in ready to issue prefetch queue " << cs_q_hits << std::endl;
    std::cout << "Hits in oracle_pf " << oracle_pf_hits << std::endl;
    std::cout << "Unhandled misses not replaced " << unhandled_misses_not_replaced << std::endl;
    std::cout << "Unhandled misses replaced " << unhandled_misses_replaced << std::endl;
    std::cout << "Unhandled non-write misses not filled " << unhandled_non_write_misses_not_filled << std::endl;
    std::cout << "Unhandled write misses not filled " << unhandled_write_misses_not_filled << std::endl;
    std::cout << "New misses recorded: " << new_misses << std::endl;
    file_write();
  } 
  else {
    std::cout << "Last round write" << std::endl;
    file_write();
  }
}
