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
    tb[i].aft_cs_acc = true;
    
    for (size_t j = 0; j < BITMAP_SIZE; j++) 
    {
      tb[i].bitmap[j] = false;
      tb[i].bitmap_store[j] = false;
    }
  }

  for (size_t i = 0; i < TAG_COUNTER_SIZE; i++)
  {
    ct[i].valid = true; 
    ct[i].counter = 16;
  }

  for (size_t i = 0; i < RJ_PF_SIZE; i++) 
  {
    rj_tb[i].valid = false;
    pf_tb[i].valid = false;
  }
}

void page_bitmap::prefetcher::update_lru(std::size_t i)
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
        tb[i].bitmap[j] = false; 

      tb[i].bitmap[block] = true;
      tb[i].bitmap[block_2] = true;
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
  tb[index].aft_cs_acc = false;

  for(auto &var : tb[index].bitmap)
    var = false;

  for(auto &var : tb[index].bitmap_store)
    var = false;

  tb[index].bitmap[block] = true;
  tb[index].bitmap[block_2] = true;
  update_lru(index);
}

void page_bitmap::prefetcher::update_bitmap(uint64_t addr)
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
      tb[i].bitmap[block] = true;
    }
  }
}

void page_bitmap::prefetcher::update_bitmap_store()
{
  for (size_t i = 0; i < TABLE_SIZE; i++) 
  {
    if (tb[i].valid) 
    {
      tb[i].page_no_store = tb[i].page_no;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
      {
        tb[i].bitmap_store[j] = tb[i].bitmap[j];
        tb[i].bitmap[j] = false;
      }
    }
  }
}

void page_bitmap::prefetcher::clear_pg_access_status()
{
  for(auto &var : tb)
    var.aft_cs_acc = true; 
}

void page_bitmap::prefetcher::gather_pf()
{
  // Clear prefetch queue.
  cs_pf.clear();

  std::cout << std::endl;

  for(size_t i = 0; i < TAG_COUNTER_SIZE; i++) 
  {
    if (ct[i].valid)
    {
      std::cout << std::setw(4) << (unsigned)i << std::setw(3) << (unsigned)ct[i].counter << "|"; 
    }  

    if (((i + 1) % 25) == 0) {
      std::cout << std::endl; 
    }
  }

  std::cout << std::endl;

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

    if (tb[i].aft_cs_acc) 
    {
      uint64_t page_addr = tb[i].page_no << 12;

      if (DEBUG_PRINT) 
        std::cout << "Page " << std::hex << tb[i].page_no << std::dec << " ["; 

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
      {
        uint64_t pf_addr = page_addr + (j << 6);

        if (tb[i].bitmap[j] && 
            tb[i].bitmap_store[j])
        {
          cs_pf.push_back(pf_addr); 

          if (DEBUG_PRINT)
            std::cout << " " << j;
        }
      } 

      if (DEBUG_PRINT) 
        std::cout << " ]" << std::endl;
    }
  }

  //cs_pf.clear();
  
  std::cout << " gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;
}

void page_bitmap::prefetcher::filter_update_lru(std::size_t i)
{
  bool half = false;

  for(auto var : filter) 
  {
    if (var.lru_bits == std::numeric_limits<uint8_t>::max()) 
    {
      half = true;
      break;
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

bool page_bitmap::prefetcher::filter_operate(uint64_t addr)
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
  filter[index].lru_bits = 0;

  return false;
}

uint8_t page_bitmap::prefetcher::saturating_counter(uint8_t val, bool increment)
{
  uint8_t rt = 16;

  if (val == 0 && !increment) 
    rt = 0; 
  else if (val >= 31 && increment) 
    rt = 31; 
  else if (increment) 
    rt = val + 1;  
  else if (!increment) 
    rt = val - 1; 

  return rt;
}

void page_bitmap::prefetcher::tag_counter_update(uint64_t addr, bool useful)
{
  // Directly mapped table.
  uint64_t truncated = (addr >> 12) & 0x3FF;

  if (ct[truncated].valid) 
  {
    uint8_t updated  = saturating_counter(ct[truncated].counter, useful);
    ct[truncated].counter = updated;
  } 
  else
  {
    ct[truncated].valid = true;
    ct[truncated].counter = 16;
  } 
}

bool page_bitmap::prefetcher::tag_counter_check(uint64_t addr)
{
  uint64_t truncated = (addr >> 12) & 0x3FF;

  for(uint64_t i = 0; i < TAG_COUNTER_SIZE; i++) 
  {
    if (ct[i].valid && 
        i == truncated &&
        ct[i].counter >= 15)
      return true;
  }

  return false;
}

void page_bitmap::prefetcher::invalidate_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  size_t index = (addr >> 6) & 0x3FF;
  rj_pf_tb[index].valid = false;
  rj_pf_tb[index].tag = 0;
}

void page_bitmap::prefetcher::invalidate_p_tb(bool rj_or_pf_tb, uint64_t addr)
{
  if (rj_or_pf_tb)
    invalidate_rj_pf_tb(rj_tb, addr); 
  else
    invalidate_rj_pf_tb(pf_tb, addr);
}

void page_bitmap::prefetcher::update_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  size_t index = (addr >> 6) & 0x3FF;
  uint16_t tag_seg = (addr >> 16) & 0xFFFFFF;

  // Allocate new entry
  if (!rj_pf_tb[index].valid)
  {
    rj_pf_tb[index].valid = true;
    rj_pf_tb[index].tag = tag_seg;
  }
  else 
  {
    rj_pf_tb[index].tag = tag_seg;  
  }
}

void page_bitmap::prefetcher::update_p_tb(bool rj_or_pf_tb, uint64_t addr)
{
  if (rj_or_pf_tb)
    update_rj_pf_tb(rj_tb, addr); 
  else 
    update_rj_pf_tb(pf_tb, addr);
}

bool page_bitmap::prefetcher::check_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  size_t index = (addr >> 6) & 0x3FF;
  uint16_t tag_seg = (addr >> 16) & 0xFFFFFF;

  return (rj_pf_tb[index].valid) && (rj_pf_tb[index].tag == tag_seg);
}

bool page_bitmap::prefetcher::check_p_tb(bool rj_or_pf_tb, uint64_t addr)
{
  bool res = false;

  if (rj_or_pf_tb)
    res = check_rj_pf_tb(rj_tb, addr); 
  else
    res = check_rj_pf_tb(pf_tb, addr);

  return res;
}
