#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "ysyxsoc.h"

extern char _heap_start;
int main(const char *args);

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER);

#define UART_REG_THR 0x0
#define UART_REG_DLL 0x0
#define UART_REG_DLM 0x1
#define UART_REG_LCR 0x3
#define UART_LCR_DLAB 0x80
#define UART_LCR_8N1  0x03


void putch(char ch) {
  outb(SERIAL_PORT + UART_REG_THR, (uint8_t)ch);
}

void halt(int code) {
  asm volatile("mv a0, %0; ebreak" : : "r"(code));
  while (1) {
  }
}

void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}