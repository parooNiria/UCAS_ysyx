#ifndef YSYXSOC_H__
#define YSYXSOC_H__

#include <klib-macros.h>
#include <riscv/riscv.h>

#define MROM_BASE 0x20000000
#define UART_BASE 0x10000000
#define CLINT_ADDR 0x02000000
#define SERIAL_PORT UART_BASE

#define VGA_FB_BASE 0x21000000
#define VGA_FB_W    640
#define VGA_FB_H    480

#define GPIO_BASE 0x10002000
#define GPIO_LED   (GPIO_BASE + 0x0)  // 16-bit LED output (write)
#define GPIO_SW    (GPIO_BASE + 0x4)  // 16-bit switch input (read)
#define GPIO_SEG   (GPIO_BASE + 0x8)  // 32-bit 7-segment (write, 4-bit/hex per digit)

#define SRAM_BASE 0x80000000   // PSRAM (QSPI IS66WVS4M8ALL, 4MB)
#define SRAM_SIZE 0x00400000

#define SDRAM_BASE 0xa0000000  // SDRAM (MT48LC16M16A2, 32MB)
#define SDRAM_SIZE 0x02000000

#define CLINT_SIZE 0x0000ffff

// PMEM_END is determined by the linker script's _stack_top,
// which sits at the top of the active memory region (PSRAM or SDRAM).
extern char _stack_top;
#define PMEM_END  ((uintptr_t)&_stack_top)

#endif