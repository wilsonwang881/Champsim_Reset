#include "oracle.h"

void spp::SPP_ORACLE::init() 
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

void spp::SPP_ORACLE::update(uint64_t addr)
{
  if (RECORD_OR_REPLAY) 
  {
    uint64_t blk_addr = (addr >> 6) << 6; 

    auto set_check = dup_check.insert(blk_addr); 

    if (!set_check.second)
      access.push_back(blk_addr); 

    if (access.size() > ACCESS_LEN) 
      access.pop_front();  
  }
}

void spp::SPP_ORACLE::file_write()
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

std::vector<std::pair<uint64_t, bool>> spp::SPP_ORACLE::file_read()
{
  std::vector<std::pair<uint64_t, bool>> pf;

  if (!RECORD_OR_REPLAY) 
  {
    uint64_t readin;

    while(!rec_file.eof())
    {
      rec_file >> readin;

      if (readin == 0)
        break; 

      pf.push_back(std::make_pair(readin, true));
    }

    std::cout << "Read " << pf.size() << " accesses from file." << std::endl;
  }

  return pf;
}

void spp::SPP_ORACLE::finish()
{
  if (!RECORD_OR_REPLAY)
    rec_file.close();
}
