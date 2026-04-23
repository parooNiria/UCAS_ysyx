#ifndef NPC_H__
#define NPC_H__

#include <riscv/riscv.h>

#define DEVICE_BASE 0xa0000000

#define SERIAL_PORT (DEVICE_BASE + 0x00003f8)

extern char _pmem_start;
#define PMEM_SIZE (128 * 1024 * 1024)
#define PMEM_END  ((uintptr_t)&_pmem_start + PMEM_SIZE)

#endif