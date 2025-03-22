#include "oracle.h"

void spp_l3::SPP_ORACLE::init() {
  can_write = true; // Change to false for context switch simulation.

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

  available_pf = SET_NUM * WAY_NUM;

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i];
    set_availability[i] = WAY_NUM;
  }

  file_read();
}

void spp_l3::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit, bool replay, uint64_t type) {

  if (!RECORD_OR_REPLAY && can_write) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.miss_or_hit = hit;
    tmpp.type = type;
    uint64_t set_check = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

    if (!hit && replay) {
      tmpp.cycle_demanded = cycle;    
      uint64_t way_check = check_set_pf_avail(addr);   

      if (way_check == WAY_NUM) {

        if (oracle_pf.empty() && type != 3) {
          set_kill_counter[set_check].insert((addr >> 6) << 6);
          new_misses++;
        }

        if (set_kill_counter[set_check].size() > WAY_NUM) {
          std::cout << "Simulation killed at a with set " << set_check << " way " << way_check << std::endl;
          kill_simulation();
        }
      }
      else if(way_check < WAY_NUM && 
              cache_state[set_check * WAY_NUM + way_check].addr != ((addr >> 6) << 6)) {
        assert(cache_state[set_check * WAY_NUM + way_check].addr == 0);

        if (oracle_pf.empty() && type != 3) {
          set_kill_counter[set_check].insert((addr >> 6) << 6);
          new_misses++;
        }

        if (set_kill_counter[set_check].size() > WAY_NUM) {
          std::cout << "Simulation killed at b with set " << set_check << " way " << way_check << std::endl;
          kill_simulation();
        }
      }      
      else if(way_check < WAY_NUM && 
              cache_state[set_check * WAY_NUM + way_check].addr == ((addr >> 6) << 6))
        update_pf_avail(addr, cycle);
      else {
        std::cout << "Failed: way " << way_check << " set " << set_check << " addr " << ((addr >> 6) << 6) << " cache_state addr " << cache_state[set_check * WAY_NUM + way_check].addr  << std::endl;
        assert(false);
      }
    }

    tmpp.addr = (addr >> 6) << 6;    
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) {
      file_write();
      can_write = false;
    }
  }
}

void spp_l3::SPP_ORACLE::file_write() {

  if (can_write && access.size() > 0) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);

    for(auto var : access)
      rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << " " << (unsigned)var.type << std::endl;

    rec_file << "0 0 0 0" << std::endl;
    rec_file.close();
    std::cout << "Oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  } 
}

void spp_l3::SPP_ORACLE::file_read() {
  oracle_pf.clear();
  acc_timestamp tmpp;

  if (!RECORD_OR_REPLAY) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit, type;

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

      readin.push_back(tmpp);
    }

    rec_file.close();
    std::cout << "Oracle: read " << readin.size() << " accesses from file." << std::endl;

    if (BELADY_CACHE_REPLACEMENT_POLICY_ACTIVE) {
      // Separate accesses into different sets.
      std::array<std::deque<acc_timestamp>, SET_NUM> set_processing;

      for(auto var : readin) 
        set_processing[var.set].push_back(var);

      // Use the optimal cache replacement policy to work out hit/miss for each access.
      for(auto &set : set_processing) {
        std::vector<acc_timestamp> set_container;

        for (uint64_t i = 0; i < set.size(); i++) {
          bool found = false;

          for(auto blk : set_container) {
            if (blk.addr == set[i].addr) {
              found = true;
              break;
            } 
          }

          // The set has the block.
          if (found) 
            set[i].miss_or_hit = 1; 
          // The set does not have the block.
          else {
            // The set has space.
            if (set_container.size() < WAY_NUM) {
              // Update the set.
              set_container.push_back(set[i]);

              // Set the new block to be a miss.
              set[i].miss_or_hit = 0;

              // Safety check.
              assert(set_container.size() <= WAY_NUM);
            }
            // The set has no space.
            else {
              // Calculate the re-use distance.
              for(auto &el : set_container) {
                for(uint64_t j = i + 1; j < set.size(); j++) {
                  uint64_t distance = -1;

                  if (set[j].addr == el.addr) {
                    distance = j - i;
                    break; 
                  }  

                  el.reuse_distance = distance;
                }
              }

              // Evict the block with the longest reuse distance.
              uint64_t reuse_distance = set_container[0].reuse_distance;
              uint64_t eviction_candidate = 0;

              for (uint64_t j = 0; j < WAY_NUM; j++) {
                if (set_container[j].reuse_distance > reuse_distance) 
                  eviction_candidate = j; 
              }

              // Evict the block.
              set_container.erase(set_container.begin()+eviction_candidate);

              // Set the new block to be a miss.
              set[i].miss_or_hit = 0;

              // Update the set.
              set_container.push_back(set[i]);

              // Safety check.
              assert(set_container.size() <= WAY_NUM);
            }
          }
        }
      }

      for(auto &acc : readin) {
        acc.miss_or_hit = set_processing[acc.set].front().miss_or_hit;
        set_processing[acc.set].pop_front();
      }
    }

    // Use the hashmap to gather accesses.
    std::map<uint64_t, std::deque<uint64_t>> parsing;

    for (size_t i = 0; i < readin.size(); i++) {
      uint64_t addr = readin.at(i).addr;

      if (readin.at(i).miss_or_hit == 0) {
        // Found in the hashmap already.
        if (auto search = parsing.find(addr); search != parsing.end()) 
          search->second.push_back(1); 
        else {
          parsing[addr];
          parsing[addr].push_back(1);
        }
      } 
      else {
        if (auto search = parsing.find(addr); search != parsing.end()) 
          search->second.back() = search->second.back() + 1;
        else 
          assert(false);
      }
    }

    // Use the hashmap to walk the memory accesses.
    std::deque<acc_timestamp> oracle_pf_tmpp;

    for(uint64_t i = 0; i < readin.size(); i++) {
      uint64_t addr = readin.at(i).addr;

      if (readin.at(i).miss_or_hit == 0) { 
        auto search = parsing.find(addr);
        assert(search != parsing.end());
        assert(search->second.size() != 0);

        uint64_t accesses = search->second.front();
        acc_timestamp acc_timestamp_tmpp;
        acc_timestamp_tmpp.addr = addr;
        acc_timestamp_tmpp.miss_or_hit = accesses;
        acc_timestamp_tmpp.cycle_demanded = readin.at(i).cycle_demanded;
        acc_timestamp_tmpp.set = readin.at(i).set;
        acc_timestamp_tmpp.type = readin.at(i).type;
        oracle_pf_tmpp.push_back(acc_timestamp_tmpp);
        search->second.pop_front();

        if (search->second.size() == 0)
          parsing.erase(search); 
      }  
    }

    std::map<uint64_t, std::deque<uint64_t>> eviction_check;

    for(auto var : oracle_pf_tmpp) {
      oracle_pf.push_back(var);      
      oracle_pf.back().require_eviction = true;
      oracle_pf.back().pfed_lower_lvl = false;
    }

    for (int i = oracle_pf.size() - 1; i >= 0; i--) {
      uint64_t set = oracle_pf[i].set;
      uint64_t addr = oracle_pf[i].addr;

      if (auto search = eviction_check.find(set); search != eviction_check.end()) {

        if (eviction_check[set].size() < WAY_NUM) {
          eviction_check[set].push_back(addr); 
          oracle_pf[i].require_eviction = false;
        }
      }
      else {
        eviction_check[set];
        eviction_check[set].push_back(addr);
        oracle_pf[i].require_eviction = false;
      }
    }

    uint64_t non_pf_counter = 0;

    for(auto var : oracle_pf) {
      if (var.type == 3) 
        non_pf_counter++; 
    }

    std::cout << "Oracle: pre-processing collects " << oracle_pf.size() << " prefetch targets from file read." << std::endl;
    std::cout << "Oracle: skipping " << non_pf_counter << " prefetch targets because they are WRITE misses." << std::endl;
    std::cout << "Oracle: issuing " << (oracle_pf.size() - non_pf_counter) << " prefetches." << std::endl;
  }
}

uint64_t spp_l3::SPP_ORACLE::check_set_pf_avail(uint64_t addr) {
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  uint64_t res = (set + 1) * WAY_NUM;
  addr = (addr >> 6) << 6;
  uint64_t i;
 
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {

    if (cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0 && cache_state[i].require_eviction)
      assert(false); 

    if (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0 && cache_state[i].require_eviction) {
      std::cout << "pending_access = " << cache_state[i].pending_accesses << " addr " << addr << std::endl;
      assert(false); 
    }
      
    if (cache_state[i].addr == addr || (cache_state[i].pending_accesses == 0 && cache_state[i].addr == 0)) {
      res = i;
      break;
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

  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {

    if (cache_state[i].addr == addr) {
      cache_state[i].pending_accesses--;

      if (DEBUG_PRINT) 
        std::cout << "Accessed addr = " << addr << " at set " << set << " way " << i - set * WAY_NUM << " remaining accesses " << cache_state[i].pending_accesses << std::endl;

      cache_state[i].timestamp = cycle; 

      if (cache_state[i].pending_accesses == 0) {
        cache_state[i].addr = 0; 
        cache_state[i].timestamp = 0;
        available_pf++;
        //assert(available_pf <= (WAY_NUM * SET_NUM));
        set_availability[set]++;
        assert(set_availability[set] <= WAY_NUM);
      }

      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else {
    return -1;
  }
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

std::tuple<uint64_t, uint64_t, bool, bool> spp_l3::SPP_ORACLE::poll() {
  std::tuple<uint64_t, uint64_t, bool, bool> target = std::make_tuple(0, 0, 0, 0);

  if (oracle_pf.empty()) 
    return target; 

  // Find the address to be prefetched.
  uint64_t way;
  auto ite = oracle_pf.begin();
  bool erase = false;
  uint64_t set; 

  while (ite < oracle_pf.end()) {
    set = ite->set;

    if (set_availability[set] > 0) {
      way = check_set_pf_avail(ite->addr);

      if ((way < WAY_NUM) && 
          (cache_state[set * WAY_NUM + way].addr != ite->addr)) {

        std::get<0>(target) = ite->addr;

        if (ite->type == 3) {
          std::get<3>(target) = true;
          //std::get<0>(target) = 0;

          if (DEBUG_PRINT) 
            std::cout << "Skipping addr " << ite->addr << " type " << ite->type << std::endl;
        } 

        std::get<1>(target) = ite->cycle_demanded;
        std::get<2>(target) = true;
        cache_state[set * WAY_NUM + way].pending_accesses = (int)(ite->miss_or_hit);
        cache_state[set * WAY_NUM + way].addr = ite->addr;
        cache_state[set * WAY_NUM + way].require_eviction = ite->require_eviction;
        set_availability[set]--;
        assert(set_availability[set] >= 0);
        available_pf--;
        assert(available_pf >= 0);
        erase = true;

        if (DEBUG_PRINT) 
          std::cout << "Runahead PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << " type " << ite->type << std::endl;

        break;
      }
    }

    ite++;
  }

  if (ite != oracle_pf.end() && erase) 
    ite = oracle_pf.erase(ite); 

  if ((oracle_pf.size() % 10000) == 0 && 
      heartbeat_printed.find(oracle_pf.size()) == heartbeat_printed.end()) {
      std::cout << "Oracle: remaining oracle access = " << oracle_pf.size() - pf_issued << std::endl;
      oracle_pf.shrink_to_fit();
      heartbeat_printed.insert(oracle_pf.size());
  }

  return target;
}

void spp_l3::SPP_ORACLE::kill_simulation() {

  // Check if there is vacancy in the cache record.
  file_write();
  std::cout << "Updating address and hit/miss record complete in LLC" << std::endl;
  ORACLE_ACTIVE = false;
  exit(0);
}

void spp_l3::SPP_ORACLE::finish() {

  if (!RECORD_OR_REPLAY) {
    rec_file.close();
    can_write = true;
    std::cout << "Last round write in replaying mode" << std::endl;
    std::cout << "Hits in MSHR: " << hit_in_MSHR << std::endl;
    std::cout << "New misses recorded: " << new_misses << std::endl;
    file_write();
  } 
  else {
    can_write = true;
    std::cout << "Last round write" << std::endl;
    file_write();
  }
}
