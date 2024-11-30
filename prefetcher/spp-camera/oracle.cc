#include "oracle.h"

void spp::SPP_ORACLE::init() 
{
  if (!ORACLE_ACTIVE) 
    return;

  can_write = false;
  oracle_pf.clear();

  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }
  else
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);

  for (size_t i = 0; i < SET_NUM * WAY_NUM; i++)
  {
    cache_state[i].addr = 0;
    cache_state[i].pending_accesses = 0;
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
}

void spp::SPP_ORACLE::update_fill(uint64_t addr)
{
  if (!ORACLE_ACTIVE)
    return; 

  if (RECORD_OR_REPLAY && can_write) 
  {
    addr = (addr >> 6) << 6;
    uint64_t set = (addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 

    // Find the "way" to update pf/block status.
    size_t i;
    for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i--) 
    {
      if (cache_state[i].addr == addr) 
      {
        cache_state[i].addr = 0;
        cache_state[i].pending_accesses = 0;
        break;  
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
  }
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
      tmpp.addr = readin_addr;
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
      uint64_t addr = oracle_pf[i].addr;
      
      if (oracle_pf[i].miss_or_hit == 0) 
      {
        uint64_t accs = 1;

        for (uint64_t j = i + 1; j < oracle_pf.size(); j++) 
        {
          if (oracle_pf[j].addr == addr && !oracle_pf[j].miss_or_hit) 
            break;     

          if (oracle_pf[j].addr == addr && oracle_pf[j].miss_or_hit) 
          {
            accs++;     
            to_be_erased.push_back(j);
          } 
        }  

        oracle_pf[i].miss_or_hit = accs;
      }
    }

    std::sort(to_be_erased.begin(), to_be_erased.end());

    for (int i = to_be_erased.size() - 1; i >= 0; i--)
    {
      oracle_pf.erase(oracle_pf.begin() + to_be_erased[i]);
    }

    std::cout << "L2C oracle: pre-processing collects " << oracle_pf.size() << " accesses from file read." << std::endl;
  }
}

uint64_t spp::SPP_ORACLE::check_set_pf_avail(uint64_t set)
{
  uint64_t res = 0;

  for (uint64_t i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if (cache_state[i].pending_accesses <= 0)
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
  for (i = set * WAY_NUM; i < (set + 1) * WAY_NUM; i++) 
  {
    if (cache_state[i].addr == addr) 
    {
      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else {
    return 0;
  }
}

int spp::SPP_ORACLE::update_pf_avail(uint64_t addr)
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

      if (cache_state[i].pending_accesses <= 0)
        cache_state[i].addr = 0; 

      break;  
    } 
  }

  if ((i - set * WAY_NUM) < WAY_NUM)
    return cache_state[i].pending_accesses;
  else
    return 0;
}

uint64_t spp::SPP_ORACLE::poll(uint64_t cycle)
{
  uint64_t target = 0;

  if (oracle_pf.empty())
    return target; 

  std::deque<uint64_t> to_be_erased;
  uint64_t set, way;

  // Find the address to be prefetched.
  for(size_t i = 0; i < 20 && i < oracle_pf.size();i++) // oracle_pf.size() 
  {
    set = (oracle_pf[i].addr >> 6) & champsim::bitmask(champsim::lg2(SET_NUM)); 
    way = check_set_pf_avail(set);

    if (way < WAY_NUM) 
    {
      target = oracle_pf[i].addr;
      //to_be_erased.push_back(i);
      cache_state[set * WAY_NUM + way].addr = target;
      cache_state[set * WAY_NUM + way].pending_accesses = static_cast<int>(oracle_pf[i].miss_or_hit);
      std::cout << "PF: addr = " << target << " set = " << set << " way = " << way << " accesses = " << oracle_pf[i].miss_or_hit << " at cycle " << cycle - interval_start_cycle << std::endl;
      to_be_erased.push_back(i);
      break;
    }
  }

  for (int j = to_be_erased.size() - 1; j >= 0; j--) 
    oracle_pf.erase(oracle_pf.begin() + to_be_erased[j]); 

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
