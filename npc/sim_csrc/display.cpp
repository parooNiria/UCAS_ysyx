#include "include/common.h"
#include "include/trace.h"
#include <svdpi.h>
#include <stdio.h>
#include <stdint.h>

static const char *regs[32] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void print_registers() {
  svSetScope(svGetScopeFromName("TOP.top"));
  printf("====================================== CPU Registers ======================================\n");
  for (int i = 0; i < 32; i++) {
    printf("%4s: 0x%08x\t", regs[i], read_register(i));
    if ((i + 1) % 4 == 0) printf("\n");
  }
  printf("====================================== CSR Registers ======================================\n");
  printf("mstatus: 0x%08x\tmtvec: 0x%08x\tmepc: 0x%08x\tmcause: 0x%08x\n", 
         read_csr(0x300), read_csr(0x305), read_csr(0x341), read_csr(0x342));
  printf("===========================================================================================\n");
}

void display_trap_info(uint32_t pc, int a0, const char *reason) {
  printf("\n\033[1;31m[CPU STATE DUMP]\033[0m\n");
  printf("Trap Reason: \033[1;31m%s\033[0m\n", reason);
  printf("Error PC: 0x%08x\n", pc);
  printf("A0 (return/status): 0x%08x\n\n", a0);
  print_registers();
  display_recent_itrace();
  printf("\033[1;31m[ABORT] Simulator stopped due to error.\033[0m\n\n");
}
