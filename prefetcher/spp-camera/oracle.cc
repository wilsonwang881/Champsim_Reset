#include "oracle.h"

void spp::SPP_ORACLE::init() {
  if (!ORACLE_ACTIVE) 
    return;

  can_write = true; // Change to false for context switch simulation.
  first_round = true;

  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  } else {
    rec_file_write.open(L2C_PHY_ACC_WRITE_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file_write.close();
  }

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++) {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0;
    cache_state[i].timestamp = 0;
  }

  lru_counter = 0;
  available_pf = SET_NUM * WAY_NUM - 16;

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i];
    set_availability[i] = WAY_NUM;
  }
}

uint64_t spp::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit, bool replay) {
  uint64_t possible_do_not_fill_addr = 0;

  if (!ORACLE_ACTIVE) 
    return possible_do_not_fill_addr;

  if (RECORD_OR_REPLAY && can_write) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.addr = (addr >> 6) << 6;
    tmpp.miss_or_hit = hit;
    access.push_back(tmpp); 

    if (access.size() >= (ACCESS_LEN / 10000)) {
      rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

      for(auto var : access)
          rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

      rec_file.close();
      std::cout << "L2C oracle: writing " << access.size() << " accesses to file." << std::endl;
      access.clear();
      access.shrink_to_fit();
    }

    if (access.size() >= ACCESS_LEN) {
      file_write();
      can_write = false;
    }
  }

  if (!RECORD_OR_REPLAY && can_write) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.miss_or_hit = hit;

    readin_index++;

    if (!hit && replay) {
      tmpp.cycle_demanded = 0;    
      

      uint64_t way_check = check_set_pf_avail(addr);   

      if (way_check >= WAY_NUM) {
        kill_simulation(cycle, addr, hit);
      }
      else if( way_check < WAY_NUM) {
        uint64_t set_check = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));
        cache_state[set_check * WAY_NUM + way_check].pending_accesses--;

        if (cache_state[set_check * WAY_NUM + way_check].pending_accesses < 0) {
          kill_simulation(cycle, addr, hit);
        }
        
        if (cache_state[set_check * WAY_NUM + way_check].pending_accesses <= 0) {
          cache_state[set_check * WAY_NUM + way_check].pending_accesses = 0;
          cache_state[set_check * WAY_NUM + way_check].addr = 0;
          cache_state[set_check * WAY_NUM + way_check].timestamp = 0;
          available_pf++;
          available_pf = std::min(available_pf, (uint64_t)(1024 * 8 - 16));
          set_availability.find(set_check)->second++;
          possible_do_not_fill_addr = addr;          
        }

        tmpp.miss_or_hit = 1;
      }      
    }

    tmpp.addr = (addr >> 6) << 6;    
    access.push_back(tmpp); 

    if (access.size() >= (ACCESS_LEN / 10000)) {
      rec_file_write.open(L2C_PHY_ACC_WRITE_FILE_NAME, std::ofstream::app);

      for(auto var : access) {
          rec_file_write << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;
      }

      rec_file_write.close();
      std::cout << "L2C oracle: writing " << access.size() << " accesses to write file." << std::endl;
      access.clear();
      access.shrink_to_fit();
    }

    if (access.size() >= ACCESS_LEN) {
      file_write();
      can_write = false;
    }
  }

  return possible_do_not_fill_addr;
}

void spp::SPP_ORACLE::update_fill(uint64_t addr) {
  if (!ORACLE_ACTIVE)
    return; 

  if (!RECORD_OR_REPLAY) {
    addr = (addr >> 6) << 6;
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

    // Find the "way" to update pf/block status.
    size_t i;
    for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
      if (cache_state[i].addr == addr) {
        //std::cout << "Updating cache state in set " << set << " way " << (i - set * WAY_NUM) << " after updating addr " << cache_state[i].addr << std::endl;
        cache_state[i].addr = 0;

        if (available_pf == 0) 
          available_pf = 0; 
        else {
          available_pf++;
          available_pf = std::min(available_pf, WAY_NUM * SET_NUM - 16);
          set_availability.find(set)->second++;
        }

        cache_state[i].pending_accesses = 0;
        cache_state[i].timestamp = 0;
      } 
    }
  }
}

uint64_t spp::SPP_ORACLE::evict_one_way(uint64_t addr) {
  if (!ORACLE_ACTIVE)
    return 0; 

  if (!RECORD_OR_REPLAY) {
    addr = (addr >> 6) << 6;
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

    // Find if there is a free way.
    for (size_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
      if (cache_state[i].pending_accesses == 0)
        return 0; 
    }

    // Find the "way" to update pf/block status.
    uint64_t lru = std::numeric_limits<uint64_t>::max();
    size_t j = 0;
    uint64_t evict_addr = 0;

    for (size_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) {
      if (cache_state[i].timestamp <= lru && cache_state[i].timestamp != 0) {
        j = i;
        lru = cache_state[i].timestamp;
      } 
    }

    cache_state[j].pending_accesses = 0;
    evict_addr = cache_state[j].addr;
    cache_state[j].addr = 0;
    cache_state[j].timestamp = 0;

    available_pf++;
    available_pf = std::min(available_pf, (uint64_t)(1024 * 8 - 16));
    set_availability[set]++;

    return evict_addr; 
  }

  return 0;
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

  if (!ORACLE_ACTIVE) 
    return;

  if (RECORD_OR_REPLAY && can_write && access.size() > 0) {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : access)
        rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

    rec_file << "0 0 0" << std::endl;
    rec_file.close();
    std::cout << "L2C oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  } else if (can_write && access.size() > 0) {
    rec_file_write.open(L2C_PHY_ACC_WRITE_FILE_NAME, std::ofstream::app);

    for(auto var : access)
        rec_file_write << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

    rec_file_write << "0 0 0" << std::endl;
    rec_file_write.close();
    std::cout << "L2C oracle: writing " << access.size() << " accesses to write file." << std::endl;
    access.clear();
  }
}

void spp::SPP_ORACLE::file_read()
{
  oracle_pf.clear();

  if (!ORACLE_ACTIVE) 
    return;

  acc_timestamp tmpp;

  if (!RECORD_OR_REPLAY) {

    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit;

    while(!rec_file.eof()) {
      rec_file >> readin_cycle_demanded >> readin_addr >> readin_miss_or_hit;

      tmpp.cycle_demanded = readin_cycle_demanded;
      tmpp.addr = (readin_addr >> 6) << 6;
      tmpp.miss_or_hit = readin_miss_or_hit;

      if (readin_addr == 0)
        break; 

      readin.push_back(tmpp);
    }

    rec_file.close();

    std::cout << "L2C oracle: read " << readin.size() << " accesses from file." << std::endl;

    std::deque<uint64_t> to_be_erased;

    // Use the hashmap to gather accesses.
    std::map<uint64_t, std::deque<uint64_t>> parsing;

    for (size_t i = 0; i < readin.size(); i++) {
      uint64_t addr = readin.at(i).addr;

      if (readin.at(i).miss_or_hit == 0) {
        // Found in the hashmap already.
        if (auto search = parsing.find(addr); search != parsing.end()) {
           if (readin.at(i).cycle_demanded == 0) {
             search->second.back() = search->second.back() + 1;
           }
           else{
            search->second.push_back(1); 
           }
        }
        else {
          parsing[addr];
          parsing[addr].push_back(1);

          /*
          if (readin.at(i).cycle_demanded == 0)
          {
            readin.at(i).cycle_demanded = 1;
          }
          */
          
        }
      } else {
        if (auto search = parsing.find(addr); search != parsing.end()) {
          search->second.back() = search->second.back() + 1;
        }
        else 
          assert(false);
      }
    }

    // Use the hashmap to walk the memory accesses.
    std::deque<acc_timestamp> oracle_pf_tmpp;

    for(uint64_t i = 0; i < readin.size(); i++) {
      uint64_t addr = readin.at(i).addr;

      if (readin.at(i).miss_or_hit == 0 && readin.at(i).cycle_demanded != 0) { 
        auto search = parsing.find(addr);
        assert(search != parsing.end());
        assert(search->second.size() != 0);

        uint64_t accesses = search->second.front();
        acc_timestamp acc_timestamp_tmpp;
        acc_timestamp_tmpp.addr = addr;
        acc_timestamp_tmpp.miss_or_hit = accesses;
        oracle_pf_tmpp.push_back(acc_timestamp_tmpp);
        search->second.pop_front();

        if (search->second.size() == 0) {
          parsing.erase(search); 
        }
      }  
    }

    for(auto var : oracle_pf_tmpp) {
      oracle_pf.push_back(var); 
      oracle_pf.back().cycle_demanded = (oracle_pf.back().addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));
    }

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

    if ((cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0))
      assert(false); 

    if ((cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0)) {
      std::cout << "pending_access = " << cache_state[i].pending_accesses << std::endl;
      assert(false); 
    }
      

    if (cache_state[i].addr == addr) {
      res = (set + 1) * WAY_NUM; // i;
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
    if (cache_state[i].addr == addr) {
      cache_state[i].timestamp = lru_counter;
      lru_counter++;
      break;  
    } 
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

uint64_t spp::SPP_ORACLE::poll(uint64_t address) {
  uint64_t target = 0;

  if (oracle_pf.empty()) 
    return target; 

  if (address == 0 && initial_fill == 0) {
    return target; 
  }

  uint64_t address_set = (address >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));
  uint64_t way, set_vacancy;
  std::unordered_set<uint64_t> checked_set;

  // Find the address to be prefetched.
  auto ite = oracle_pf.begin();

  while(ite != oracle_pf.end()) {

    uint64_t set = ite->cycle_demanded; 

    if (set == address_set || initial_fill != 0) {
    
      set_vacancy = set_availability[set];

      uint64_t prev_checked_set_size = checked_set.size();
      checked_set.insert(set);

      if ((set_vacancy > 0) && (checked_set.size() > prev_checked_set_size)) {

        way = check_set_pf_avail(ite->addr);
        
        if (way < WAY_NUM) {

          /*
          if (cache_state[set * WAY_NUM + way].addr == address && address != 0) {
            checked_set.erase(set);
            cache_state[set * WAY_NUM + way].pending_accesses += (int)(ite->miss_or_hit);
            ite = oracle_pf.erase(ite); 
            ite--;
          }
          else 
          */ 
          {
            cache_state[set * WAY_NUM + way].pending_accesses = (int)(ite->miss_or_hit);
            target = ite->addr;
            cache_state[set * WAY_NUM + way].addr = target;
            //std::cout << "PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << std::endl;
            set_availability[set]--;
            available_pf--;
            hit_address = 0;

            if (initial_fill > 0)
              initial_fill--;
            
            break;
          }
        }    
      }
    }

    if (checked_set.size() == (SET_NUM - 1)) 
      break; 
 
    ++ite;
  }

  if (target != 0 && ite != oracle_pf.end())  
    ite = oracle_pf.erase(ite); 

  if ((oracle_pf.size() % 10000) == 0) {
      std::cout << "Remaining oracle access = " << oracle_pf.size() - pf_issued << std::endl;
      oracle_pf.shrink_to_fit();
  }

  return target;
}

void spp::SPP_ORACLE::kill_simulation(uint64_t cycle, uint64_t addr, bool hit) {

  uint64_t shifted_addr = (addr >> 6) << 6;
  acc_timestamp to_be_added;
  to_be_added.cycle_demanded = 0;
  to_be_added.addr = shifted_addr;
  to_be_added.miss_or_hit = 0;

  readin.insert(readin.begin() + readin_index, to_be_added);
  std::cout << "Updated address " << shifted_addr << " at index " << readin_index << " with hit/miss " << to_be_added.miss_or_hit << std::endl;
  rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
  rec_file.close();

  rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

  for(auto var : readin) {
      rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;
  }

  rec_file << "0 0 0" << std::endl;

  rec_file.close();
  std::cout << "L2C oracle: writing " << readin.size() << " accesses to write file." << std::endl;

  std::cout << "Updating address and hit/miss record complete" << std::endl;

  acc_timestamp tmpp;
  tmpp.cycle_demanded = cycle - interval_start_cycle;
  tmpp.addr = shifted_addr;
  tmpp.miss_or_hit = hit;
  access.push_back(tmpp);
  file_write();

  assert(false);
}

void spp::SPP_ORACLE::finish() {
  if (!ORACLE_ACTIVE) 
    return;

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
