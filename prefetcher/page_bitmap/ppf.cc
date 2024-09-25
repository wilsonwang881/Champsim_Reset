#include "ppf.h"

void page_bitmap::PAGE_BITMAP_PPF::init()
{
  for (std::size_t i = 0; i < TAG_COUNTER_SIZE; i++)
  {
    ct[i].valid = true; 
    ct[i].counter = 16;
    upt[i].valid = true;
    upt[i].counter = 16;
  }

  for (std::size_t i = 0; i < RJ_PF_SIZE; i++) 
  {
    rj_tb[i].valid = false;
    pf_tb[i].valid = false;
  }
}

uint8_t page_bitmap::PAGE_BITMAP_PPF::saturating_counter(uint8_t val, bool increment)
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

void page_bitmap::PAGE_BITMAP_PPF::tag_counter_update(uint64_t addr, bool useful)
{
  // Directly mapped table.
  uint64_t truncated = (addr >> 12) & 0x3FF;
  uint64_t upper_truncated = (addr >> 6) & 0x3FF;
  uint64_t sum = ct[truncated].counter + upt[upper_truncated].counter;

  if ((sum >= 45) || (sum <= 15))
    return;

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

  if (upt[upper_truncated].valid) 
  {
    uint8_t updated  = saturating_counter(upt[upper_truncated].counter, useful);
    upt[upper_truncated].counter = updated;
  } 
  else
  {
    upt[upper_truncated].valid = true;
    upt[upper_truncated].counter = 16;
  } 
}

bool page_bitmap::PAGE_BITMAP_PPF::tag_counter_check(uint64_t addr)
{
  uint64_t truncated = (addr >> 12) & 0x3FF;
  uint64_t upper_truncated = (addr >> 6) & 0x3FF;

  uint64_t sum = ct[truncated].counter + upt[upper_truncated].counter;

  return sum >= 10;
}

void page_bitmap::PAGE_BITMAP_PPF::invalidate_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  uint64_t index = (addr >> 6) & 0x3FF;
  rj_pf_tb[index].valid = false;
  rj_pf_tb[index].tag = 0;
}

void page_bitmap::PAGE_BITMAP_PPF::update_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  uint64_t index = (addr >> 6) & 0x3FF;
  uint16_t tag_seg = (addr >> 16); // & 0xFFFFFF;

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

bool page_bitmap::PAGE_BITMAP_PPF::check_rj_pf_tb(rj_pf_r rj_pf_tb[], uint64_t addr)
{
  uint64_t index = (addr >> 6) & 0x3FF;
  uint16_t tag_seg = (addr >> 16); // & 0xFFFFFF;

  return (rj_pf_tb[index].valid) && (rj_pf_tb[index].tag == tag_seg);
}
