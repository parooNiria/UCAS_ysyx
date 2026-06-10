/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <difftest-def.h>
#include <memory/paddr.h>
#include <string.h>
void init_log(const char *log_file);

__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
#ifdef CONFIG_YSYXSOC
    if (in_flash(addr)) {
      uint8_t* flash_host = flash_get_host();
      memcpy(flash_host + (addr - FLASH_BASE), buf, n);
    } else if (in_sram(addr)) {
      uint8_t* sram_host = sram_get_host();
      memcpy(sram_host + (addr - SRAM_BASE), buf, n);
    } else if (in_sdram(addr)) {
      uint8_t* sdram_host = sdram_get_host();
      memcpy(sdram_host + (addr - SDRAM_BASE), buf, n);
    } else
#endif
    if (in_pmem(addr)) {
      memcpy(guest_to_host(addr), buf, n);
    } else {
      panic("difftest_memcpy: address " FMT_PADDR " out of bounds", addr);
    }
  } else {
#ifdef CONFIG_YSYXSOC
    if (in_flash(addr)) {
      uint8_t* flash_host = flash_get_host();
      memcpy(buf, flash_host + (addr - FLASH_BASE), n);
    } else if (in_sram(addr)) {
      uint8_t* sram_host = sram_get_host();
      memcpy(buf, sram_host + (addr - SRAM_BASE), n);
    } else if (in_sdram(addr)) {
      uint8_t* sdram_host = sdram_get_host();
      memcpy(buf, sdram_host + (addr - SDRAM_BASE), n);
    } else
#endif
    if (in_pmem(addr)) {
      memcpy(buf, guest_to_host(addr), n);
    } else {
      panic("difftest_memcpy: address " FMT_PADDR " out of bounds", addr);
    }
  }
}

__EXPORT void difftest_regcpy(void *dut, bool direction) {
  if (direction == DIFFTEST_TO_REF) {
    memcpy(&cpu, dut, DIFFTEST_REG_SIZE);
  } else {
    memcpy(dut, &cpu, DIFFTEST_REG_SIZE);
  }
}

__EXPORT void difftest_exec(uint64_t n) {
  cpu_exec(n);
}

__EXPORT void difftest_raise_intr(word_t NO) {
  // Not used for now as per instructions
}

__EXPORT void difftest_init(int port) {
  void init_mem();
  init_mem();
  // #ifdef CONFIG_MTRACE
  // init_log("memtrace.log");
  // #endif
  /* Perform ISA dependent initialization. */
  init_isa();
}
