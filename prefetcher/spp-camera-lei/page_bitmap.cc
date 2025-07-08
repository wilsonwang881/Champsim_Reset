#include "page_bitmap.h"

void spp::SPP_PAGE_BITMAP::pb_file_read() {
  if (!READ_PAGE_ACCESS) 
    return; 

  std::ifstream file(PAGE_BITMAP_ACCESS);

  if (!file.good()) 
    return; 

  pb_file.open(PAGE_BITMAP_ACCESS, std::ifstream::in);
  uint64_t asid, pg_no;

  while(!pb_file.eof()) {
    pb_file >> asid >> pg_no;

    if (pg_no == 0 && asid == 0)
      break; 

    for (size_t i = 0; i < BITMAP_SIZE; i++) 
      pb_file >> pb_acc[asid][pg_no][i]; 
  }

  pb_file.close();
  pb_file.open(PAGE_BITMAP_ACCESS, std::ofstream::out | std::ofstream::trunc);
  std::cout << "Done reading page bitmap file." << std::endl;
  pb_file.close();
}

void spp::SPP_PAGE_BITMAP::pb_file_write(uint64_t asid) {
  if (!READ_PAGE_ACCESS) 
    return; 

  pb_file.open(PAGE_BITMAP_ACCESS, std::ofstream::app);

  for(auto pair : this_round_pg_acc) {
    uint64_t accu = std::accumulate(pair.second.block.begin(), pair.second.block.end(), 0);

    if (accu > FILTER_THRESHOLD) {
      pb_file << asid << " " << pair.first;

      for (size_t i = 0; i < BITMAP_SIZE; i++) 
        pb_file << (pair.second.block[i] ? "\u25FC" : "\u25FB"); 

      pb_file << std::endl;
    }
  }

  pb_file.close();
}

void spp::SPP_PAGE_BITMAP::lru_operate(std::vector<PAGE_R> &l, std::size_t i, uint64_t way) {
  uint64_t set_begin = i - (i % way);

  for (size_t j = set_begin; j < set_begin + way; j++) {
    if (l[j].lru_bits >=  0x3FF) {
      for (size_t k = set_begin; k < set_begin + way; k++) {
        if (l[k].lru_bits > 2) 
          l[k].lru_bits = l[k].lru_bits - 2; 
        else
          l[k].lru_bits = 0;
      }

      break;
    } 
  }

  for (size_t j = set_begin; j < set_begin + way; j++) 
    l[j].lru_bits++;

  l[i].lru_bits = 0;
}

void spp::SPP_PAGE_BITMAP::update(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  if (RECORD_PAGE_ACCESS) {
    this_round_pg_acc[page].block[block] = true;
    this_round_pg_acc[page].total_access++;
    this_round_pg_acc[page].row_access[block / 8]++; 
  }

  // Page already exists.
  // Update the bitmap of that page.
  // Update the LRU bits.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (tb[i].valid && tb[i].page_no == page) {
      tb[i].bitmap[block] = true;
      lru_operate(tb, i, TABLE_WAY);
      tb[i].acc_counter++;

      return;
    }
  }

  // Page not found.
  // Check or update filter first.
  bool check_filter = filter_operate(addr);

  if (!check_filter)
    return; 

  // Allocate new entry for the new page with more than threshold number of blocks.
  std::vector<uint64_t> filter_blks = {block};

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid && filter[i].page_no == page) {
      for(size_t j = 0; j < BITMAP_SIZE; j++) {
        if (filter[i].bitmap[j]) 
          filter_blks.push_back(j);

        filter[i].bitmap[j] = false;
      }

      filter[i].valid = false;
      break;
    } 
  }

  // Find an invalid entry for the page.
  for (size_t i = set * TABLE_WAY; i < (set + 1) * TABLE_WAY; i++) {
    if (!tb[i].valid) {
      tb[i].valid = true;
      tb[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].bitmap[j] = false; 

      for(auto var : filter_blks) 
        tb[i].bitmap[var] = true; 

      lru_operate(tb, i, TABLE_WAY);
      tb[i].acc_counter = filter_blks.size();

      return;
    }
  }

  // All pages valid.
  // Find LRU page.
  auto lru_el = std::max_element(tb.begin() + set * TABLE_WAY, tb.begin() + (set + 1) * TABLE_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->page_no = page;

  for(auto &var : lru_el->bitmap)
    var = false;

  for(auto &var : lru_el->bitmap_store)
    var = false;

  for(auto var : filter_blks) 
    lru_el->bitmap[var] = true; 

  lru_el->acc_counter = filter_blks.size();

  lru_operate(tb, lru_el - tb.begin(), TABLE_WAY);
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

std::vector<std::pair<uint64_t, bool>> spp::SPP_PAGE_BITMAP::gather_pf(uint64_t asid) {
  cs_pf.clear();
  std::vector<std::pair<uint64_t, bool>> pf;

  if (READ_PAGE_ACCESS) {
    for(auto pair : pb_acc) {
      if (pair.first == asid) {
        for(auto sub_pair : pair.second) {
          uint64_t page_addr = sub_pair.first << 12;

          for (size_t i = 0; i < BITMAP_SIZE; i++) {
            if (sub_pair.second[i]) 
              cs_pf.push_back(std::make_pair(page_addr + (i << 6), true)); 
          }

          std::cout << asid << " " << sub_pair.first;

          for (size_t i = 0; i < BITMAP_SIZE; i++) 
            std::cout << " " << sub_pair.second[i];

          std::cout << std::endl;
        } 
      } 
    } 
  }
  else {
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
        
        //std::cout << "page_no " << tb[i].page_no;

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (tb[i].bitmap[j]) {
            cs_pf.push_back(std::make_pair(page_addr + (j << 6), true)); 
            //std::cout << " " << j;
          }
        }

        //std::cout << std::endl;
      }
    }

    std::cout << "Page bitmap page matches: " << page_match << std::endl;
    uint64_t filter_sum = 0;

    for (size_t i = 0; i < FILTER_SIZE; i++) {
      if (filter[i].valid) {
        uint64_t page_addr = filter[i].page_no << 12;

        for (size_t j = 0; j < BITMAP_SIZE; j++) {
          if (filter[i].bitmap[j]) {
            cs_pf.push_back(std::make_pair(page_addr + (j << 6), true));
            filter_sum++;
          }
        } 
      } 
    }

    /*
    for (size_t i = 0; i < TABLE_SIZE; i++) {
      tb[i].valid = false;
      
      for (size_t j = 0; j < BITMAP_SIZE; j++)
        tb[i].bitmap[j] = false; 
    }
    */

    if (RECORD_PAGE_ACCESS) 
      print_page_access(); 
  }

  std::cout << "Page bitmap gathered " << cs_pf.size() << " prefetches from past accesses." << std::endl;

  for(auto var : cs_pf)
    pf.push_back(var); 

  return pf;
}

bool spp::SPP_PAGE_BITMAP::filter_operate(uint64_t addr) {
  uint64_t set = calc_set(addr);
  uint64_t page = addr >> 12;
  uint64_t block = (addr & 0xFFF) >> 6;

  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (filter[i].valid &&
        filter[i].page_no == page) {
      filter[i].bitmap[block] = true;
      lru_operate(filter, i, FILTER_WAY);
      uint64_t accu = std::accumulate(filter[i].bitmap, filter[i].bitmap + BITMAP_SIZE, 0);

      if (accu > FILTER_THRESHOLD) 
        return true; 
      else 
        return false;
    } 
  }

  // Allocate new entry in the filter.
  // If any invalid entry exists.
  for (size_t i = set * FILTER_WAY; i < (set + 1) * FILTER_WAY; i++) {
    if (!filter[i].valid) {
      filter[i].valid = true;
      filter[i].page_no = page;

      for (size_t j = 0; j < BITMAP_SIZE; j++) 
        filter[i].bitmap[j] = false; 

      filter[i].bitmap[block] = true;
      lru_operate(filter, i, FILTER_WAY);

      return false;
    } 
  }

  // If filter full, use LRU to replace.
  auto lru_el = std::max_element(filter.begin() + set * FILTER_WAY, filter.begin() + (set + 1) * FILTER_WAY,
                [](const auto& l, const auto& r) { return l.lru_bits < r.lru_bits; }); 

  lru_el->page_no = page;
  lru_el->valid = true;

  for(auto &var : lru_el->bitmap)
    var = false;

  lru_el->bitmap[block] = true;
  lru_operate(filter, lru_el - filter.begin(), FILTER_WAY);

  return false;
}

void spp::SPP_PAGE_BITMAP::update_usefulness(uint64_t addr) {
  if (issued_cs_pf.find((addr >> 6) << 6) != issued_cs_pf.end()) {
    issued_cs_pf_hit++; 
    issued_cs_pf.erase((addr >> 6) << 6);
  }
}

uint64_t spp::SPP_PAGE_BITMAP::calc_set(uint64_t addr) {
  return (addr >> 6) & champsim::bitmask(champsim::lg2(TABLE_SET));
}

void spp::SPP_PAGE_BITMAP::print_page_access() {
  uint64_t same_pg_cnt = 0;

  for(auto pair : this_round_pg_acc) {

    uint64_t accu = std::accumulate(pair.second.block.begin(), pair.second.block.end(), 0);

    if (auto search = last_round_pg_acc.find(pair.first); search != last_round_pg_acc.end()) {

      if (accu > FILTER_THRESHOLD) {
        same_pg_cnt++;
        std::cout << "Page " << pair.first << " match, accesses: " << last_round_pg_acc[pair.first].total_access << "/" << this_round_pg_acc[pair.first].total_access << " last/this" << std::endl;

        for (std::size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          for (size_t j = 0; j < BITMAP_SIZE / 8; j++) 
            std::cout << (last_round_pg_acc[pair.first].block[i * 8 + j] ? "\u25FC" : "\u25FB");

          std::cout << std::setw(5) << last_round_pg_acc[pair.first].row_access[i] << " "; 
        }

        std::cout << "last round" << std::endl;

        for (std::size_t i = 0; i < BITMAP_SIZE / 8; i++) {
          for (size_t j = 0; j < BITMAP_SIZE / 8; j++) 
            std::cout << (pair.second.block[i * 8 + j] ? "\u25FC" : "\u25FB");
          
          std::cout << std::setw(5) << this_round_pg_acc[pair.first].row_access[i] << " ";
        }

        std::cout << "this round " << std::endl;
      }
    } 
  }

  last_round_pg_acc = this_round_pg_acc;
  this_round_pg_acc.clear();

  std::cout << "Page bitmap same page: " << same_pg_cnt << std::endl;
}
