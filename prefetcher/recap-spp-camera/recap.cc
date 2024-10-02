#include "recap.h"

void spp::SPP_RECAP::init()
{
  for(size_t i = 0; i < TABLE_SIZE; i++)
  {

    tb[i].valid = false;
    tb[i].reuse = false;
    
    for (size_t j = 0; j < 64; j++) 
      tb[i].bitmap[j] = false;
  }
}

void spp::SPP_RECAP::update_lru(std::size_t i)
{
  bool half = false;

  for(auto var : tb) 
  {

    if (var.lru_bits >= (std::numeric_limits<uint16_t>::max() & 0xFFFF)) 
    {

      half = true;
      break;
    } 
  }

  if (half) 
  {

    for(auto &var : tb) 
      var.lru_bits = var.lru_bits >> 1; 
  }

  tb[i].lru_bits = 0;

  for(auto &var : tb) 
  {

    if (var.valid) 
      var.lru_bits++;
  }
}

void spp::SPP_RECAP::update(uint64_t addr)
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

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
        tb[i].bitmap[j] = false; 

      tb[i].bitmap[block] = true;
      update_lru(i);
      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  std::size_t index = 0;
  uint16_t lru = 0;

  for(size_t i = 0; i < TABLE_SIZE; i++) 
  {

    if (tb[i].valid &&
        tb[i].lru_bits > lru) 
    {

      index = i;
      lru = tb[i].lru_bits;
    } 
  }

  tb[index].page_no = page;
  tb[index].reuse = false;

  for(auto &var : tb[index].bitmap) 
    var = false;

  tb[index].bitmap[block] = true;
  update_lru(index);
}

void spp::SPP_RECAP::update_reuse(uint64_t addr)
{
  uint64_t page = addr >> 12;

  // Update the reuse bit.
  for (size_t i = 0; i < TABLE_SIZE; i++)
  {

    if (tb[i].valid &&
        tb[i].page_no == page) 
      tb[i].reuse = true;;
  }
}

std::vector<uint64_t > spp::SPP_RECAP::gather_pf()
{
  // Clear prefetch queue.
  cs_pf.clear();

  // Start from MRU pages.
  std::vector<std::pair<std::size_t, uint16_t>> i_lru_vec;

  for(size_t i = 0; i < TABLE_SIZE; i++) 
  {

    if (tb[i].valid) 
      i_lru_vec.push_back(std::make_pair(i, tb[i].lru_bits)); 
  }

  std::sort(i_lru_vec.begin(), i_lru_vec.end(), [](auto &left, auto &right) {
      return left.second < right.second;
      });

  // Get the prefetches.
  for(auto var : i_lru_vec) 
  {

    size_t i = var.first;
    uint64_t page_addr = tb[i].page_no << 12;

    for (size_t j = 0; j < 64; j++) 
    {

      if (tb[i].bitmap[j]) 
      {

        if (!BLOCK_REUSE_MODE) 
        {

          if (tb[i].reuse)
            cs_pf.push_back(page_addr + (j << 6)); 
        }
        else 
          cs_pf.push_back(page_addr + (j << 6)); 
      }
    } 
  }

  std::cout << " gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;

  return cs_pf;
}

