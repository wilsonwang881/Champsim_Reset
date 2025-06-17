#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::init()
{
  for(size_t i = 0; i < TABLE_SIZE; i++)
  {
    tb[i].valid = false;
    //HL
    tb[i].last_offset = 0;
    delta_counter=0;
    
    for (size_t j = 0; j < BITMAP_SIZE; j++) 
    {
      tb[i].bitmap[j] = false;
      //tb[i].bitmap_store[j] = false;
      tb[i].saturated_bit=false;
    }
    //HL
    for (size_t k = 0; k <DELTA_SIZE; k++) 
    {
      tb[i].delta[k] = 0;
      tb[i].c_delta[k] = 0;
      tb[i].lru_delta[k] = 0;
    }
  }

  for (size_t i = 0; i < COUNTER_SIZE; i++)
    ct[i].valid = false; 

  for (size_t i = 0; i < FILTER_SIZE; i++) {
    filter[i].valid = false; 
  }
}

void spp::SPP_PAGE_BITMAP::update_lru(std::size_t i)
{
  bool half = false;

  for(auto &var : tb) 
  {
    if (var.lru_bits >= (std::numeric_limits<uint16_t>::max() & 0xFFFF)) 
    {
      half = true;

      /*
      var.valid = false;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
      {
        var.bitmap[j] = false;
        //var.bitmap_store[j] = false;
      }
      */

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

void spp::SPP_PAGE_BITMAP::update(uint64_t addr)
{
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;
  size_t match_delta= DELTA_SIZE;

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = 0; i < TABLE_SIZE; i++)
  {
    if (tb[i].valid && tb[i].page_no == page) 
    {
      //tb[i].bitmap[block] = true;
      //HL
      int64_t delta;
      delta=block-tb[i].last_offset;
      tb[i].last_offset=block;

      //find the delta
      for(match_delta = 0;match_delta < DELTA_SIZE; match_delta++)
      {
        //delta is found
        if(tb[i].delta[match_delta]==delta)
        {
          tb[i].c_delta[match_delta]++;
          if(tb[i].c_delta[match_delta]==C_DELTA_MAX)
          {
            //delta_counter++;
            int64_t block_offset;
            int64_t block_offset_2;
            block_offset=block+delta;
            block_offset_2=block+delta+delta;
            //tb[i].bitmap_delta[block_offset]=true;
            //tb[i].bitmap_delta[block_offset_2]=true;

            //update the delta block
            if(tb[i].bitmap[block_offset]<COUNT_MAX)
            {
                tb[i].bitmap[block_offset]=tb[i].bitmap[block_offset]+1;
            }
            if(tb[i].bitmap[block_offset]==COUNT_MAX)
            {
                tb[i].saturated_bit=true;
            }

            tb[i].c_delta[match_delta]=tb[i].c_delta[match_delta]>>1;
            //uint64_t page_addr;
            //page_addr=tb[i].page_no << 12;
            //bop_pf.push_back(page_addr + (block_offset << 6));
            //std::cout<<"we have prefetch at page "<<tb[i].page_no<<" with block "<<block_offset<<" with total delta value "<<delta_counter<<std::endl; 
          }
          break;
        }
      }

      //delta is not found,replace the least LRU
      if(match_delta==DELTA_SIZE)
      {
        for(match_delta = 0;match_delta < DELTA_SIZE; match_delta++)
        {
          if(tb[i].lru_delta[match_delta]==(DELTA_SIZE-1))
          {
            tb[i].delta[match_delta]=0;
            tb[i].c_delta[match_delta]=0;
            break;
          }
        }
      }

      //update delta_LRU
      for (size_t j=0;j<DELTA_SIZE;j++)
      {
        if(tb[i].lru_delta[j]<tb[i].lru_delta[match_delta])
        {
          tb[i].lru_delta[j]++; 
        }
      }
      tb[i].lru_delta[match_delta]=0;

      //update the block it self
      if(tb[i].bitmap[block]<COUNT_MAX)
      {
        tb[i].bitmap[block]=tb[i].bitmap[block]+1;
      }
      if(tb[i].bitmap[block]==COUNT_MAX)
      {
        tb[i].saturated_bit=true;
      }
      update_lru(i);
      return;
    }
  }

  // Page not found.
  // Check or update filter first.
  bool check_filter = filter_operate(addr);

  if (!check_filter)
    return; 

  // Allocate new entry for the new page with 2 blocks.
  size_t block_2 = 0;

  for(auto &var : filter)
  {
    if (var.valid &&
        var.page_no == page) 
    {
      block_2 = var.block_no;  
      var.valid = false;
    } 
  }

  // Find an invalid entry for the page.
  for (size_t i = 0; i < TABLE_SIZE; i++) 
  {
    if (!tb[i].valid) 
    {
      tb[i].valid = true;
      tb[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].bitmap[j] = 0;

      //HL
      for (size_t k = 0; k <DELTA_SIZE; k++) 
      {
        tb[i].delta[k] = 0;
        tb[i].c_delta[k] = 0;
        tb[i].lru_delta[k] = 0;
      }
      //HL
      tb[i].last_offset=block_2; 

      tb[i].bitmap[block] = 1;
      tb[i].bitmap[block_2] = 1;
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

  for(auto &var : tb[index].bitmap)
    var = 0;
/*
  for(auto &var : tb[index].bitmap_store)
    var = false;
*/
  tb[index].bitmap[block] = 1;
  tb[index].bitmap[block_2] = 1;

  //HL

  for (size_t k = 0; k <DELTA_SIZE; k++) 
  {
    tb[index].delta[k] = 0;
    tb[index].c_delta[k] = 0;
    tb[index].lru_delta[k] = 0;
  }

  tb[index].last_offset=block_2;

  update_lru(index);
}

void spp::SPP_PAGE_BITMAP::evict(uint64_t addr)
{
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Check tb first.
  for (size_t i = 0; i < TABLE_SIZE; i++)
  {
    if (tb[i].valid &&
        tb[i].page_no == page)
    {
      //tb[i].bitmap[block] = false;
      if(tb[i].bitmap[block]>0)
      {
        tb[i].bitmap[block]=tb[i].bitmap[block]-1;
      }
    }
  }

  // Check filter.
  for (size_t i = 0; i < FILTER_SIZE; i++) 
  {
    if (filter[i].page_no == page && filter[i].block_no == block) 
    {
      filter[i].valid = false;
      filter[i].page_no = 0;
      filter[i].block_no = 0;
    }
  }
}

void spp::SPP_PAGE_BITMAP::update_bitmap(uint64_t addr)
{
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = 0; i < TABLE_SIZE; i++)
  {
    if (tb[i].valid &&
        tb[i].page_no == page)
    {
      //tb[i].bitmap[block] = true;
      if(tb[i].bitmap[block]<COUNT_MAX)
      {
        tb[i].bitmap[block]=tb[i].bitmap[block]+1;
      }
      if(tb[i].bitmap[block]==COUNT_MAX)
      {
        tb[i].saturated_bit=true;
      }
    }
  }
}

void spp::SPP_PAGE_BITMAP::update_bitmap_store()
{
  for (size_t i = 0; i < TABLE_SIZE; i++) 
  {
    if (tb[i].valid && (tb[i].saturated_bit==true)) 
    {
        for (size_t j = 0; j < BITMAP_SIZE; j++)
        { 
          tb[i].bitmap[j] = tb[i].bitmap[j]>>1; // | tb[i].bitmap_store[j];
        }
/*
<<<<<<< HEAD
        tb[i].saturated_bit=false;
      
=======

        if (!found) 
        {
          for (size_t j = 0; j < BITMAP_SIZE; j++) 
          {
            tb[i].bitmap_store[j] = tb[i].bitmap[j]; // | tb[i].bitmap_store[j];
            tb[i].bitmap[j] = false;
          }

          tb[i].page_no_store = tb[i].page_no;
        }
      }
>>>>>>> master
*/
    }
  }
}

std::vector<std::pair<uint64_t, bool>> spp::SPP_PAGE_BITMAP::gather_pf()
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

  //int page_match = 0;

  // Get the prefetches.
  for(auto var : i_lru_vec) 
  {
    size_t i = var.first;

    //if (tb[i].page_no == tb[i].page_no_store) 
    //{
      //page_match++;
      uint64_t page_addr = tb[i].page_no << 12;

      if (PAGE_BITMAP_DEBUG_PRINT) 
        std::cout << "Page " << std::hex << tb[i].page_no << std::dec << " ["; 

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
      {
        if (tb[i].bitmap[j]>=2)
        {
          cs_pf.push_back(std::make_pair(page_addr + (j << 6), false)); 

          if (PAGE_BITMAP_DEBUG_PRINT)
            std::cout << " " << j;
        }
      } 

      if (PAGE_BITMAP_DEBUG_PRINT) 
        std::cout << " ]" << std::endl;
    //}
  }
  
  //std::cout << "Page bitmap page matches: " << page_match << std::endl;

  int pf_from_filter = 0;

  /*
  for(auto var : filter) 
  {
    if (var.valid)
    {
      pf_from_filter++;
      //std::cout << (unsigned)var.page_no << " " << (unsigned)var.block_no << std::endl;
      uint64_t addr = (var.page_no << 12) + (var.block_no << 6);
      cs_pf.push_back(std::make_pair(addr, false));
    }
  }
  */

  std::vector<std::pair<uint64_t, bool>> pf;
  for(auto var : cs_pf) {
    pf.push_back(var); 
  }
  
  std::cout << "Page bitmap gathered " << pf_from_filter << " prefetches from filter." << std::endl;
  std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;

  return pf;
}

void spp::SPP_PAGE_BITMAP::filter_update_lru(std::size_t i)
{
  bool half = false;

  for(auto var : filter) 
  {
    if (var.lru_bits >= (uint16_t)0xFFFF) 
    {
      half = true;
      break;
      //var.valid = false;
    } 
  }

  if (half) 
  {
    for(auto &var : filter) 
      var.lru_bits = var.lru_bits >> 1; 
  }

  filter[i].lru_bits = 0;

  for(auto &var : filter) 
  {
    if (var.valid) 
      var.lru_bits++;
  }
}

bool spp::SPP_PAGE_BITMAP::filter_operate(uint64_t addr)
{
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  bool same_page_same_block = false;
  bool same_page_diff_block = false;

  // In the filter table,
  // find the entry with same page number,
  // but different block number.
  // Ensure the entry has at least 2 unique blocks.
  for (size_t i = 0; i < FILTER_SIZE; i++) 
  {
    if (filter[i].valid &&
        filter[i].page_no == page &&
        filter[i].block_no != block) 
      same_page_diff_block = true;

    if (filter[i].valid &&
        filter[i].page_no == page &&
        filter[i].block_no == block) 
    {
      same_page_same_block = true; 
      filter_update_lru(i);
    }
  }

  // Return if at least same page found.
  if (same_page_same_block) 
    return false;
  
  if (same_page_diff_block) 
    return true; 

  // Allocate new entry in the filter.
  // If any invalid entry exists.
  for(size_t i = 0; i < FILTER_SIZE; i++) 
  {
    if (!filter[i].valid) 
    {
      filter[i].valid = true;
      filter[i].page_no = page;
      filter[i].block_no = block;
      filter_update_lru(i);

      return false;
    } 
  }

  // If filter full, use LRU to replace.
  size_t index = 0;
  uint8_t lru = 0;

  for (size_t i = 0; i < FILTER_SIZE; i++) 
  {
    if (filter[i].lru_bits > lru) 
    {
      index = i;
      lru = filter[i].lru_bits;
    } 
  }

  filter[index].page_no = page;
  filter[index].block_no = block;
  filter_update_lru(index);

  return false;
}

void spp::SPP_PAGE_BITMAP::counter_update_lru(std::size_t i)
{
  bool half = false;

  for(auto var : ct) 
  {
    if (var.lru_bits == (std::numeric_limits<uint16_t>::max() & 0xFFF)) 
    {
      half = true;
      break;
    } 
  }

  if (half) 
  {
    for(auto &var : ct) 
      var.lru_bits = var.lru_bits >> 1; 
  }

  ct[i].lru_bits = 0;

  for(auto &var : ct) 
  {
    if (var.valid) 
      var.lru_bits++;
  }
}

void spp::SPP_PAGE_BITMAP::counter_update(uint64_t addr)
{
  uint64_t blk_addr = (addr >> 6) << 6;

  // Try to find an existing entry.
  for(size_t i = 0; i < COUNTER_SIZE; i++) 
  {
    if (ct[i].valid && ct[i].addr == blk_addr) 
    {
      ct[i].counter++;
      counter_update_lru(i);
      return;
    } 
  }

  // No existing entry found.
  // Allocate new entry.
  for (size_t i = 0; i < COUNTER_SIZE; i++) 
  {
    if (!ct[i].valid) 
    {
      ct[i].valid = true;
      ct[i].addr = blk_addr;
      ct[i].counter = 1;
      counter_update(i);
      return;
    } 
  }


  // All entries valid.
  // Find the LRU entry.
  std::size_t index = 0;
  uint16_t lru = 0;

  for(size_t i = 0; i < COUNTER_SIZE; i++) 
  {
    if (ct[i].valid &&
        ct[i].lru_bits > lru) 
    {
      index = i;
      lru = ct[i].lru_bits;
    } 
  }

  ct[index].addr = blk_addr;
  ct[index].counter = 1;
  counter_update_lru(index);
}
