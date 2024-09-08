#include "page_bitmap.h"

using unique_key = std::pair<CACHE*, uint32_t>;

namespace {
  std::map<unique_key, page_bitmap::prefetcher> PAGE_BITMAP; 
}

void page_bitmap::prefetcher::init()
{
  for(size_t i = 0; i < TABLE_SIZE; i++)
  {
    tb[i].valid = false;
    
    for (size_t j = 0; j < 64; j++) 
    { 
      tb[i].bitmap[j] = false;
    }
  }
}

void page_bitmap::prefetcher::update_lru(std::size_t i)
{
  bool half = false;

  for(auto var : tb) {
    if (var.lru_bits == std::numeric_limits<uint16_t>::max()) {
      half = true;
      break;
    } 
  }

  if (half) 
  {
    for(auto &var : tb) {
      var.lru_bits = var.lru_bits >> 1; 
    }
  }

  tb[i].lru_bits = 0;

  for(auto &var : tb) {
    if (var.valid) {
      var.lru_bits++;
    }
  }
}

void page_bitmap::prefetcher::update(uint64_t addr)
{
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = 0; i < TABLE_SIZE; i++)
  {
    if (tb[i].valid &&
        tb[i].page_no == page) 
    {
      tb[i].bitmap[block] = true;
      update_lru(i);
      return;
    }
  }

  // Page not found.
  // Find an invalid entry for the page.
  for (size_t i = 0; i < TABLE_SIZE; i++) 
  {
    if (!tb[i].valid) 
    {
      tb[i].valid = true;
      tb[i].page_no = page;
      tb[i].bitmap[block] = true;
      update_lru(i);
      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  std::size_t index = 0;
  uint16_t lru = 0;

  for(size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid &&
        tb[i].lru_bits > lru) 
    {
      index = i;
      lru = tb[i].lru_bits;
    } 
  }

  tb[index].page_no = page;

  for(auto &var : tb[index].bitmap) {
    var = false;
  }

  tb[index].bitmap[block] = true;
  update_lru(index);
}

void page_bitmap::prefetcher::gather_pf()
{
  // Clear prefetch queue.
  cs_pf.clear();

  // Start from MRU pages.
  std::vector<std::pair<std::size_t, uint16_t>> i_lru_vec;

  for(size_t i = 0; i < TABLE_SIZE; i++) {

    if (tb[i].valid) {
      i_lru_vec.push_back(std::make_pair(i, tb[i].lru_bits)); 
    }
  }

  std::sort(i_lru_vec.begin(), i_lru_vec.end(), [](auto &left, auto &right) {
      return left.second < right.second;
      });

  std::cout << "Valid pages = " << i_lru_vec.size() << std::endl;

  // Get the prefetches.
  for(auto var : i_lru_vec) {

    size_t i = var.first;
    //std::cout << "i = " << i << " LRU = " << (unsigned)tb[i].lru_bits << std::endl; 
    uint64_t page_addr = tb[i].page_no << 12;

    for (size_t j = 0; j < 64; j++) {

      if (tb[i].bitmap[j]) {
        cs_pf.push_back(page_addr + (j << 6)); 
      }
    } 
  }
  
  std::cout << "Gathered " << cs_pf.size() << " prefetches from past accesses in LLC." << std::endl;
}

