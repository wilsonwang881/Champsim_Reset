#include "oracle.h"

void spp_l3::SPP_ORACLE::init() {
  can_write = true; // Change to false for context switch simulation.

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
  }

  available_pf = SET_NUM * WAY_NUM - 16;

  for(uint64_t i = 0; i < SET_NUM; i++) {
    set_availability[i];
    set_availability[i] = WAY_NUM;
  }

  file_read();
}

void spp_l3::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit, bool replay) {

  if (!RECORD_OR_REPLAY && can_write) {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.miss_or_hit = hit;
    uint64_t set_check = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

    if (!hit && replay) {
      tmpp.cycle_demanded = cycle;    
      uint64_t way_check = check_set_pf_avail(addr);   

      if (way_check == WAY_NUM) {
        set_kill_counter[set_check].insert((addr >> 6) << 6);

        if (set_kill_counter[set_check].size() >= WAY_NUM) {
          std::cout << "Simulation killed at a with set " << set_check << " way " << way_check << std::endl;
          kill_simulation(cycle, addr, hit);
        }
      }
      else if(way_check < WAY_NUM && cache_state[set_check * WAY_NUM + way_check].addr != ((addr >> 6) << 6)) {
        assert(cache_state[set_check * WAY_NUM + way_check].addr == 0);

        set_kill_counter[set_check].insert((addr >> 6) << 6);

        if (set_kill_counter[set_check].size() >= WAY_NUM) {
          std::cout << "Simulation killed at b with set " << set_check << " way " << way_check << std::endl;
          kill_simulation(cycle, addr, hit);
        }
      }      
      else if(way_check < WAY_NUM && cache_state[set_check * WAY_NUM + way_check].addr == ((addr >> 6) << 6)){
        cache_state[set_check * WAY_NUM + way_check].pending_accesses = 1;
        update_pf_avail(addr, cycle);
      }
      else
        assert(false);
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
      rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

    rec_file << "0 0 0" << std::endl;
    rec_file.close();
    std::cout << "L3C oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  } 
}

void spp_l3::SPP_ORACLE::file_read() {
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
    std::cout << "L3C oracle: read " << readin.size() << " accesses from file." << std::endl;
    std::deque<uint64_t> to_be_erased;

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
    
    std::cout << "L3C oracle: pre-processing collects " << oracle_pf.size() << " accesses from file read." << std::endl;
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
      //std::cout << "Accessed addr = " << addr << " at set " << set << " way " << i - set * WAY_NUM << " remaining accesses " << cache_state[i].pending_accesses << std::endl;
      cache_state[i].timestamp = cycle; 

      if (cache_state[i].pending_accesses == 0) {
        cache_state[i].addr = 0; 
        cache_state[i].timestamp = 0;
        available_pf++;
        available_pf = std::min(available_pf, (uint64_t)(WAY_NUM * SET_NUM - 16));
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

bool spp_l3::SPP_ORACLE::check_require_eviction(uint64_t addr) {

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

std::tuple<uint64_t, uint64_t, bool> spp_l3::SPP_ORACLE::poll(uint64_t mode, uint64_t cycle) {

  std::tuple<uint64_t, uint64_t, bool> target = std::make_tuple(0, 0, 0);

  if (oracle_pf.empty()) 
    return target; 

  uint64_t way, set_vacancy;

  // Find the address to be prefetched.
  auto ite = oracle_pf.begin();
  bool erase = false;
  uint64_t set = ite->set; 
  set_vacancy = set_availability[set];

  if (set_vacancy > 0 && mode == 1) {
    way = check_set_pf_avail(ite->addr);
    
    if ((way < WAY_NUM && cache_state[set * WAY_NUM + way].addr != ite->addr) || (way == WAY_NUM && (ite->cycle_demanded - cycle) < 50)) {
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
    }    
  }
  else if (mode == 2) {

    while (ite != oracle_pf.end()) {
      set = ite->set;

      if (set_availability[set] > 0) {
        way = check_set_pf_avail(ite->addr);

        if ((way < WAY_NUM) && (cache_state[set * WAY_NUM + way].addr != ite->addr)) {
          cache_state[set * WAY_NUM + way].pending_accesses = (int)(ite->miss_or_hit);
          std::get<0>(target) = ite->addr;
          std::get<1>(target) = ite->cycle_demanded;
          std::get<2>(target) = true;
          cache_state[set * WAY_NUM + way].addr = ite->addr;
          cache_state[set * WAY_NUM + way].require_eviction = ite->require_eviction;
          //std::cout << "Runahead PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " require_eviction " << cache_state[set * WAY_NUM + way].require_eviction << std::endl;
          set_availability[set]--;
          available_pf--;
          hit_address = 0;
          erase = true;
          break;
        }
      }

      ite++;
    }
  }

  if (std::get<0>(target) != 0 && ite != oracle_pf.end() && erase)  
    ite = oracle_pf.erase(ite); 

  if ((oracle_pf.size() % 10000) == 0) {
      std::cout << "LLC: remaining oracle access = " << oracle_pf.size() - pf_issued << std::endl;
      oracle_pf.shrink_to_fit();
  }

  return target;
}

void spp_l3::SPP_ORACLE::kill_simulation(uint64_t cycle, uint64_t addr, bool hit) {

  // Check if there is vacancy in the cache record.
  file_write();
  std::cout << "Updating address and hit/miss record complete in LLC" << std::endl;
  done = true;
  ORACLE_ACTIVE = false;
  exit(0);
}

void spp_l3::SPP_ORACLE::finish() {

  if (!RECORD_OR_REPLAY) {
    rec_file.close();
    can_write = true;
    std::cout << "Last round write in replaying mode" << std::endl;
    std::cout << "Hits in MSHR: " << (unsigned)hit_in_MSHR << std::endl;
    file_write();
  } else {
    can_write = true;
    std::cout << "Last round write" << std::endl;
    file_write();
  }
}
