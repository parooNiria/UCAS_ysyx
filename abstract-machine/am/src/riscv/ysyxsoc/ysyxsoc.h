#ifndef YSYXSOC_H__
#define YSYXSOC_H__

#include <klib-macros.h>
#include <riscv/riscv.h>

#define MROM_BASE 0x20000000
#define UART_BASE 0x10000000
#define CLINT_ADDR 0x02000000
#define SERIAL_PORT UART_BASE

#define SRAM_BASE 0x80000000
#define SRAM_SIZE 0x00400000
#define CLINT_SIZE 0x0000ffff
#define PMEM_END  (SRAM_BASE + SRAM_SIZE)

#endif