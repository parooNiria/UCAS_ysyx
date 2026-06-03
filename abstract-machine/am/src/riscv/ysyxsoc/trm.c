#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "ysyxsoc.h"

extern char _heap_start;
int main(const char *args);

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER);

#define UART_REG_THR 0x0
#define UART_REG_RBR 0x0
#define UART_REG_DLL 0x0
#define UART_REG_IER 0x1
#define UART_REG_DLM 0x1
#define UART_REG_IIR 0x2
#define UART_REG_FCR 0x2
#define UART_REG_LCR 0x3
#define UART_REG_MCR 0x4
#define UART_REG_LSR 0x5
#define UART_REG_MSR 0x6
#define UART_REG_SCR 0x7

#define UART_LCR_DLAB 0x80
#define UART_LCR_8N1  0x03

#define UART_LSR_THRE 0x20
#define UART_LSR_TEMT 0x40

#define inb(addr) (*(volatile uint8_t *)(addr))

void putch(char ch) {
  // Wait for THRE (bit 5 of LSR): transmitter holding register empty
  while ((inb(SERIAL_PORT + UART_REG_LSR) & UART_LSR_THRE) == 0);
  outb(SERIAL_PORT + UART_REG_THR, (uint8_t)ch);
}

void halt(int code) {
  // Wait for TEMT (bit 6 of LSR): transmitter completely idle
  while ((inb(SERIAL_PORT + UART_REG_LSR) & UART_LSR_TEMT) == 0);
  asm volatile("mv a0, %0; ebreak" : : "r"(code));
  while (1) {
  }
}

void uart_init() {
    /*
   * UART16550 Initialization
   *
   * Baud rate formula:  divisor = clk_freq / (baud_rate * 16)
   *   Sim: divisor = 1 (fastest, enable every clock cycle)
   *   FPGA 50MHz->115200: divisor = 50000000/(115200*16) ≈ 27
   */

  // Step 1: Set DLAB=1 to access divisor latches (DLL/DLM)
  outb(SERIAL_PORT + UART_REG_LCR, UART_LCR_DLAB);

  // Step 2: Write divisor latch (DLL @ addr0, DLM @ addr1 when DLAB=1)
  uint16_t divisor = 1;  // Use 1 for simulation; adjust for FPGA clock
  outb(SERIAL_PORT + UART_REG_DLL, (uint8_t)(divisor & 0xFF));
  outb(SERIAL_PORT + UART_REG_DLM, (uint8_t)((divisor >> 8) & 0xFF));

  // Step 3: Clear DLAB, set 8 data bits, no parity, 1 stop bit (8N1)
  outb(SERIAL_PORT + UART_REG_LCR, UART_LCR_8N1);
}

void _trm_init() {
  
  uart_init();
  uint32_t vendor, arch;
  asm volatile("csrr %0, 0xF11" : "=r"(vendor));
  asm volatile("csrr %0, 0xF12" : "=r"(arch));
  for (const char *p = (const char *)&vendor + 3; p >= (const char *)&vendor; p--)
    putch(*p);
  printf("_%d\n", arch);
  int ret = main(mainargs);
  halt(ret);
}
