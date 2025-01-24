#include "oracle.h"

void spp::SPP_ORACLE::init() 
{
  if (!ORACLE_ACTIVE) 
    return;

  can_write = true; // Change to false for context switch simulation.
  first_round = true;
  oracle_pf.clear();

  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }
  else
  {
    rec_file_write.open(L2C_PHY_ACC_WRITE_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file_write.close();
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
  }

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++)
  {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0;
    cache_state[i].timestamp = 0;
  }
}

void spp::SPP_ORACLE::update_demand(uint64_t cycle, uint64_t addr, bool hit)
{
  if (!ORACLE_ACTIVE) 
    return;

  if (RECORD_OR_REPLAY && can_write)  
  {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.addr = (addr >> 6) << 6;
    tmpp.miss_or_hit = hit;
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) 
    {
      file_write();
      can_write = false;
    }
  }

  if (!RECORD_OR_REPLAY && can_write) 
  {
    acc_timestamp tmpp;
    tmpp.cycle_demanded = cycle - interval_start_cycle;
    tmpp.addr = (addr >> 6) << 6;
    tmpp.miss_or_hit = hit;
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) 
    {
      file_write();
      can_write = false;
    }
  }
}

void spp::SPP_ORACLE::create_new_entry(uint64_t addr, uint64_t cycle, bool& success, uint64_t& evict_addr)
{
  if (!ORACLE_ACTIVE)
    return; 

  if (!RECORD_OR_REPLAY) 
  {
    success = false;
    addr = (addr >> 6) << 6;
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

    // Find the "way" to update pf/block status.
    size_t i;
    for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
    {
      if (cache_state[i].addr == addr) 
      {
        cache_state[i].timestamp = cycle;
        success = true;
        //std::cout << "Found in cache_state addr " << addr << " set " << set << " way " << (i - set * WAY_NUM) << std::endl;
        break;
      }

      if (cache_state[i].addr == 0 && cache_state[i].pending_accesses == 0) 
      {
        cache_state[i].addr = addr;
        cache_state[i].pending_accesses = -1;
        cache_state[i].timestamp = cycle; 
        //std::cout << "Create new entry addr " << addr << " set " << set << " way " << (i - set * WAY_NUM) << std::endl;
        available_pf--;
        success = true;
        break;  
      } 
    }
    
    /*
    if (!success) 
    {
      std::cout << "Create new entry failed" << std::endl; 

      // Find the victim.
      uint64_t timestamp_comparison = uint64_t(-1);
      size_t max_timestamp_index = 0;
      for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
      {
        std::cout << "set " << set << " way " << (i - set * WAY_NUM) << " max timestamp index " << max_timestamp_index << " comparison " << timestamp_comparison << std::endl;
        if (cache_state[i].timestamp < timestamp_comparison && cache_state[i].pending_accesses > 0) 
        {
          max_timestamp_index = i;
          timestamp_comparison = cache_state[i].timestamp;
        }
      }

      if (timestamp_comparison != uint64_t(-1)) 
      {
        evict_addr = cache_state[max_timestamp_index].addr;

        // Put the pending prefetch back to the queue.
        acc_timestamp tmpp;
        tmpp.cycle_demanded = 0;
        tmpp.addr = cache_state[max_timestamp_index].addr;
        tmpp.miss_or_hit = cache_state[max_timestamp_index].pending_accesses;

        oracle_pf.push_front(tmpp);
        std::cout << "Pushed addr " << oracle_pf.front().addr << " with access " << oracle_pf.front().miss_or_hit << std::endl;

        // Update the cache state.
        cache_state[max_timestamp_index].addr = addr;
        cache_state[max_timestamp_index].pending_accesses = -1;
        cache_state[max_timestamp_index].timestamp = cycle;
        //std::cout << "Updated cache_state at" << std::endl;
      }
    }
    */
  }
}

void spp::SPP_ORACLE::update_fill(uint64_t addr)
{
  if (!ORACLE_ACTIVE)
    return; 

  if (!RECORD_OR_REPLAY) 
  {
    addr = (addr >> 6) << 6;
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

    // Find the "way" to update pf/block status.
    size_t i;
    for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
    {
      if (cache_state[i].addr == addr) 
      {
        //std::cout << "Updating cache state in set " << set << " way " << (i - set * WAY_NUM) << " after updating addr " << cache_state[i].addr << std::endl;
        cache_state[i].addr = 0;
        cache_state[i].pending_accesses = 0;
        cache_state[i].timestamp = 0;
        available_pf++;
        //break;  
      } 
    }
  }
}

void spp::SPP_ORACLE::refresh_cache_state()
{
  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++)
  {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0; 
    cache_state[i].timestamp = 0;
    cache_state[i].require_eviction = true;
  }

  available_pf = SET_NUM * WAY_NUM;
  lru_clearing_addr.clear();
}

void spp::SPP_ORACLE::file_write()
{
  if (!ORACLE_ACTIVE) 
    return;

  if (RECORD_OR_REPLAY && can_write && access.size() > 0) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : access)
        rec_file << var.cycle_demanded << " " << var.addr << " " << var.miss_or_hit << std::endl;

    rec_file << "0 0 0" << std::endl;
    rec_file.close();
    std::cout << "L2C oracle: writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  }
  else if (can_write && access.size() > 0) 
  {
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

  if (!RECORD_OR_REPLAY) 
  {
    uint64_t readin_cycle_demanded, readin_addr, readin_miss_or_hit;

    while(!rec_file.eof())
    {
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

    // Pre-processing accesses.
    for (size_t i = 0; i < oracle_pf.size(); i++) 
    {
      if (((oracle_pf[i].addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM))) == 625) 
        std::cout << "Reading " << oracle_pf[i].addr << " hit? " << oracle_pf[i].miss_or_hit << std::endl;

      uint64_t addr = oracle_pf[i].addr;
      
      if (oracle_pf[i].miss_or_hit == 0) 
      {
        uint64_t accs = 1;

        for (uint64_t j = i + 1; j < oracle_pf.size(); j++) 
        {
          if (oracle_pf[j].addr == addr && !oracle_pf[j].miss_or_hit) 
            break;     

          if (oracle_pf[j].addr == addr && (oracle_pf[j].miss_or_hit == 1)) 
          {
            accs++;     
            to_be_erased.push_back(j);
          } 
        }  

        oracle_pf[i].miss_or_hit = accs;
        oracle_pf[i].require_eviction = true;
      }
    }

    std::sort(to_be_erased.begin(), to_be_erased.end());

    for (int i = to_be_erased.size() - 1; i >= 0; i--)
      oracle_pf.erase(oracle_pf.begin() + to_be_erased[i]);

    for (uint64_t i = 0; i < SET_NUM; i++) 
    {
      uint64_t last_n_ways = 0;

      for (int j = oracle_pf.size() - 1; j >= 0; j--) 
      {
        uint64_t set = (oracle_pf[j].addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM));

        if (set == i && last_n_ways < WAY_NUM) 
        {
          oracle_pf[j].require_eviction = false; 
          last_n_ways++;
        }

        if (last_n_ways == WAY_NUM) 
          break;
      }
    }

    std::cout << "=======================" << std::endl;
    std::fstream parsed_file;
    parsed_file.open("parsed_memory_access.txt", std::ofstream::out | std::ofstream::trunc);
    parsed_file.close();
    parsed_file.open("parsed_memory_access.txt", std::ofstream::app);

    for(auto var : oracle_pf) 
    {
      parsed_file << var.addr << " " << var.miss_or_hit << std::endl;
    }

    parsed_file.close();

    std::cout << "======================" << std::endl;

    for(auto var : oracle_pf) 
    {
      if (((var.addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM))) == 625) 
        std::cout << "Processing :" << var.addr << " hit " << var.miss_or_hit << " times " << std::endl;
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
  //uint64_t availability = 0;

  /*
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if (cache_state[i].addr == 0 && cache_state[i].pending_accesses == 0) 
      availability++;  
  }

  if (availability <= 3) 
    return WAY_NUM; 
    */
 
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if ((cache_state[i].addr != 0 && cache_state[i].pending_accesses == 0) || (cache_state[i].addr == 0 && cache_state[i].pending_accesses != 0))
    {
      //std::cout << "set = " << set << " way = " << i - set * WAY_NUM << " addr = " << cache_state[i].addr << " pending_accesses = " << cache_state[i].pending_accesses << std::endl;
      assert(false); 
    }

    if (cache_state[i].addr == addr) 
    {
      res = (set + 1) * WAY_NUM;
      break;
    }

    if (cache_state[i].pending_accesses == 0 && cache_state[i].addr == 0)
    {
      res = i; 
      break;
    }
  }

  return res - set * WAY_NUM;
}

int spp::SPP_ORACLE::check_pf_status(uint64_t addr)
{
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

  // Find the "way" to update pf/block status.
  size_t i;
  addr = (addr >> 6) << 6;
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if (cache_state[i].addr == addr) 
      break;  
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else {
    return -1;
  }
}

int spp::SPP_ORACLE::update_pf_avail(uint64_t addr, uint64_t cycle, bool& evict)
{
  if (RECORD_OR_REPLAY)
    return 1; 

  addr = (addr >> 6) << 6;
  uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

  // Find the "way" to update pf/block status.
  size_t i;
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if (cache_state[i].addr == addr) 
    {
      cache_state[i].pending_accesses--;
      std::cout << "Accessed addr = " << addr << " at set " << set << " way " << i - set * WAY_NUM << " remaining accesses " << cache_state[i].pending_accesses << std::endl;

      cache_state[i].timestamp = cycle; 

      if (cache_state[i].pending_accesses == 0)
      {
        cache_state[i].addr = 0; 
        cache_state[i].timestamp = 0;
        evict = cache_state[i].require_eviction;
      }

      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else
    return -1;
}

uint64_t spp::SPP_ORACLE::poll(uint64_t cycle)
{
  uint64_t target = 0;

  if (oracle_pf.empty()) // || available_pf <= 0)
    return target; 

  std::deque<uint64_t> to_be_erased;
  uint64_t set, way;

  std::unordered_set<uint64_t> checked_set;

  // Find the address to be prefetched.
  for(size_t i = 0; i < oracle_pf.size();i++) // oracle_pf.size() 
  {
    //assert(oracle_pf[i].addr > 0);
    set = (oracle_pf[i].addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
    way = check_set_pf_avail(oracle_pf[i].addr);

    uint64_t prev_checked_set_size = checked_set.size();
    checked_set.insert(way);

    if ((way < WAY_NUM) && (checked_set.size() > prev_checked_set_size))// && set_blk_availability[set] > 0) 
    {
      target = oracle_pf[i].addr;
      cache_state[set * WAY_NUM + way].addr = target;
      cache_state[set * WAY_NUM + way].pending_accesses = static_cast<int>(oracle_pf[i].miss_or_hit);
      cache_state[set * WAY_NUM + way].timestamp = cycle;
      cache_state[set * WAY_NUM + way].require_eviction = oracle_pf[i].require_eviction;

      //std::cout << "PF: addr = " << cache_state[set * WAY_NUM + way].addr << " set " << set << " way " << way << " accesses = " << cache_state[set * WAY_NUM + way].pending_accesses << " at cycle " << cycle - interval_start_cycle << std::endl;
      to_be_erased.push_back(i);
      available_pf--;
      break;
    }

    if (checked_set.size() == (WAY_NUM * SET_NUM)) 
      break; 
  }

  for (int j = to_be_erased.size() - 1; j >= 0; j--) 
    oracle_pf.erase(oracle_pf.begin() + to_be_erased[j]); 

  if ((oracle_pf.size() % 5000) == 0) 
    std::cout << "Remaing oracle access = " << oracle_pf.size() << std::endl;

  return target;
}

void spp::SPP_ORACLE::finish()
{
  if (!ORACLE_ACTIVE) 
    return;

  if (!RECORD_OR_REPLAY)
    rec_file.close();
  else
  {
    can_write = true;
    std::cout << "Last round write" << std::endl;
    file_write();
  }
}
