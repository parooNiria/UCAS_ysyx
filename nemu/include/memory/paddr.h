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

#ifndef __MEMORY_PADDR_H__
#define __MEMORY_PADDR_H__

#include <common.h>

#define PMEM_LEFT  ((paddr_t)CONFIG_MBASE)
#define PMEM_RIGHT ((paddr_t)CONFIG_MBASE + CONFIG_MSIZE - 1)
#define RESET_VECTOR (PMEM_LEFT + CONFIG_PC_RESET_OFFSET)

// MROM and SRAM address space definitions
#define MROM_BASE  ((paddr_t)0x20000000)
#define MROM_SIZE  ((paddr_t)0x1000)
#define SRAM_BASE  ((paddr_t)0x0f000000)
#define SRAM_SIZE  ((paddr_t)0x2000)
#define UART_BASE  ((paddr_t)0x10000000)
#define UART_SIZE  ((paddr_t)0x1000)

// Memory region check functions
static inline bool in_pmem(paddr_t addr) {
  return addr - CONFIG_MBASE < CONFIG_MSIZE;
}

static inline bool in_mrom(paddr_t addr) {
  return addr - MROM_BASE < MROM_SIZE;
}

static inline bool in_sram(paddr_t addr) {
  return addr - SRAM_BASE < SRAM_SIZE;
}

static inline bool in_uart_space(paddr_t addr) {
  return addr - UART_BASE < UART_SIZE;
}
/* convert the guest physical address in the guest program to host virtual address in NEMU */
uint8_t* guest_to_host(paddr_t paddr);
/* convert the host virtual address in NEMU to guest physical address in the guest program */
paddr_t host_to_guest(uint8_t *haddr);

// MROM and SRAM access functions
uint8_t* mrom_get_host(void);
uint8_t* sram_get_host(void);
void mrom_sync_from_host(const void *data, size_t size);

word_t paddr_read(paddr_t addr, int len);
void paddr_write(paddr_t addr, int len, word_t data);

#endif
