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

#ifdef CONFIG_YSYXSOC
// Flash XIP address space (replaces MROM as boot device)
#define FLASH_BASE  ((paddr_t)0x30000000)
#define FLASH_SIZE  ((paddr_t)0x1000000)  // 16MB flash
#define SRAM_BASE  ((paddr_t)0x0f000000)
#define SRAM_SIZE  ((paddr_t)0x2000)
#define SDRAM_BASE ((paddr_t)0xa0000000)
#define SDRAM_SIZE ((paddr_t)0x04000000)  // 32MB MT48LC16M16A2
#define UART_BASE  ((paddr_t)0x10000000)
#define UART_SIZE  ((paddr_t)0x1000)
#endif

// Memory region check functions
static inline bool in_pmem(paddr_t addr) {
  return addr - CONFIG_MBASE < CONFIG_MSIZE;
}

#ifdef CONFIG_YSYXSOC
static inline bool in_flash(paddr_t addr) {
  return addr - FLASH_BASE < FLASH_SIZE;
}

static inline bool in_sram(paddr_t addr) {
  return addr - SRAM_BASE < SRAM_SIZE;
}

static inline bool in_sdram(paddr_t addr) {
  return addr - SDRAM_BASE < SDRAM_SIZE;
}

static inline bool in_uart_space(paddr_t addr) {
  return addr - UART_BASE < UART_SIZE;
}
#endif
/* convert the guest physical address in the guest program to host virtual address in NEMU */
uint8_t* guest_to_host(paddr_t paddr);
/* convert the host virtual address in NEMU to guest physical address in the guest program */
paddr_t host_to_guest(uint8_t *haddr);

#ifdef CONFIG_YSYXSOC
// Flash, SRAM, and SDRAM access functions
uint8_t* flash_get_host(void);
uint8_t* sram_get_host(void);
uint8_t* sdram_get_host(void);
void flash_sync_from_host(const void *data, size_t size);
#endif

word_t paddr_read(paddr_t addr, int len);
void paddr_write(paddr_t addr, int len, word_t data);

#endif
