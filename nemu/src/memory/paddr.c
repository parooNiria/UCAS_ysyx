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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>

#ifndef MTRACE_COND
#define MTRACE_COND true
#endif

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

// Flash (XIP boot) and SRAM memory regions
static uint8_t flash[FLASH_SIZE] PG_ALIGN = {};
static uint8_t sram[SRAM_SIZE] PG_ALIGN = {};

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

// Flash and SRAM host access functions
uint8_t* flash_get_host(void) { return flash; }
uint8_t* sram_get_host(void) { return sram; }

void flash_sync_from_host(const void *data, size_t size) {
  assert(size <= FLASH_SIZE);
  memcpy(flash, data, size);
  Log("Synced flash with %zu bytes", size);
}

static word_t flash_read(paddr_t addr, int len) {
  word_t ret = host_read(flash + addr - FLASH_BASE, len);
  return ret;
}

static void flash_write(paddr_t addr, int len, word_t data) {
  // Flash is read-only via XIP, ignore writes
  Log("Warning: Write to flash at " FMT_PADDR " ignored", addr);
}

static word_t sram_read(paddr_t addr, int len) {
  word_t ret = host_read(sram + addr - SRAM_BASE, len);
  return ret;
}

static void sram_write(paddr_t addr, int len, word_t data) {
  host_write(sram + addr - SRAM_BASE, len, data);
}

static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr) {
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

#ifdef CONFIG_MTRACE
static inline bool mtrace_addr_hit(paddr_t addr, int len) {
#ifdef CONFIG_MTRACE_ADDR_RANGE
  uint64_t left = (uint64_t)(paddr_t)CONFIG_MTRACE_ADDR_START;
  uint64_t right = (uint64_t)(paddr_t)CONFIG_MTRACE_ADDR_END;
  uint64_t start = (uint64_t)addr;
  uint64_t end = start + (uint64_t)len - 1;
  return !(end < left || start > right);
#else
  return true;
#endif
}

static inline bool mtrace_on(paddr_t addr, int len) {
  return MTRACE_COND && mtrace_addr_hit(addr, len);
}

static inline void mtrace_read_log(paddr_t addr, int len, word_t data) {
  if (mtrace_on(addr, len)) {
    log_write("[mtrace] R pc=" FMT_WORD " addr=" FMT_PADDR " len=%d data=" FMT_WORD "\n",
        cpu.pc, addr, len, data);
  }
}

static inline void mtrace_write_log(paddr_t addr, int len, word_t data) {
  if (mtrace_on(addr, len)) {
    log_write("[mtrace] W pc=" FMT_WORD " addr=" FMT_PADDR " len=%d data=" FMT_WORD "\n",
        cpu.pc, addr, len, data);
  }
}
#endif

void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  memset(flash, 0, FLASH_SIZE);
  memset(sram, 0, SRAM_SIZE);
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
}

word_t paddr_read(paddr_t addr, int len) {
  word_t ret = 0;

  if (likely(in_flash(addr))) {
    ret = flash_read(addr, len);
#ifdef CONFIG_MTRACE
    mtrace_read_log(addr, len, ret);
#endif
    return ret;
  }

  if (likely(in_sram(addr))) {
    ret = sram_read(addr, len);
#ifdef CONFIG_MTRACE
    mtrace_read_log(addr, len, ret);
#endif
    return ret;
  }

  if (likely(in_pmem(addr))) {
    ret = pmem_read(addr, len);
#ifdef CONFIG_MTRACE
    mtrace_read_log(addr, len, ret);
#endif
    return ret;
  }

#ifdef CONFIG_DEVICE
  ret = mmio_read(addr, len);
#ifdef CONFIG_MTRACE
  mtrace_read_log(addr, len, ret);
#endif
  return ret;
#endif

  out_of_bound(addr);
  return 0;
}

void paddr_write(paddr_t addr, int len, word_t data) {
  if (likely(in_flash(addr))) {
    flash_write(addr, len, data);
#ifdef CONFIG_MTRACE
    mtrace_write_log(addr, len, data);
#endif
    return;
  }

  if (likely(in_sram(addr))) {
    sram_write(addr, len, data);
#ifdef CONFIG_MTRACE
    mtrace_write_log(addr, len, data);
#endif
    return;
  }

  if (likely(in_pmem(addr))) {
    pmem_write(addr, len, data);
#ifdef CONFIG_MTRACE
    mtrace_write_log(addr, len, data);
#endif
    return;
  }
  if (likely(in_uart_space(addr))) {
    return;
  }

#ifdef CONFIG_DEVICE
  mmio_write(addr, len, data);
#ifdef CONFIG_MTRACE
  mtrace_write_log(addr, len, data);
#endif
  return;
#endif

  out_of_bound(addr);
}
