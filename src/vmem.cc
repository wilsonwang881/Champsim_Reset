/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vmem.h"

#include <cassert>

#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include <fmt/core.h>

VirtualMemory::VirtualMemory(uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram)
    : next_ppage(VMEM_RESERVE_CAPACITY), last_ppage(1ull << (LOG2_PAGE_SIZE + champsim::lg2(page_table_page_size / PTE_BYTES) * page_table_levels)),
      minor_fault_penalty(minor_penalty), pt_levels(page_table_levels), pte_page_size(page_table_page_size)
{
  assert(page_table_page_size > 1024);
  assert(page_table_page_size == (1ull << champsim::lg2(page_table_page_size)));
  assert(last_ppage > VMEM_RESERVE_CAPACITY);

  auto required_bits = champsim::lg2(last_ppage);
  if (required_bits > 64)
    fmt::print("WARNING: virtual memory configuration would require {} bits of addressing.\n", required_bits); // LCOV_EXCL_LINE
  if (required_bits > champsim::lg2(dram.size()))
    fmt::print("WARNING: physical memory size is smaller than virtual memory size.\n"); // LCOV_EXCL_LINE
                                                                                        //
  // WL
  if (RECORD_OR_READ) 
  {
    std::cout << "Recording vmem mapping." << std::endl;
    page_table_file.open(page_table_file_name, std::ofstream::out | std::ofstream::trunc);
    page_table_file.close();
    va_to_pa_file.open(va_to_pa_file_name, std::ofstream::out | std::ofstream::trunc);
    va_to_pa_file.close();
  }
  else {
    std::cout << "Reading vmem mapping." << std::endl;
    page_table_file.open(page_table_file_name, std::ifstream::in);
    fr_page_table.clear();

    uint32_t cpu_num; 
    uint64_t vaddr_shifted; 
    uint32_t level;
    uint64_t nxt_pg;

    while(!page_table_file.eof())
    {
      page_table_file>> cpu_num >> vaddr_shifted >> level >> nxt_pg;

      std::tuple key{cpu_num, vaddr_shifted, level};
      fr_page_table.insert({key, nxt_pg});
    }

    page_table_file.close();

    va_to_pa_file.open(va_to_pa_file_name, std::ifstream::in);
    fr_vpage_to_ppage_map.clear();

    while(!va_to_pa_file.eof())
    {
      va_to_pa_file >> cpu_num >> vaddr_shifted >> nxt_pg;
      fr_vpage_to_ppage_map.insert({{cpu_num, vaddr_shifted}, nxt_pg});
    }

    va_to_pa_file.close();

    //next_ppage = next_ppage + PAGE_SIZE * (fr_vpage_to_ppage_map.size() + fr_page_table.size());

  }
  // WL
}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, std::size_t level) const
{
  return (vaddr >> shamt(level)) & champsim::bitmask(champsim::lg2(pte_page_size / PTE_BYTES));
}

uint64_t VirtualMemory::ppage_front() const
{
  assert(available_ppages() > 0);
  return next_ppage;
}

void VirtualMemory::ppage_pop() { next_ppage += PAGE_SIZE; }

std::size_t VirtualMemory::available_ppages() const { return (last_ppage - next_ppage) / PAGE_SIZE; }

std::pair<uint64_t, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  cpu_num = 0; // WL: harded coded cpu_num to be 0

  // WL
  uint64_t translation;

  if(RECORD_IN_USE)
  {
    bool found = false;
    for(auto var : fr_vpage_to_ppage_map) {
      if (var.first.first == cpu_num && var.first.second == (vaddr >> LOG2_PAGE_SIZE)) {
        translation = var.second;
        found = true;
        break;
      } 
    }

    assert(found != false);
  }
  // WL
  else
    translation = ppage_front(); 

  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, translation});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
  {
    // WL
    if (RECORD_OR_READ) 
    {
      va_to_pa_file.open(va_to_pa_file_name, std::ofstream::app);
      va_to_pa_file << cpu_num << " " << (vaddr >> LOG2_PAGE_SIZE) << " " << translation << std::endl;
      va_to_pa_file.close();
    }

    if (RECORD_OR_READ)
      ppage_pop(); 
    // WL
  }

  auto paddr = champsim::splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE);
  if ((champsim::debug_print) && champsim::operable::cpu0_num_retired >= champsim::operable::number_of_instructions_to_skip_before_log) { // WL
    fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} fault: {}\n", __func__, paddr, vaddr, fault);
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}

std::pair<uint64_t, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, std::size_t level)
{
  cpu_num = 0; // WL: hard coded cpu_num, which is used as 
  if (next_pte_page == 0) {
    next_pte_page = ppage_front();
    ppage_pop();
  }

  if (vaddr == 24909557776) {
    std::cout << "vaddr match with level " << level << std::endl; 
  }

  // WL
  if(RECORD_IN_USE)
  {
    bool found = false;

    for(auto var : fr_page_table) {
      if (std::get<0>(var.first) == cpu_num && (std::get<1>(var.first) == (vaddr >> shamt(level))) && std::get<2>(var.first) == level) {
        found = true;
        next_pte_page = var.second;
        break;
      } 
    }

    if (!found) 
    {
      std::cout << "cpu_num = " << cpu_num << " vaddr = " << vaddr << " level = " << level << " vaddr_shifted = " << (vaddr >> (shamt(level))) << std::endl;  
      auto [ppage, fault] = va_to_pa(cpu_num, vaddr);
    }
    //assert(found != false);
  }
  // WL

  std::tuple key{cpu_num, vaddr >> shamt(level), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {

    // WL
    if (RECORD_OR_READ) 
    {
      page_table_file.open(page_table_file_name, std::ofstream::app);
      page_table_file << cpu_num << " " << (vaddr >> shamt(level)) << " " << level << " " << next_pte_page << std::endl;
      page_table_file.close();
    }
    // WL

    if (RECORD_OR_READ) { // WL
      next_pte_page += pte_page_size;
      if (!(next_pte_page % PAGE_SIZE)) {
        next_pte_page = ppage_front();
        ppage_pop();
      }
    }
  }

  auto offset = get_offset(vaddr, level);
  auto paddr = champsim::splice_bits(ppage->second, offset * PTE_BYTES, champsim::lg2(pte_page_size));
  if ((champsim::debug_print) && champsim::operable::cpu0_num_retired >= champsim::operable::number_of_instructions_to_skip_before_log) { // WL
    fmt::print("[VMEM] {} paddr: {:x} vaddr: {:x} pt_page_offset: {} translation_level: {} fault: {} asid: {}\n", __func__, paddr, vaddr, offset, level, fault, cpu_num);
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}
