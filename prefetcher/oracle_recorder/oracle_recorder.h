#ifndef ORACLE_RECORDER
#define ORACLE_RECORDER

#include <unordered_set>
#include <map>
#include <cassert>
#include <string>
#include <set>
#include <fstream>
#include <deque>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 64
#define NUMBER_OF_PREFETCH_UNIT 400
#define OBSERVATION_WINDOW 4000
#define RECORD_NON_UNIQ_ACCESS 1
#define NON_UNIQ_IP_SIZE 500
#define NON_UNIQ_DATA_SIZE 4000

class CACHE;

namespace oracle_recorder 
{
  class prefetcher
  {
    /*
    std::unordered_set<uint64_t> uniq_prefetch_address;
    std::ofstream L1D_access_file;
    uint64_t data_size = 0;

    std::deque<reset_misc::access> non_uniq_ip;
    std::deque<reset_misc::access> non_uniq_data;
    bool after_cs_ip_recorded = true;
    bool after_cs_data_recorded = true;
    bool cs_moment = false;
    bool after_cs_moment = false;
    bool previous_can_record = false;
    std::set<uint64_t> ip_duplicate_check, data_duplicate_check;

    public:

    void initialize_record_file();
    void duplicate_check(std::deque<reset_misc::access> &non_uniq_dq, std::set<uint64_t> &check_set, uint64_t limit);
    void record_non_uniq(uint64_t addr, std::deque<reset_misc::access> &non_uniq_dq, std::set<uint64_t> &duplicate_set, uint64_t current_cycle, uint64_t limit);
    void write_to_file(std::string file_name, std::deque<reset_misc::access> dq, std::string seperator);
    */
  };
}

#endif 

