#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <stdint.h>

struct diff_context_t {
  uint32_t gpr[32];
  uint32_t mstatus;
  uint32_t mepc;
  uint32_t mcause;
  uint32_t mtvec;
  uint32_t pc;
};

#endif
