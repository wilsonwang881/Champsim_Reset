#include "camera.h"

void camera::prefetcher::init()
{
  issued = 0;
}

void camera::prefetcher::acc_operate(uint64_t addr)
{
  uint64_t pg_no = addr >> 12;
  uint8_t blk_no = (addr >> 6) & 0x3F;

  bool found = false;

  for(auto &var : acc)
  {
    if (var.page_no == pg_no)
    {
      found = true;

      if(std::find(var.blk.begin(), var.blk.end(), blk_no) == var.blk.end() || var.blk.size() == 0) 
      {
        if (var.blk.size() >= 20)
          var.blk.pop_front();

        var.blk.push_back(blk_no);
      } 

      break;
    }
  } 

  if (!found) 
  {
    if (acc.size() >= 4096) 
    {
      acc.pop_front();  
    }

    pg tmpp;
    tmpp.page_no = pg_no;
    tmpp.blk.push_back(blk_no);
    acc.push_back(tmpp);
  }
}

