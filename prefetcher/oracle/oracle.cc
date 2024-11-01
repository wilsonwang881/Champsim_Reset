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

void oracle::prefetcher::update(uint64_t cycle, uint64_t addr)
{
  if (RECORD_OR_REPLAY && can_write) 
  {
    acc_timestamp tmpp;
    tmpp.cycle_diff = cycle - interval_start_cycle;
    tmpp.addr = (addr >> 6) << 6;
    access.push_back(tmpp); 

    if (access.size() >= ACCESS_LEN) 
    {
      file_write();
      can_write = false;
    }
  }
}

void oracle::prefetcher::check_progress(uint64_t cycle, uint64_t addr)
{
  if (!RECORD_OR_REPLAY) 
  {
    addr = (addr >> 6) << 6;
    size_t j;
    bool found = false;
    size_t q_size = progress_q.size();
    size_t limit = 200;
    size_t queue_lookup_limit = std::min(q_size, q_size);

    for (size_t i = 0; i < queue_lookup_limit; i++) 
    {
      if (progress_q[i].addr == addr && progress_q[i].cycle_diff >= cycle) 
      {
        j = i;
        found = true;
        break;
      }
    }

    if (found)
    {
      cycles_speedup = progress_q[j].cycle_diff - cycle;
      //std::cout << "Found at " << j << " with progress_q size = " << progress_q.size() << " cycle speedup = " << cycles_speedup << std::endl;
      progress_q.erase(progress_q.begin(), progress_q.begin() + j);
    }
  }
}

void oracle::prefetcher::file_write()
{
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

void oracle::prefetcher::file_read()
{
  cs_pf.clear();
  progress_q.clear();

  if (!RECORD_OR_REPLAY) 
  {
    uint64_t readin_cycle_diff, readin_addr;

    while(!rec_file.eof())
    {
      rec_file >> readin_cycle_diff >> readin_addr;

      if (readin_addr == 0)
        break; 

      acc_timestamp tmpp;
      tmpp.cycle_diff = readin_cycle_diff;
      tmpp.addr = readin_addr;

      cs_pf.push_back(tmpp);
      progress_q.push_back(tmpp);
    }

    std::cout << "Read " << cs_pf.size() << " accesses from file." << std::endl;
  }
}

void oracle::prefetcher::finish()
{
  if (!RECORD_OR_REPLAY)
    rec_file.close();
  else
    file_write();
}
