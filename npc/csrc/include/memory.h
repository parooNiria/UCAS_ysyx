#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "common.h"

#ifndef PMEM_BASE
#define PMEM_BASE 0x80000000u
#endif

#ifndef PMEM_SIZE
#define PMEM_SIZE (128u * 1024u * 1024u)
#endif

extern const uint32_t kPmemBase;
extern const uint32_t kPmemSize;
extern uint8_t *pmem;
extern bool g_mmio_accessed;

bool in_pmem(uint32_t addr);
int pmem_read_internal(uint32_t addr);
int pmem_read(int raddr);
extern "C" void pmem_write(int waddr, int wdata, char wmask);

void scan_memory(uint32_t addr, int len);
long load_img(const char *img_file);

#endif
