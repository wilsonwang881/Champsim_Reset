#include "oracle.h"

void spp::SPP_ORACLE::init() {
  can_write = true; // Change to false for context switch simulation.
  first_round = true;

  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++) {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0;
    cache_state[i].timestamp = 0;
    cache_state[i].require_eviction = true;
    set_kill_counter[i] = 0;
  }

  available_pf = SET_NUM * WAY_NUM - 16;

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i];
    set_availability[i] = WAY_NUM;
  }
}

uint64_t spp::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit, bool replay) {
  uint64_t possible_do_not_fill_addr = 0;

  if (!RECORD_OR_REPLAY && can_write) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.miss_or_hit = hit;

    uint64_t set_check = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

    if (!hit && replay) {
      tmpp.cycle_demanded = cycle;    
      
      uint64_t way_check = check_set_pf_avail(addr);   

      if (way_check >= WAY_NUM) {

        set_kill_counter[set_check]++;

        if (set_kill_counter[set_check] > 7) {
          std::cout << "Simulation killed at a with set " << set_check << " way " << way_check << std::endl;
          kill_simulation(cycle, addr, hit);
        }
      }
      else if(way_check < WAY_NUM) {
        cache_state[set_check * WAY_NUM + way_check].pending_accesses--;

        if (cache_state[set_check * WAY_NUM + way_check].pending_accesses < 0) {
          set_kill_counter[set_check]++;

          if (set_kill_counter[set_check] > 7) {
            std::cout << "Simulation killed at b with set " << set_check << " way " << way_check << std::endl;
            kill_simulation(cycle, addr, hit);
          }
        }
        
        if (cache_state[set_check * WAY_NUM + way_check].pending_accesses <= 0) {
          cache_state[set_check * WAY_NUM + way_check].pending_accesses = 0;
          cache_state[set_check * WAY_NUM + way_check].addr = 0;
          cache_state[set_check * WAY_NUM + way_check].timestamp = 0;
          available_pf++;
          available_pf = std::min(available_pf, (uint64_t)(1024 * 8 - 16));
          set_availability.find(set_check)->second++;
          //possible_do_not_fill_addr = addr;          
        }

        tmpp.miss_or_hit = hit;
      }      
    }

    tmpp.addr = (addr >> 6) << 6;    
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) {
      file_write();
      can_write = false;
    }
  }

  return possible_do_not_fill_addr;
}

void spp::SPP_ORACLE::refresh_cache_state() {

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++) {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0; 
    cache_state[i].timestamp = 0;
  }

  available_pf = SET_NUM * WAY_NUM - 16;

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i];
    set_availability[i] = WAY_NUM;
  }
}

void spp::SPP_ORACLE::file_write() {

  if (can_write && access.size() > 0) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);

    for(auto var : access)
      rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

    rec_file << "0 0 0" << std::endl;
    rec_file.close();
    std::cout << "L2C oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  } 
}

void spp::SPP_ORACLE::file_read()
{
  oracle_pf.clear();
  acc_timestamp tmpp;

  if (!RECORD_OR_REPLAY) {

    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit;

    while(!rec_file.eof()) {
      rec_file >> readin_cycle_demanded >> readin_addr >> readin_miss_or_hit;
      tmpp.cycle_demanded = readin_cycle_demanded;
      tmpp.addr = (readin_addr >> 6) << 6;
      tmpp.set = (tmpp.addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));   
      tmpp.miss_or_hit = readin_miss_or_hit;
      tmpp.require_eviction = true;

      if (readin_addr == 0)
        break; 

      readin.push_back(tmpp);
    }

    rec_file.close();
    std::cout << "L2C oracle: read " << readin.size() << " accesses from file." << std::endl;
    std::deque<uint64_t> to_be_erased;

    /*
    // Separate accesses into different sets.
    std::array<std::deque<acc_timestamp>, SET_NUM> set_processing;

    for(auto var : readin) 
      set_processing[var.set].push_back(var);

    // Calculate the re-use distance.
    for(auto &set : set_processing) {

      for(uint64_t i = 0; i < set.size(); i++) {

        uint64_t distance = -1;

        for (uint64_t j = i + 1; j < set.size(); j++) {
          if (set[j].addr == set[i].addr) {
            distance = j - i;
            break; 
          }  
        }

        set[i].reuse_distance = distance;
      }
    }

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
          if (set_container.size() < SET_NUM) {

            // Update the set.
            set_container.push_back(set[i]);

            // Set the new block to be a miss.
            set[i].miss_or_hit = 0;
          }
          // The set has no space.
          else {

            // Evict the block with the longest reuse distance.
            uint64_t reuse_distance = set_container[0].reuse_distance;
            uint64_t eviction_candidate = 0;

            for (uint64_t j = 0; j < SET_NUM; j++) {
              if (set_container[j].reuse_distance > reuse_distance) {
                eviction_candidate = j; 
              } 
            }

            // Evict the block.
            set_container.erase(set_container.begin()+eviction_candidate);

            // Set the new block to be a miss.
            set[i].miss_or_hit = 0;
          }
        }
      }
    }

    for(auto &acc : readin) {
      acc.miss_or_hit = set_processing[acc.set].front().miss_or_hit;
      set_processing[acc.set].pop_front();
    }
    */

    /*
    uint64_t x = 0;

    for(auto set : set_processing) {

      std::cout << "set " << x << ": ";

      for(auto acc : set) {
        std::cout << (unsigned)acc.miss_or_hit << " "; 
      } 

      std::cout << std::endl;

      std::cout << "set " << x << ": ";

      for(auto acc : set) {
        std::cout << (signed)acc.reuse_distance << " "; 
      } 

      std::cout << std::endl;

      x++;
    }
    */

    // Use the hashmap to gather accesses.
    std::map<uint64_t, std::deque<uint64_t>> parsing;

    for (size_t i = 0; i < readin.size(); i++) {
      uint64_t addr = readin.at(i).addr;

      if (readin.at(i).miss_or_hit == 0) {
        // Found in the hashmap already.
        if (auto search = parsing.find(addr); search != parsing.end()) {
          search->second.push_back(1); 
        } 
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

        if (eviction_check[set].size() < (WAY_NUM)) {
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
    
    /*
    for(auto var : oracle_pf) {
      if (var.set == 442) {
        std::cout << "addr " << var.addr << " in set " << var.set << " require_eviction " << var.require_eviction << " cycle_demanded " << var.cycle_demanded << std::endl; 
      } 
    }
    */

    std::cout << "L2C oracle: pre-processing collects " << oracle_pf.size() << " accesses from file read." << std::endl;
  }
}

uint64_t spp::SPP_ORACLE::check_set_pf_avail(uint64_t addr) 
{
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  uint64_t res = (set + 1) * WAY_NUM;
  addr = (addr >> 6) << 6;
  uint64_t i;
 
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {

    if (cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0 && cache_state[i].require_eviction)
      assert(false); 

    if (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0 && cache_state[i].require_eviction) {
      std::cout << "pending_access = " << cache_state[i].pending_accesses << std::endl;
      assert(false); 
    }
      

    if (cache_state[i].addr == addr) {
      res = (set + 1) * WAY_NUM;
      break;
    }

    if (cache_state[i].pending_accesses == 0 && cache_state[i].addr == 0) {
      res = i; 
      break;
    }
  }

  return res - set * WAY_NUM;
}

int spp::SPP_ORACLE::check_pf_status(uint64_t addr) {
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

int spp::SPP_ORACLE::update_pf_avail(uint64_t addr, uint64_t cycle) {
  if (RECORD_OR_REPLAY)
    return 1; 

  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

  // Find the "way" to update pf/block status.
  size_t i;
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (cache_state[i].addr == addr) {
      cache_state[i].pending_accesses--;
      //std::cout << "Accessed addr = " << addr << " at set " << set << " way " << i - set * WAY_NUM << " remaining accesses " << cache_state[i].pending_accesses << std::endl;

      cache_state[i].timestamp = cycle; 

      if (cache_state[i].pending_accesses == 0) {
        cache_state[i].addr = 0; 
        cache_state[i].timestamp = 0;
        cache_state[i].require_eviction = true;
        available_pf++;
        available_pf = std::min(available_pf, (uint64_t)(1024 * 8 - 16));
        set_availability.find(set)->second++;
      }

      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else
    return -1;
}

bool spp::SPP_ORACLE::check_require_eviction(uint64_t addr) {

  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
  bool need_eviction = true;

  // Find the "way" to update pf/block status.
  size_t i;
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
    if (cache_state[i].addr == addr) {
      need_eviction = cache_state[i].require_eviction;
      break;  
    } 
  }

  return need_eviction;
}

void spp::SPP_ORACLE::update_persistent_lru_addr(uint64_t addr, bool pop) {

  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

  if (!pop) {
    size_t before_size = persistent_lru_addr[set].size();
    persistent_lru_addr[set].insert(addr);

    if (persistent_lru_addr[set].size() > before_size) {
      set_kill_counter[set]++;
      //std::cout << "Increment persistent lru in set " << set << " addr " << addr << " to " << persistent_lru_addr[set].size() << std::endl;
    }
  } 
  else {
    auto search = persistent_lru_addr[set].find(addr);

    if (search != persistent_lru_addr[set].end()) {
      persistent_lru_addr[set].erase(search);
      set_kill_counter[set]--;
      //std::cout << "Decrement persistent lru in set " << set << " addr " << addr << " to " << persistent_lru_addr[set].size() << std::endl;
    }
  }
}

std::tuple<uint64_t, uint64_t, bool> spp::SPP_ORACLE::poll(uint64_t address) {

  std::tuple<uint64_t, uint64_t, bool> target = std::make_tuple(0, 0, 0);

  if (oracle_pf.empty()) 
    return target; 

  /*
  if (address == 0 && initial_fill == 0) {
    return target; 
  }
  */

  uint64_t address_set = (address >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));
  uint64_t way, set_vacancy;
  std::unordered_set<uint64_t> checked_set;

  // Find the address to be prefetched.
  auto ite = oracle_pf.begin();

  bool erase = false;

  //while((ite != oracle_pf.end()))  //((ite - oracle_pf.begin()) < 100) && 
  {
    uint64_t set = ite->set; 

    //if (set == address_set || initial_fill != 0) 
    {
      set_vacancy = set_availability[set];
      uint64_t prev_checked_set_size = checked_set.size();
      checked_set.insert(set);

      if ((set_vacancy > 0) && (checked_set.size() > prev_checked_set_size)) {

        way = check_set_pf_avail(ite->addr);
        
        if (way < WAY_NUM) {
          cache_state[set * WAY_NUM + way].pending_accesses = (int)(ite->miss_or_hit);
          std::get<0>(target) = ite->addr;
          std::get<1>(target) = ite->cycle_demanded;
          std::get<2>(target) = true;
          cache_state[set * WAY_NUM + way].addr = ite->addr;
          cache_state[set * WAY_NUM + way].require_eviction = ite->require_eviction;
          //std::cout << "PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << std::endl;
          set_availability[set]--;
          available_pf--;
          hit_address = 0;
          erase = true;

          if (initial_fill > 0)
            initial_fill--;
          
          //break;
        }    
      }
      /*
      else {

        if (!ite->pfed_lower_lvl) {
          std::get<0>(target) = ite->addr;
          std::get<1>(target) = ite->cycle_demanded;
          std::get<2>(target) = false;
          ite->pfed_lower_lvl = true;
          //std::cout << "PF lower level: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << std::endl;
          break;
        }
      }
      */
    }

    /*
    if (checked_set.size() == (SET_NUM)) {
      std::cout << "PF set saturated" << std::endl;
      break; 
    } 
    */

    //break;
 
    //++ite;
  }

  if (std::get<0>(target) != 0 && ite != oracle_pf.end() && erase)  
    ite = oracle_pf.erase(ite); 

  /*
  if (std::get<0>(target) == 0) {
    ite = oracle_pf.begin();

    while(((ite - oracle_pf.begin()) < 100) && (ite != oracle_pf.end())) {

      //uint64_t set = ite->set; 

      //set_vacancy = set_availability[set];

      //if (set_vacancy > 0) 
      {

        //way = check_set_pf_avail(ite->addr);
        
        if (!ite->pfed_lower_lvl) {
          std::get<0>(target) = ite->addr;
          std::get<1>(target) = ite->cycle_demanded;
          std::get<2>(target) = false;
          ite->pfed_lower_lvl = true;
          //std::cout << "PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << std::endl;
          hit_address = 0;

          break;
        }    
      }

      ++ite;
    }
  }
*/


  if ((oracle_pf.size() % 10000) == 0) {
      std::cout << "Remaining oracle access = " << oracle_pf.size() - pf_issued << std::endl;
      oracle_pf.shrink_to_fit();
  }

  return target;
}

void spp::SPP_ORACLE::kill_simulation(uint64_t cycle, uint64_t addr, bool hit) {

  // Check if there is vacancy in the cache record.
  file_write();
  std::cout << "Updating address and hit/miss record complete" << std::endl;
  exit(0);
}

void spp::SPP_ORACLE::finish() {

  if (!RECORD_OR_REPLAY) {
    rec_file.close();
    can_write = true;
    std::cout << "Last round write in replaying mode" << std::endl;
    file_write();
  } else {
    can_write = true;
    std::cout << "Last round write" << std::endl;
    file_write();
  }
}
