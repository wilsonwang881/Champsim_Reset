#include "oracle.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, oracle::prefetcher> ORACLE; 
}

void oracle::prefetcher::init() 
{
  // Clear the L2C access file if in recording mode.
  if (RECORD_OR_REPLAY) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::out | std::ofstream::trunc);
    rec_file.close();
  }
  else
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ifstream::in);
}

void oracle::prefetcher::update(uint64_t addr)
{
  if (RECORD_OR_REPLAY && can_write) 
  {
    uint64_t blk_addr = (addr >> 6) << 6; 

    auto set_check = dup_check.insert(blk_addr); 

    if (!set_check.second)
      access.push_back(blk_addr); 

    if (access.size() >= ACCESS_LEN) 
    {
      file_write();
      can_write = false;
    }
  }
}

void oracle::prefetcher::file_write()
{
  if (RECORD_OR_REPLAY) 
  {
    rec_file.open(L2C_PHY_ACC_FILE_NAME, std::ofstream::app);

    for(auto var : access) 
      rec_file << var << std::endl;

    rec_file << 0 << std::endl;
    rec_file.close();
    std::cout << "Writing " << access.size() << " accesses to file." << std::endl;
    dup_check.clear();
    access.clear();
  }
}

void oracle::prefetcher::file_read()
{
  cs_pf.clear();

  if (!RECORD_OR_REPLAY) 
  {
    uint64_t readin;

    while(!rec_file.eof())
    {
      rec_file >> readin;

      if (readin == 0)
        break; 

      cs_pf.push_back(readin);
    }

    std::cout << "Read " << cs_pf.size() << " accesses from file." << std::endl;
  }
}

void oracle::prefetcher::finish()
{
  if (!RECORD_OR_REPLAY)
    rec_file.close();
}
