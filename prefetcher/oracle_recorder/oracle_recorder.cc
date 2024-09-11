#include "cache.h"
#include <unordered_set>
#include <map>
#include <cassert>
#include <string>
#include <set>

#define PREFETCH_UNIT_SHIFT 8
#define PREFETCH_UNIT_SIZE 64
#define NUMBER_OF_PREFETCH_UNIT 400
#define OBSERVATION_WINDOW 4000
#define RECORD_NON_UNIQ_ACCESS 1
#define NON_UNIQ_IP_SIZE 500
#define NON_UNIQ_DATA_SIZE 4000

namespace {

  struct tracker {

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
    
    void initialize_record_file()
    {
      if (RECORD_NON_UNIQ_ACCESS) {
        std::cout << "Non-unique access record files cleared." << std::endl;
        std::ofstream non_uniq_ip_file;
        non_uniq_ip_file.open("non_uniq_ip.txt", std::ios::out);
        non_uniq_ip_file.close();

        std::ofstream non_uniq_data_file;
        non_uniq_data_file.open("non_uniq_data.txt", std::ios::out);
        non_uniq_data_file.close();
      }
    }

    void duplicate_check(std::deque<reset_misc::access> &non_uniq_dq, std::set<uint64_t> &check_set, uint64_t limit)
    {
      std::deque<reset_misc::access> non_uniq_dup;

      check_set.clear();

      for (auto it = non_uniq_dq.end() - 1; it > non_uniq_dq.begin(); --it) {
        check_set.insert(it->addr); 

        if (check_set.size() <= limit) {
          non_uniq_dup.push_front(*it);
        }
        else {
          check_set.erase(it->addr);
          break;
        }
      }
      
      non_uniq_dq.clear();

      for(auto var : non_uniq_dup) {
        non_uniq_dq.push_back(var); 
      }
    }

    void record_non_uniq(uint64_t addr, std::deque<reset_misc::access> &non_uniq_dq, std::set<uint64_t> &duplicate_set, uint64_t current_cycle, uint64_t limit)
    {
      reset_misc::access acc;
      acc.cycle = current_cycle;
      acc.addr = (addr >> 6) << 6;
      acc.occurance = 1;

      if (non_uniq_dq.empty()) {
        non_uniq_dq.push_back(acc); 
        duplicate_set.insert(acc.addr);
      }
      else if (addr == non_uniq_dq.back().addr) {
        non_uniq_dq.back().cycle = current_cycle;
        non_uniq_dq.back().occurance++; 
      }
      else {
        non_uniq_dq.push_back(acc);
        duplicate_set.insert(acc.addr);

        if (duplicate_set.size() > limit) {
          duplicate_check(non_uniq_dq, duplicate_set, limit); 
        }
      }
    }

    void write_to_file(std::string file_name, std::deque<reset_misc::access> dq, std::string seperator)
    {
      std::ofstream file;
      file.open(file_name, std::ios_base::app);

      for(auto var : dq) {
        file << var.cycle << " " << var.addr << " " << var.occurance << std::endl; 
      }

      file << "999999 " << seperator << " 999999" << std::endl;
      file.close();

      std::cout << "Writing " << dq.size() << " entries to file " << file_name << " with " << seperator << " flag." << std::endl;
    }

    void update_dq_knn(std::deque<reset_misc::access> source_dq, std::deque<reset_misc::addr_occr> &sink_dq, uint64_t length)
    {
      sink_dq.clear();

      for(auto it = source_dq.end() - 1; it >= source_dq.begin(); --it ) {

        bool found = false;

        for(auto var : sink_dq) {

          if (var.addr == it->addr) {
            found = true;
            var.occr ++;
            break;
          } 
        } 

        if (!found) {
          reset_misc::addr_occr x;
          x.addr = it->addr;
          x.occr = it->occurance;
          x.cycle = it->cycle;
          sink_dq.push_front(x);
        }
      }

      std::cout << "KNN deque updated" << std::endl;
    }

    void record(bool record_before_cs, bool record_after_cs)
    {
      if (record_before_cs) {
        write_to_file("non_uniq_ip.txt", non_uniq_ip, "before");
        write_to_file("non_uniq_data.txt", non_uniq_data, "before");
        update_dq_knn(non_uniq_data, reset_misc::dq_before_knn, NON_UNIQ_DATA_SIZE);
        non_uniq_ip.clear();
        non_uniq_data.clear();
        ip_duplicate_check.clear();
        data_duplicate_check.clear();
      }

      if (record_after_cs) {
        if (ip_duplicate_check.size() >= NON_UNIQ_IP_SIZE && !after_cs_ip_recorded) {
          write_to_file("non_uniq_ip.txt", non_uniq_ip, "after"); 
          after_cs_ip_recorded = true;
        }

        if (data_duplicate_check.size() >= NON_UNIQ_DATA_SIZE && !after_cs_data_recorded) {
          write_to_file("non_uniq_data.txt", non_uniq_data, "after"); 
          update_dq_knn(non_uniq_data, reset_misc::dq_after_knn, NON_UNIQ_DATA_SIZE);
          after_cs_data_recorded = true;
          cs_moment = false;
          after_cs_moment = false;
          reset_misc::can_record_after_access = false;
        }
      }
    }
  };

  std::map<CACHE*, tracker> trackers;
}

    static int cmpfunc (const void*a,const void*b)
    {
      return(*(uint64_t*)a-*(uint64_t*)b);
    }

    std::vector<uint64_t> distinct_page ( uint64_t* arr ,int n) //finding distinct pages for every 1000 points
    {
      std::vector<uint64_t> special_page;
      special_page.clear(); 
      qsort(arr,n,sizeof(uint64_t),cmpfunc);
      for(int i=0;i<n;i++)
      {
        while (i<n-1 && arr[i]==arr[i+1])
        {
          i++;  
        }
        special_page.push_back(arr[i]);
        std::cout<<"The distinct pages are"<<arr[i]<<std::endl;
      }
      return special_page;
      //printf("The number of distinct page is:%d\n",special_page);
    }

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << "->Prefetcher L1D recorder initialized @ " << current_cycle << " cycles." << std::endl;
  
  if (RECORD_NON_UNIQ_ACCESS) {
    ::trackers[this].initialize_record_file();
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  auto &pref = ::trackers[this];

  if ((type == champsim::to_underlying(access_type::WRITE) ||
      type == champsim::to_underlying(access_type::LOAD)) && 
      ip != addr){

    pref.record_non_uniq(ip, pref.non_uniq_ip, pref.ip_duplicate_check, current_cycle, NON_UNIQ_IP_SIZE);
    pref.record_non_uniq(addr, pref.non_uniq_data, pref.data_duplicate_check, current_cycle, NON_UNIQ_DATA_SIZE);

    if (reset_misc::can_record_after_access && !pref.previous_can_record) {
      pref.cs_moment = true;
      pref.after_cs_moment = false;
    }

    if (pref.cs_moment) {
      pref.record(true, false); 
      pref.cs_moment = false;
      pref.after_cs_moment = true;
      pref.after_cs_ip_recorded = false;
      pref.after_cs_data_recorded = false;
    }
    else {

      if (pref.after_cs_moment) {
        pref.record(false, true);
      }
    }
  }

  pref.previous_can_record = reset_misc::can_record_after_access;

  //std::cout<<"The prefetching operation is on"<<std::endl;
  if (type == champsim::to_underlying(access_type::WRITE)) {
    return metadata_in; 
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate()
{
  
}

void CACHE::prefetcher_final_stats() {}


