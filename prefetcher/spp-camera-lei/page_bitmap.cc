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

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = 0; i < TABLE_SIZE; i++) {
    if (tb[i].valid && tb[i].page_no == page) {
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
