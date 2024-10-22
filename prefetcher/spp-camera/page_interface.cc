#include <iostream>     // std::cout, std::endl
#include <iomanip>      // std::setw
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <cmath>
#include "cache.h"
#include "page_interface.h"

// TODO: Find a good 64-bit hash function

uint64_t spp::PREFETCH_FILTER::get_hash(uint64_t key)
{
  // Robert Jenkins' 32 bit mix function
  key += (key << 12);
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);

  // Knuth's multiplicative method
  key = (key >> 3) * 2654435761;

  return key;
}

bool spp::PREFETCH_FILTER::check(uint64_t check_addr, uint64_t base_addr, FILTER_REQUEST filter_request, int32_t sum)
{ 

  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = get_hash(cache_line);

  //MAIN FILTER
  uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  //REJECT FILTER
  uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
           remainder_reject = hash % (1 << REMAINDER_BIT_REJ);
  /*         
  SPP_DP (
      cout << "[FILTER] check_addr: " << hex << check_addr << " check_cache_line: " << (check_addr >> LOG2_BLOCK_SIZE);
      cout << " request type: " << filter_request;
      cout << " hash: " << hash << dec << " quotient: " << quotient << " remainder: " << remainder << endl;
      );
  */
  switch (filter_request) {

    case SPP_PERC_REJECT: // To see what would have been the prediction given perceptron has rejected the PF
      if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
        // We want to check if the prefetch would have gone through had perc not rejected
        // So even in perc reject case, I'm checking in the accept filter for redundancy
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
            );
            */
        return false; // False return indicates "Do not prefetch"
      } else {
        if (train_neg) {
          valid_reject[quotient_reject] = 1;
          remainder_tag_reject[quotient_reject] = remainder_reject;

          // Logging perc features
          address_reject[quotient_reject] = base_addr;
          //pc_reject[quotient_reject] = ip;
          //pc_1_reject[quotient_reject] = GHR.ip_1;
          //pc_2_reject[quotient_reject] = GHR.ip_2;
          //pc_3_reject[quotient_reject] = GHR.ip_3;
          //delta_reject[quotient_reject] = cur_delta;
          perc_sum_reject[quotient_reject] = sum;
          page_number_reject[quotient_reject]=base_addr>>12;;
          block_number_reject[quotient_reject]=(base_addr & 0xFFF) >> 6;
          //last_signature_reject[quotient_reject] = last_sig;
          //cur_signature_reject[quotient_reject] = curr_sig;
          //confidence_reject[quotient_reject] = conf;
          //la_depth_reject[quotient_reject] = depth;
        }
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " PF rejected by perceptron. Set valid_reject for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag_reject[quotient_reject] << endl; 
            cout << " More Recorded Metadata: Addr: " << hex << address_reject[quotient_reject] << dec << " PC: " << pc_reject[quotient_reject] << " Delta: " << delta_reject[quotient_reject] << " Last Signature: " << last_signature_reject[quotient_reject] << " Current Signature: " << cur_signature_reject[quotient_reject] << " Confidence: " << confidence_reject[quotient_reject] << endl;
            );
            */
      }
      break;

    case SPP_L2C_PREFETCH:
      if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
            );*/

        return false; // False return indicates "Do not prefetch"
      }
      // else {

      //valid[quotient] = 1;  // Mark as prefetched
      //useful[quotient] = 0; // Reset useful bit
      //remainder_tag[quotient] = remainder;

      //		// Logging perc features
      //		delta[quotient] = cur_delta;
      //		pc[quotient] = ip;
      //		pc_1[quotient] = GHR.ip_1;
      //		pc_2[quotient] = GHR.ip_2;
      //		pc_3[quotient] = GHR.ip_3;
      //		last_signature[quotient] = last_sig; 
      //		cur_signature[quotient] = curr_sig;
      //		confidence[quotient] = conf;
      //		address[quotient] = base_addr; 
      //		perc_sum[quotient] = sum;
      //		la_depth[quotient] = depth;
      //		
      //		SPP_DP (
      //    cout << "[FILTER] " << __func__ << " set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
      //    cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag[quotient] << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
      //			cout << " More Recorded Metadata: Addr:" << hex << address[quotient] << dec << " PC: " << pc[quotient] << " Delta: " << delta[quotient] << " Last Signature: " << last_signature[quotient] << " Current Signature: " << cur_signature[quotient] << " Confidence: " << confidence[quotient] << endl;
      //);
      //}
      break;

    case SPP_LLC_PREFETCH:
      if ((valid[quotient] || useful[quotient]) && remainder_tag[quotient] == remainder) { 
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " line is already in the filter check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
            );
        */
        return false; // False return indicates "Do not prefetch"
      } else {
        // NOTE: SPP_LLC_PREFETCH has relatively low confidence 
        // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
        // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
        // we can get this cache line immediately from the LLC (not from DRAM)
        // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " don't set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
            );
          */
      }
      break;

    case L2C_DEMAND:
      if ((remainder_tag[quotient] == remainder) && (useful[quotient] == 0)) {
        useful[quotient] = 1;
        /*
        if (valid[quotient]) {
          GHR.pf_useful++; // This cache line was prefetched by SPP and actually used in the program
          // For stats
          GHR.pf_l2c_good++;
        }
        */
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " set useful for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient];
            cout << " GHR.pf_issued: " << GHR.pf_issued << " GHR.pf_useful: " << GHR.pf_useful << endl; 
            if (valid[quotient])
            cout << " Calling Perceptron Update (INC) as L2C_DEMAND was useful" << endl;
            );
        */
        if (valid[quotient]) {
          // Prefetch leads to a demand hit
          perc_update(address[quotient],1, perc_sum[quotient]);
                           
          // Histogramming Idea
          int32_t perc_sum_shifted = perc_sum[quotient] + (PERC_COUNTER_MAX+1)*PERC_FEATURES; 
          int32_t hist_index = perc_sum_shifted / 10;
          hist_hits[hist_index]++;
        }
      }
      //If NOT Prefetched
      if (!(valid[quotient] && remainder_tag[quotient] == remainder)) {
        // AND If Rejected by Perc
        if (valid_reject[quotient_reject] && remainder_tag_reject[quotient_reject] == remainder_reject) {
          /*
          SPP_DP (
              cout << "[FILTER] " << __func__ << " not doing anything for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
              cout << " quotient: " << quotient << " valid_reject:" << valid_reject[quotient_reject];
              cout << " GHR.pf_issued: " << GHR.pf_issued << " GHR.pf_useful: " << GHR.pf_useful << endl; 
              cout << " Calling Perceptron Update (DEC) as a useful L2C_DEMAND was rejected and reseting valid_reject" << endl;
              );
          */
          if (train_neg) {
            // Not prefetched but could have been a good idea to prefetch
            perc_update(address_reject[quotient_reject],0, perc_sum_reject[quotient_reject]);
            valid_reject[quotient_reject] = 0;
            remainder_tag_reject[quotient_reject] = 0;
            // Printing Stats
            //GHR.reject_update++;
          }
        }
      }
      break;

    case L2C_EVICT:
      // Decrease global pf_useful counter when there is a useless prefetch (prefetched but not used)
      if (valid[quotient] && !useful[quotient]) {
        //if (GHR.pf_useful) 
          //GHR.pf_useful--;
        /*
        SPP_DP (
            cout << "[FILTER] " << __func__ << " eviction for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
            cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
            cout << " Calling Perceptron Update (DEC) as L2C_DEMAND was not useful" << endl;
            cout << " Reseting valid_reject" << endl;
            );
        */
        // Prefetch leads to eviction
        perc_update(address[quotient],0, perc_sum[quotient]);
      }
      // Reset filter entry
      valid[quotient] = 0;
      useful[quotient] = 0;
      remainder_tag[quotient] = 0;

      // Reset reject filter too
      valid_reject[quotient_reject] = 0;
      remainder_tag_reject[quotient_reject] = 0;

      break;

    default:
      std::cout<<"Hello"<<std::endl;
      // Assertion
      //cout << "[FILTER] Invalid filter request type: " << filter_request << endl;
      //assert(0);
  }

  return true;
}


bool spp::PREFETCH_FILTER::add_to_filter(uint64_t check_addr, uint64_t base_addr, FILTER_REQUEST filter_request,int32_t sum)
{

  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = get_hash(cache_line);

  //MAIN FILTER
  uint64_t quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  //REJECT FILTER
  uint64_t quotient_reject = (hash >> REMAINDER_BIT_REJ) & ((1 << QUOTIENT_BIT_REJ) - 1),
           remainder_reject = hash % (1 << REMAINDER_BIT_REJ);

  switch (filter_request) {
    case SPP_L2C_PREFETCH:
      valid[quotient] = 1;  // Mark as prefetched
      useful[quotient] = 0; // Reset useful bit
      remainder_tag[quotient] = remainder;

      // Logging perc features
      //delta[quotient] = cur_delta;
      //pc[quotient] = ip;
      //pc_1[quotient] = GHR.ip_1;
      //pc_2[quotient] = GHR.ip_2;
      //pc_3[quotient] = GHR.ip_3;
      //last_signature[quotient] = last_sig; 
      //cur_signature[quotient] = curr_sig;
      //confidence[quotient] = conf;
      page_number[quotient]=base_addr>>12;
      block_number[quotient]=(base_addr & 0xFFF) >> 6;
      address[quotient] = base_addr; 
      perc_sum[quotient] = sum;
      //la_depth[quotient] = depth;
      /*
      SPP_DP (
          cout << "[FILTER] " << __func__ << " set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
          cout << " quotient: " << quotient << " remainder_tag: " << remainder_tag[quotient] << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
          cout << " More Recorded Metadata: Addr:" << hex << address[quotient] << dec << " PC: " << pc[quotient] << " Delta: " << delta[quotient] << " Last Signature: " << last_signature[quotient] << " Current Signature: " << cur_signature[quotient] << " Confidence: " << confidence[quotient] << endl;
          );*/
      break;

    case SPP_LLC_PREFETCH:
      // NOTE: SPP_LLC_PREFETCH has relatively low confidence (FILL_THRESHOLD <= SPP_LLC_PREFETCH < PF_THRESHOLD) 
      // Therefore, it is safe to prefetch this cache line in the large LLC and save precious L2C capacity
      // If this prefetch request becomes more confident and SPP eventually issues SPP_L2C_PREFETCH,
      // we can get this cache line immediately from the LLC (not from DRAM)
      // To allow this fast prefetch from LLC, SPP does not set the valid bit for SPP_LLC_PREFETCH

      //valid[quotient] = 1;
      //useful[quotient] = 0;
      /*
      SPP_DP (
          cout << "[FILTER] " << __func__ << " don't set valid for check_addr: " << hex << check_addr << " cache_line: " << cache_line << dec;
          cout << " quotient: " << quotient << " valid: " << valid[quotient] << " useful: " << useful[quotient] << endl; 
          );
      */
      break;
    default:
      std::cout << "hello"<< std::endl;
      //assert(0);
  }
  return true;
}

/*
void spp::GLOBAL_REGISTER::update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset, int pf_delta) 
{
  // NOTE: GHR implementation is slightly different from the original paper
  // Instead of matching (last_offset + delta), GHR simply stores and matches the pf_offset
  uint32_t min_conf = 100,
           victim_way = MAX_GHR_ENTRY;

  SPP_DP (
      cout << "[GHR] Crossing the page boundary pf_sig: " << hex << pf_sig << dec;
      cout << " confidence: " << pf_confidence << " pf_offset: " << pf_offset << " pf_delta: " << pf_delta << endl;
      );

  for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
    //if (sig[i] == pf_sig) { // TODO: Which one is better and consistent?
    // If GHR already holds the same pf_sig, update the GHR entry with the latest info
    if (valid[i] && (offset[i] == pf_offset)) {
      // If GHR already holds the same pf_offset, update the GHR entry with the latest info
      sig[i] = pf_sig;
      confidence[i] = pf_confidence;
      //offset[i] = pf_offset;
      delta[i] = pf_delta;

      SPP_DP (cout << "[GHR] Found a matching index: " << i << endl;);

      return;
    }

    // GHR replacement policy is based on the stored confidence value
    // An entry with the lowest confidence is selected as a victim
    if (confidence[i] < min_conf) {
      min_conf = confidence[i];
      victim_way = i;
    }
  }

  // Assertion
  if (victim_way >= MAX_GHR_ENTRY) {
    cout << "[GHR] Cannot find a replacement victim!" << endl;
    assert(0);
  }

  SPP_DP (
      cout << "[GHR] Replace index: " << victim_way << " pf_sig: " << hex << sig[victim_way] << dec;
      cout << " confidence: " << confidence[victim_way] << " pf_offset: " << offset[victim_way] << " pf_delta: " << delta[victim_way] << endl;
      );

  valid[victim_way] = 1;
  sig[victim_way] = pf_sig;
  confidence[victim_way] = pf_confidence;
  offset[victim_way] = pf_offset;
  delta[victim_way] = pf_delta;
  }
*/
/*
  uint32_t GLOBAL_REGISTER::check_entry(uint32_t page_offset)
  {
    uint32_t max_conf = 0,
             max_conf_way = MAX_GHR_ENTRY;

    for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
      if ((offset[i] == page_offset) && (max_conf < confidence[i])) {
        max_conf = confidence[i];
        max_conf_way = i;
      }
    }

    return max_conf_way;
  }
*/
  void spp::PREFETCH_FILTER::get_perc_index(uint64_t base_addr, uint64_t perc_set[PERC_FEATURES])
  {
    // Returns the imdexes for the perceptron tables
    uint64_t page_number=base_addr>>12;
    uint64_t block_number=(base_addr & 0xFFF) >> 6;
    uint64_t  pre_hash[PERC_FEATURES];

    pre_hash[0] = page_number;
    pre_hash[1] = block_number;
    pre_hash[2] = block_number^page_number;
    pre_hash[3] = base_addr;

    for (int i = 0; i < PERC_FEATURES; i++) {
      perc_set[i] = (pre_hash[i]) % PERC_DEPTH[i]; // Variable depths
      /*
      SPP_DP (
          cout << "  Perceptron Set Index#: " << i << " = " <<  perc_set[i];
          );
          */
    }
    /*
    SPP_DP (
        cout << endl;
        );
        */		
  }

  int32_t	spp::PREFETCH_FILTER::perc_predict(uint64_t base_addr)
  {

    uint64_t perc_set[PERC_FEATURES];
    // Get the indexes in perc_set[]
    get_perc_index(base_addr,perc_set);

    int32_t sum = 0;
    for (int i = 0; i < PERC_FEATURES; i++) {
      sum += perc_weights[perc_set[i]][i];	
      // Calculate Sum
    }
    /*
    SPP_DP (
        cout << " Sum of perceptrons: " << sum << " Prediction made: " << ((sum >= PERC_THRESHOLD_LO) ?  ((sum >= PERC_THRESHOLD_HI) ? FILL_L2 : FILL_LLC) : 0)  << endl;
        );*/
    // Return the sum
    return sum;
  }

  void 	spp::PREFETCH_FILTER::perc_update(uint64_t base_addr, bool direction, int32_t perc_sum)
  {

    uint64_t perc_set[PERC_FEATURES];
    // Get the perceptron indexes
    get_perc_index(base_addr,perc_set);

    int32_t sum = 0;
    for (int i = 0; i < PERC_FEATURES; i++) {
      // Marking the weights as touched for final dumping in the csv
      perc_touched[perc_set[i]][i] = 1;	
    }
    // Restore the sum that led to the prediction
    sum = perc_sum;

    if (!direction) { // direction = 1 means the sum was in the correct direction, 0 means it was in the wrong direction
      // Prediction wrong
      for (int i = 0; i < PERC_FEATURES; i++) {
        if (sum >= PERC_THRESHOLD_HI) {
          // Prediction was to prefectch -- so decrement counters
          if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
            perc_weights[perc_set[i]][i]--;
        }
        if (sum < PERC_THRESHOLD_HI) {
          // Prediction was to not prefetch -- so increment counters
          if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
            perc_weights[perc_set[i]][i]++;
        }
      }
      /*
      SPP_DP (
          int differential = (sum >= PERC_THRESHOLD_HI) ? -1 : 1;
          cout << " Direction is: " << direction << " and sum is:" << sum;
          cout << " Overall Differential: " << differential << endl;
          );*/
    }
    if (direction && sum > NEG_UPDT_THRESHOLD && sum < POS_UPDT_THRESHOLD) {
      // Prediction correct but sum not 'saturated' enough
      for (int i = 0; i < PERC_FEATURES; i++) {
        if (sum >= PERC_THRESHOLD_HI) {
          // Prediction was to prefetch -- so increment counters
          if (perc_weights[perc_set[i]][i] < PERC_COUNTER_MAX)
            perc_weights[perc_set[i]][i]++;
        }
        if (sum < PERC_THRESHOLD_HI) {
          // Prediction was to not prefetch -- so decrement counters
          if (perc_weights[perc_set[i]][i] > -1*(PERC_COUNTER_MAX+1) )
            perc_weights[perc_set[i]][i]--;
        }
      }
      /*
      SPP_DP (
          int differential = 0;
          if (sum >= PERC_THRESHOLD_HI) differential =  1;
          if (sum  < PERC_THRESHOLD_HI) differential = -1;
          cout << " Direction is: " << direction << " and sum is:" << sum;
          cout << " Overall Differential: " << differential << endl;
          );
          */
    }
  }

//void CACHE::prefetcher_cycle_operate(){}

