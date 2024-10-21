#include "cache.h"
#include <iostream>

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  //std::cout << set << " " << way << " " << set * NUM_WAY + way << " " << this->block.size() << std::endl;
  //std::cout << "addr = " << addr << " translation => addr = " << this->block[set * NUM_WAY + way].address << " v_addr = " << this-block[set * NUM_SET + way].v_address << " evicted address = " << evicted_addr << std::endl;
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
