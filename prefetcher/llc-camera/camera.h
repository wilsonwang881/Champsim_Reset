#ifndef ORACLE_RECORDER
#define ORACLE_RECORDER

#include <unordered_set>
#include <map>
#include <cassert>
#include <string>
#include <set>
#include <fstream>
#include <deque>
#include <algorithm>
#include <iostream>
#include <limits>

class CACHE;

namespace camera 
{
  class prefetcher
  {
    struct pg{
      bool valid;
      uint64_t page_no;
      std::deque<uint8_t> blk;
    };

    public:

    std::deque<pg> acc;
    std::set<uint64_t> cs_pf;
    std::set<uint64_t> issued_cs_pf;
    std::deque<uint64_t> cs_q;
    uint64_t issued;

    void init();
    void acc_operate(uint64_t addr);
  };
}

#endif 

