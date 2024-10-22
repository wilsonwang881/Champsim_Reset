#ifndef _PREFETCH_FILTER
#include <cstdint>
#include <cstddef>
#include <map>
#include <numeric>
#include <limits>
#include <iostream>
#include <deque>
#include <algorithm>
#include <vector>

namespace spp
{
//uint64_t get_hash(uint64_t key);

// Prefetch filter parameters
#define QUOTIENT_BIT  10
#define REMAINDER_BIT 6
#define HASH_BIT (QUOTIENT_BIT + REMAINDER_BIT + 1)
#define FILTER_SET (1 << QUOTIENT_BIT)

#define QUOTIENT_BIT_REJ  10
#define REMAINDER_BIT_REJ 8
#define HASH_BIT_REJ (QUOTIENT_BIT_REJ + REMAINDER_BIT_REJ + 1)
#define FILTER_SET_REJ (1 << QUOTIENT_BIT_REJ)

// Global register parameters
#define GLOBAL_COUNTER_BIT 10
#define GLOBAL_COUNTER_MAX ((1 << GLOBAL_COUNTER_BIT) - 1) 
#define MAX_GHR_ENTRY 8
#define PAGES_TRACKED 6

// Perceptron paramaters
#define PERC_ENTRIES 4096 //Upto 12-bit addressing in hashed perceptron
#define PERC_FEATURES 4 //Keep increasing based on new features
#define PERC_COUNTER_MAX 15 //-16 to +15: 5 bits counter 
#define PERC_THRESHOLD_HI  -5
#define PERC_THRESHOLD_LO  -15
#define POS_UPDT_THRESHOLD  90
#define NEG_UPDT_THRESHOLD -80

//enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT}; // Request type for prefetch filter

class PREFETCH_FILTER {

  //enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT};
  public:
    enum FILTER_REQUEST {SPP_L2C_PREFETCH, SPP_LLC_PREFETCH, L2C_DEMAND, L2C_EVICT, SPP_PERC_REJECT};
    uint64_t remainder_tag[FILTER_SET],
             pc[FILTER_SET],
             page_number[FILTER_SET],
             block_number[FILTER_SET],
             address[FILTER_SET];
    bool     valid[FILTER_SET],  // Consider this as "prefetched"
             useful[FILTER_SET]; // Consider this as "used"
    int32_t  delta[FILTER_SET],
             perc_sum[FILTER_SET];
    //uint32_t last_signature[FILTER_SET],
             //confidence[FILTER_SET],
             //cur_signature[FILTER_SET],
             //la_depth[FILTER_SET];

    uint64_t remainder_tag_reject[FILTER_SET_REJ],
             pc_reject[FILTER_SET_REJ],
             //pc_1_reject[FILTER_SET_REJ],
             //pc_2_reject[FILTER_SET_REJ],
             //pc_3_reject[FILTER_SET_REJ],
             address_reject[FILTER_SET_REJ],
             page_number_reject[FILTER_SET],
             block_number_reject[FILTER_SET];
    bool     valid_reject[FILTER_SET_REJ]; // Entries which the perceptron rejected
    int32_t  delta_reject[FILTER_SET_REJ],
             perc_sum_reject[FILTER_SET_REJ];
    //uint32_t last_signature_reject[FILTER_SET_REJ],
             //confidence_reject[FILTER_SET_REJ],
             //cur_signature_reject[FILTER_SET_REJ],
             //la_depth_reject[FILTER_SET_REJ];

    // Tried the set-dueling idea which din't work out
    uint32_t PSEL_1;
    uint32_t PSEL_2;

    // To enable / disable negative training using reject filter
    // Set to 1 in the prefetcher file
    bool train_neg;

    float hist_hits[55];
    float hist_tots[55];

    //PERCEPTRON

      // Perc Weights
      int32_t perc_weights[PERC_ENTRIES][PERC_FEATURES];

      // Only for dumping csv
      bool    perc_touched[PERC_ENTRIES][PERC_FEATURES];

      // CONST depths for different features
      int32_t PERC_DEPTH[PERC_FEATURES];


    PREFETCH_FILTER() {
      //cout << endl << "Initialize PREFETCH FILTER" << endl;
      //cout << "FILTER_SET: " << FILTER_SET << endl;

      for (int i = 0; i < 55; i++) {
        hist_hits[i] = 0;
        hist_tots[i] = 0;
      }
      for (uint32_t set = 0; set < FILTER_SET; set++) {
        remainder_tag[set] = 0;
        valid[set] = 0;
        useful[set] = 0;
      }
      for (uint32_t set = 0; set < FILTER_SET_REJ; set++) {
        valid_reject[set] = 0;
        remainder_tag_reject[set] = 0;
      }
      train_neg = 0;

      //Initlaization of Perceptron
      PERC_DEPTH[0] = 2048;   //page #;
      PERC_DEPTH[1] = 4096;   //block #;
      PERC_DEPTH[2] = 4096;   //page#^block#;
      PERC_DEPTH[3] = 1024;   //addr;

      for (int i = 0; i < PERC_ENTRIES; i++) {
        for (int j = 0;j < PERC_FEATURES; j++) {
          perc_weights[i][j] = 0;
          perc_touched[i][j] = 0;
        }
      }

    }
              
    bool     check(uint64_t pf_addr, uint64_t base_addr, FILTER_REQUEST filter_request, int32_t sum);
    bool     add_to_filter(uint64_t check_addr, uint64_t base_addr, FILTER_REQUEST filter_request,int32_t sum);
    uint64_t get_hash(uint64_t key);

    //Perceptron Part
    void perc_update(uint64_t check_addr, bool direction, int32_t perc_sum);
    int32_t perc_predict(uint64_t check_addr);
    void get_perc_index(uint64_t base_addr, uint64_t perc_set[PERC_FEATURES]);

};                  
/*
class PERCEPTRON {
  public:
    // Perc Weights
    int32_t perc_weights[PERC_ENTRIES][PERC_FEATURES];

    // Only for dumping csv
    bool    perc_touched[PERC_ENTRIES][PERC_FEATURES];

    // CONST depths for different features
    int32_t PERC_DEPTH[PERC_FEATURES];

    PERCEPTRON() {
      //cout << "\nInitialize PERCEPTRON" << endl;
      //cout << "PERC_ENTRIES: " << PERC_ENTRIES << endl;
      //cout << "PERC_FEATURES: " << PERC_FEATURES << endl;

      PERC_DEPTH[0] = 2048;   //page #;
      PERC_DEPTH[1] = 4096;   //block #;
      PERC_DEPTH[2] = 4096;     //page#^block#;
      PERC_DEPTH[3] = 1024;   //addr;

      for (int i = 0; i < PERC_ENTRIES; i++) {
        for (int j = 0;j < PERC_FEATURES; j++) {
          perc_weights[i][j] = 0;
          perc_touched[i][j] = 0;
        }
      }
    }
    void perc_update(uint64_t check_addr, bool direction, int32_t perc_sum);
    int32_t perc_predict(uint64_t check_addr);
    void get_perc_index(uint64_t base_addr, uint64_t perc_set[PERC_FEATURES]);
};
*/
/*
class GLOBAL_REGISTER {
  public:
    // Global counters to calculate global prefetching accuracy
    uint64_t pf_useful,
             pf_issued,
             global_accuracy; // Alpha value in Section III. Equation 3

    // Global History Register (GHR) entries
    uint8_t  valid[MAX_GHR_ENTRY];
    uint32_t sig[MAX_GHR_ENTRY],
             confidence[MAX_GHR_ENTRY],
             offset[MAX_GHR_ENTRY];
    int      delta[MAX_GHR_ENTRY];

    uint64_t ip_0,
             ip_1,
             ip_2,
             ip_3;

    uint64_t page_tracker[PAGES_TRACKED];

    // Stats Collection
    double    depth_val,
              depth_sum,
              depth_num;
    double    pf_total,
              pf_l2c,
              pf_llc,
              pf_l2c_good;
    long      perc_pass,
            perc_reject,
            reject_update;
    // Stats

    GLOBAL_REGISTER() {
      pf_useful = 0;
      pf_issued = 0;
      global_accuracy = 0;
      ip_0 = 0;
      ip_1 = 0;
      ip_2 = 0;
      ip_3 = 0;

      // These are just for stats printing
      depth_val = 0;
      depth_sum = 0;
      depth_num = 0;
      pf_total = 0;
      pf_l2c = 0;
      pf_llc = 0;
      pf_l2c_good = 0;
      perc_pass = 0;
      perc_reject = 0;
      reject_update = 0;

      for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
        valid[i] = 0;
        sig[i] = 0;
        confidence[i] = 0;
        offset[i] = 0;
        delta[i] = 0;
      }
    }

    void update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset, int pf_delta);
    uint32_t check_entry(uint32_t page_offset);
};
*/
}
#endif    