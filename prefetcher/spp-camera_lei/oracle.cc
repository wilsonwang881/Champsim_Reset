#include "oracle.h"

void spp::SPP_ORACLE::init() 
{
  if (!ORACLE_ACTIVE) 
    return;

  can_write = false;

  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }
  else
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
}

void spp::SPP_ORACLE::update(uint64_t cycle, uint64_t addr)
{
  if (!ORACLE_ACTIVE) 
    return;

  if (RECORD_OR_REPLAY && can_write) 
  {
    acc_timestamp tmpp;
    tmpp.cycle_diff = cycle - interval_start_cycle;
    tmpp.addr = (addr >> 6) << 6;
    int lookup_size = access.size() - 2000;
    int lookup_start = std::max(0, lookup_size);

    bool found = false;

    for (size_t i = lookup_start; i < access.size(); i++) 
    {
      if (access[i].addr == tmpp.addr)
      {
        found = true;
        break;
      }
    }

    if (!found) 
      access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) 
    {
      file_write();
      can_write = false;
    }
  }
}

void spp::SPP_ORACLE::file_write()
{
  if (!ORACLE_ACTIVE) 
    return;

  if (RECORD_OR_REPLAY && can_write) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : access) 
      rec_file << var.cycle_diff << " " << var.addr << std::endl;

    rec_file << 0 << " " << 0 << std::endl;
    rec_file.close();
    std::cout << "Writing " << access.size() << " accesses to file." << std::endl;
    access.clear();
  }
}

std::vector<std::pair<uint64_t, bool>> spp::SPP_ORACLE::file_read()
{
  std::vector<std::pair<uint64_t, bool>> tmpp;

  if (!ORACLE_ACTIVE) 
    return tmpp;

  if (!RECORD_OR_REPLAY) 
  {
    uint64_t readin_cycle_diff, readin_addr;

    while(!rec_file.eof())
    {
      rec_file >> readin_cycle_diff >> readin_addr;

      if (readin_addr == 0)
        break; 

      tmpp.push_back(std::make_pair(readin_addr, 1));
    }

    std::cout << "Read " << tmpp.size() << " accesses from file." << std::endl;
  }

  return tmpp;
}

void spp::SPP_ORACLE::finish()
{
  if (!ORACLE_ACTIVE) 
    return;

  if (!RECORD_OR_REPLAY)
    rec_file.close();
  else
    file_write();
}
