#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "npc.h"

extern char _heap_start;
int main(const char *args);

Area heap = RANGE(&_heap_start, PMEM_END);
static const char mainargs[MAINARGS_MAX_LEN] = TOSTRING(MAINARGS_PLACEHOLDER); // defined in CFLAGS

void putch(char ch) {
  outb(SERIAL_PORT, (uint8_t)ch);
}

void halt(int code) {
  asm volatile("mv a0, %0; ebreak" : : "r"(code));
  while (1);
}

void _trm_init() {
  // uint32_t mvendorid = 0;
  // uint32_t marchid = 0;

  // asm volatile("csrr %0, mvendorid" : "=r"(mvendorid));
  // asm volatile("csrr %0, marchid" : "=r"(marchid));

  // printf("CSR mvendorid = 0x%08x, marchid = 0x%08x (%u)\n", mvendorid, marchid, marchid);

  int ret = main(mainargs);
  halt(ret);
}
