#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::lru_operate(std::vector<PAGE_R> &l, std::size_t i) {
  for(auto &var : l) {
    if (var.lru_bits >= (std::numeric_limits<uint16_t>::max() & 0x3FF)) {
      for(auto &v : l) {
        if (v.lru_bits > 2) 
          v.lru_bits = v.lru_bits - 2; 
        else
          v.lru_bits = 0;
      }

      break;
    } 
  }

  for(auto &var : l) 
      var.lru_bits++;

  l[i].lru_bits = 0;
}

void spp::SPP_PAGE_BITMAP::update(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  //HL
  size_t match_delta= DELTA_SIZE;
  
  // Page already exists.

  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    
    //HL
    int64_t delta;
    delta=block-tb[i].last_offset;
    tb[i].last_offset=block;

    if (tb[i].valid && tb[i].page_no == page) {
      
      //std::cout<<"The delta is "<<delta<<std::endl;
      
      //find the delta
      for(match_delta = 0;match_delta < DELTA_SIZE; match_delta++) {
        //delta is found
        if(tb[i].delta[match_delta]==delta && tb[i].valid_delta[match_delta]) {
          tb[i].c_delta[match_delta]++;

          if(tb[i].c_delta[match_delta]==C_DELTA_MAX) {
            //delta_counter++;
            int64_t block_offset;
            int64_t block_offset_2;
            block_offset=block+delta;
            block_offset_2=block+delta+delta;
            //std::cout<<"The optimal delta is"<<tb[i].delta[match_delta]<<std::endl;
            //std::cout<<"The optimal delta at count is"<<tb[i].c_delta[match_delta]<<std::endl;
            //update the delta block
            
            if(block_offset>=0 && block_offset<=63)
              tb[i].bitmap[block_offset]=true;

            if(tb[i].bitmap[block_offset]==COUNT_MAX)
              tb[i].saturated_bit=true;

            if(block_offset_2>=0 && block_offset_2<=63)
              tb[i].bitmap[block_offset_2]=true;            

            tb[i].c_delta[match_delta]=tb[i].c_delta[match_delta]>>1;
            //uint64_t page_addr;
            //page_addr=tb[i].page_no << 12;
            //bop_pf.push_back(page_addr + (block_offset << 6));
            //std::cout<<"we have prefetch at page "<<tb[i].page_no<<" with block "<<block_offset<<" with total delta value "<<delta_counter<<std::endl; 
          }
          //std::cout<<"The match delta is at "<<match_delta<<std::endl;
          break;
        }
      }

      //invalid case
      if(match_delta==DELTA_SIZE) {
        for(match_delta = 0;match_delta < DELTA_SIZE; match_delta++) {
          if(tb[i].valid_delta[match_delta]==false) {
            //std::cout<<"The delta"<<tb[i].delta[match_delta]<<"is new coming"<<std::endl;
            tb[i].valid_delta[match_delta]=true;
            tb[i].delta[match_delta]=delta;
            tb[i].c_delta[match_delta]=0;

            break;
          }
        }
      }

      //delta is not found,replace the least LRU
      if(match_delta==DELTA_SIZE) {
        for(match_delta = 0;match_delta < DELTA_SIZE; match_delta++) {
          if(tb[i].lru_delta[match_delta]==(DELTA_SIZE-1)) {
            //std::cout<<"The delta"<<delta<<"is not found, least LRU"<<match_delta<<"is replaced"<<std::endl;
            tb[i].delta[match_delta]=delta;
            tb[i].c_delta[match_delta]=0;

            break;
          }
        }
      }

      //update delta_LRU
      for (size_t j=0;j<DELTA_SIZE;j++) {
        if(tb[i].lru_delta[j]<tb[i].lru_delta[match_delta])
          tb[i].lru_delta[j]++; 
      }

      tb[i].lru_delta[match_delta]=0;
      //HL end

      tb[i].bitmap[block] = true;
      lru_operate(tb, i);

      return;
    }
  }

  // Page not found.
  // Check or update filter first.
  bool check_filter = filter_operate(addr);

  if (!check_filter)
    return; 

  // Allocate new entry for the new page with 2 blocks.
  std::vector<uint64_t> filter_blks = {block};

  for(auto &var : filter) {
    if (var.valid && var.page_no == page) {
      for(size_t i = 0; i < BITMAP_SIZE; i++) {
        if (var.bitmap[i]) 
          filter_blks.push_back(i);

        var.bitmap[i] = false;
      }

      var.valid = false;
      break;
    } 
  }

  // Find an invalid entry for the page.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (!tb[i].valid) {
      tb[i].valid = true;
      tb[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].bitmap[j] = false; 

      for(auto var : filter_blks) 
        tb[i].bitmap[var] = true;

      //HL
      for (size_t k = 0; k <DELTA_SIZE; k++) {
        tb[i].delta[k] = 0;
        tb[i].c_delta[k] = 0;
        tb[i].lru_delta[k] = k;
        tb[i].valid_delta[k]=false;
      }

      //HL
      tb[i].last_offset = 0;  

      lru_operate(tb, i);

      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  std::size_t index = 0;
  uint16_t lru = 0;

  for(size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid && tb[i].lru_bits > lru) {
      index = i;
      lru = tb[i].lru_bits;
    } 
  }

  tb[index].page_no = page;

  for(auto &var : tb[index].bitmap)
    var = false;

  for(auto &var : tb[index].bitmap_store)
    var = false;

  for(auto var : filter_blks) 
    tb[index].bitmap[var] = true;

  //HL
  for (size_t k = 0; k <DELTA_SIZE; k++) {
    tb[index].delta[k] = 0;
    tb[index].c_delta[k] = 0;
    tb[index].lru_delta[k] = k;
    tb[index].valid_delta[k]=false;
  }

  tb[index].last_offset = 0; 

  lru_operate(tb, index);
}

void spp::SPP_PAGE_BITMAP::evict(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  // Check tb first.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].page_no == page &&
        tb[i].valid)
      tb[i].bitmap[block] = false;
  }

  // Check filter.
  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].page_no == page &&
        filter[i].valid) 
      filter[i].bitmap[block] = false;
  }
}

void spp::SPP_PAGE_BITMAP::update_bitmap_store() {
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid) {
      // If same pages found in the same entry.
      if (tb[i].page_no_store == tb[i].page_no) {
        for (size_t j = 0; j < BITMAP_SIZE; j++) 
          tb[i].bitmap_store[j] = tb[i].bitmap[j]; // | tb[i].bitmap_store[j];
      }
      // If same page found in different entries.
      else {
        bool found = false;

        for (size_t k = 0; k < TABLE_SIZE; k++) {
          if (tb[k].page_no == tb[i].page_no_store) {
            for (size_t j = 0; j < BITMAP_SIZE; j++) 
              tb[i].bitmap_store[j] = tb[k].bitmap[j]; // | tb[i].bitmap_store[j];

            found = true;
            break;
          }
        }

        if (!found) {
          for (size_t j = 0; j < BITMAP_SIZE; j++) {
            tb[i].bitmap_store[j] = tb[i].bitmap[j]; // | tb[i].bitmap_store[j];
            tb[i].bitmap[j] = false;
          }

          tb[i].page_no_store = tb[i].page_no;
        }
      }
    }
  }
}

std::vector<std::pair<uint64_t, bool>> spp::SPP_PAGE_BITMAP::gather_pf() {
  cs_pf.clear();
  int page_match = 0;

  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].page_no == tb[i].page_no_store &&
        tb[i].valid) {
      page_match++;
      uint64_t page_addr = tb[i].page_no << 12;

      for (size_t j = 0; j < BITMAP_SIZE; j++) {
        if (tb[i].bitmap[j] && tb[i].bitmap_store[j]) 
          cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
      }
      
      /*
      //HL
      for (size_t j = 0; j < BITMAP_SIZE; j++) {
        bool x=tb[i].bitmap[j];
        bool y=tb[i].bitmap_store[j];
        if (x||y) 
          cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
      }
      */

    }
    else {
      uint64_t page_addr = tb[i].page_no << 12;

      for (size_t j = 0; j < BITMAP_SIZE; j++) {
        if (tb[i].bitmap[j])
          cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
      }
    }
  }

  std::cout << "Page bitmap page matches: " << page_match << std::endl;

  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].valid) {
      uint64_t page_addr = filter[i].page_no << 12;

      for (size_t j = 0; j < BITMAP_SIZE; j++) {
        if (filter[i].bitmap[j]) 
          cs_pf.push_back(std::make_pair(page_addr + (j << 6), true));
      } 
    } 
  }

  std::vector<std::pair<uint64_t, bool>> pf;

  for(auto var : cs_pf)
    pf.push_back(var); 
  
  std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;

  return pf;
}

bool spp::SPP_PAGE_BITMAP::filter_operate(uint64_t addr) {
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].valid &&
        filter[i].page_no == page) {
      filter[i].bitmap[block] = true;
      uint64_t accu = 0;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
        accu = accu + filter[i].bitmap[j]; 

      lru_operate(filter, i);

      if (accu > FILTER_THRESHOLD) 
        return true; 
      else 
        return false;
    } 
  }

  // Allocate new entry in the filter.
  // If any invalid entry exists.
  for(size_t i = 0; i < FILTER_SIZE; i++) {
    if (!filter[i].valid) {
      filter[i].valid = true;
      filter[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
        filter[i].bitmap[j] = false; 

      filter[i].bitmap[block] = true;
      lru_operate(filter, i);

      return false;
    } 
  }

  // If filter full, use LRU to replace.
  size_t index = 0;
  uint8_t lru = 0;

  for (size_t i = 0; i < FILTER_SIZE; i++) {
    if (filter[i].lru_bits > lru) {
      index = i;
      lru = filter[i].lru_bits;
    } 
  }

  filter[index].page_no = page;
  filter[index].valid = true;

  for (size_t i = 0; i < BITMAP_SIZE; i++) 
    filter[index].bitmap[i] = false; 

  filter[index].bitmap[block] = true;
  lru_operate(filter, index);

  return false;
}

void spp::SPP_PAGE_BITMAP::update_usefulness(uint64_t addr) {
  if (issued_cs_pf.find((addr >> 6) << 6) != issued_cs_pf.end()) {
    issued_cs_pf_hit++; 
    issued_cs_pf.erase((addr >> 6) << 6);
  }
}

uint64_t spp::SPP_PAGE_BITMAP::calc_set(uint64_t addr, uint64_t set_num) {
  return (addr >> 6) & champsim::bitmask(champsim::lg2(set_num));
}
