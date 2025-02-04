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
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
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

void spp::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit) {
  if (!ORACLE_ACTIVE) 
    return;

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
    tmpp.addr = (addr >> 6) << 6;
    tmpp.miss_or_hit = hit;
    access.push_back(tmpp); 

    if (access.size() >= (ACCESS_LEN / 10000)) {
      rec_file_write.open(L2C_PHY_ACC_WRITE_FILE_NAME, std::ofstream::app);

      for(auto var : access)
          rec_file_write << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

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
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit;

    while(!rec_file.eof()) {
      rec_file >> readin_cycle_demanded >> readin_addr >> readin_miss_or_hit;

      tmpp.cycle_demanded = readin_cycle_demanded;
      tmpp.addr = (readin_addr >> 6) << 6;
      tmpp.miss_or_hit = readin_miss_or_hit;

      if (readin_addr == 0)
        break; 

      oracle_pf.push_back(tmpp);
    }

    std::cout << "L2C oracle: read " << oracle_pf.size() << " accesses from file." << std::endl;

    std::deque<uint64_t> to_be_erased;

    // Use the hashmap to gather accesses.
    std::map<uint64_t, std::deque<uint64_t>> parsing;

    for (size_t i = 0; i < oracle_pf.size(); i++) {
      uint64_t addr = oracle_pf[i].addr;

      if (oracle_pf[i].miss_or_hit == 0) {
        // Found in the hashmap already.
        if (auto search = parsing.find(addr); search != parsing.end()) 
          search->second.push_back(1); 
        else {
          parsing[addr];
          parsing[addr].push_back(1);
        }
      } else {
        if (auto search = parsing.find(addr); search != parsing.end()) 
          *(search->second.end()) = *(search->second.end()) + 1;
        else 
          assert(false);
      }
    }

    // Use the hashmap to walk the memory accesses.
    std::deque<acc_timestamp> oracle_pf_tmpp;

    for(uint64_t i = 0; i < oracle_pf.size(); i++) {
      uint64_t addr = oracle_pf.at(i).addr;


      if (oracle_pf.at(i).miss_or_hit == 0) {
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

    oracle_pf.clear();
    oracle_pf.shrink_to_fit();

    for(auto var : oracle_pf_tmpp)
    {
      oracle_pf.push_back(var); 
      oracle_pf.back().cycle_demanded = (oracle_pf.back().addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));
    }

    pf_size = oracle_pf.size();
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
    if ((cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0) || (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0))
      assert(false); 

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
    if (cache_state[i].addr == addr) {
      cache_state[i].timestamp = lru_counter;
      lru_counter++;
      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else {
    return -1;
  }
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
        cache_state[i].timestamp = 0;
      }

      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else
    return -1;
}

uint64_t spp::SPP_ORACLE::poll(uint64_t cycle) {
  uint64_t target = 0;

  if (oracle_pf.empty()) 
    return target; 

  uint64_t set, way, set_vacancy;

  std::unordered_set<uint64_t> checked_set;

  if (oracle_pf.size() > pf_size) {
    std::cout << "size = " << oracle_pf.size() << " actual size = " << pf_size << std::endl;
    assert(false); 
  }
  pf_size = oracle_pf.size();

  // Find the address to be prefetched.
  auto ite = oracle_pf.begin();
  for(ite = oracle_pf.begin(); ite != oracle_pf.end();) {
    uint64_t pf_size_total = oracle_pf.size();
    uint64_t pf_offset = ite - oracle_pf.begin();
    set = ite->cycle_demanded; 
    set_vacancy = set_availability[set];

    uint64_t prev_checked_set_size = checked_set.size();
    checked_set.insert(set);

    if ((set_vacancy > 0) && (checked_set.size() > prev_checked_set_size)) {

      way = check_set_pf_avail(ite->addr);
      target = ite->addr;
      cache_state[set * WAY_NUM + way].addr = target;
      cache_state[set * WAY_NUM + way].pending_accesses = static_cast<int>(ite->miss_or_hit);
      cache_state[set * WAY_NUM + way].timestamp = cycle;

      //std::cout << "PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " at cycle " << cycle - interval_start_cycle << std::endl;
      set_availability[set]--;
      available_pf--;

      if (oracle_pf.size() < 1250000) {
        std::cout << "pf_size_total = " << pf_size_total << std::endl; 
        std::cout << "pf_offset = " << pf_offset << std::endl;
      }
      break;
    }
    else if (checked_set.size() == (WAY_NUM * SET_NUM - 16)) 
      break; 
    else 
      ++ite;
  }

  if (target != 0 && ite < oracle_pf.end()) {
    ite = oracle_pf.erase(ite);
  }
  else if (ite >= oracle_pf.end()) {
    target = 0; 
  }
if (oracle_pf.size() > pf_size) {
  std::cout << "distance = " << ite - oracle_pf.begin() << " size = " << oracle_pf.size() << " actual size = " << pf_size << std::endl;
    assert(false); 
  }
  pf_size = oracle_pf.size();

  if ((oracle_pf.size() % 5000) == 0) {
      std::cout << "Remaining oracle access = " << oracle_pf.size() << std::endl;
      oracle_pf.shrink_to_fit();
  } 

  return target;
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
