#include "include/Context.h"
#include "include/memory.h"
#include "include/common.h"
#include <dlfcn.h>
#include <svdpi.h>

void (*ref_difftest_memcpy)(uint32_t addr, void *buf, size_t n, bool direction) = NULL;
void (*ref_difftest_regcpy)(void *dut, bool direction) = NULL;
void (*ref_difftest_exec)(uint64_t n) = NULL;
void (*ref_difftest_raise_intr)(uint64_t NO) = NULL;
void (*ref_difftest_init)(int port) = NULL;

enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

void init_difftest(const char *ref_so_file, long img_size, int port) {
  assert(ref_so_file != NULL);
  void *handle = dlopen(ref_so_file, RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "dlopen failed: %s\n", dlerror());
    assert(0);
  }

  ref_difftest_memcpy = (void (*)(uint32_t, void*, size_t, bool))dlsym(handle, "difftest_memcpy");
  assert(ref_difftest_memcpy);

  ref_difftest_regcpy = (void (*)(void*, bool))dlsym(handle, "difftest_regcpy");
  assert(ref_difftest_regcpy);

  ref_difftest_exec = (void (*)(uint64_t))dlsym(handle, "difftest_exec");
  assert(ref_difftest_exec);

  ref_difftest_raise_intr = (void (*)(uint64_t))dlsym(handle, "difftest_raise_intr");
  assert(ref_difftest_raise_intr);

  ref_difftest_init = (void (*)(int))dlsym(handle, "difftest_init");
  assert(ref_difftest_init);

  ref_difftest_init(port);
  ref_difftest_memcpy(0x80000000, pmem, img_size, DIFFTEST_TO_REF);

  diff_context_t cpu;
  svSetScope(svGetScopeFromName("TOP.top"));
  for (int i = 0; i < 32; i++) cpu.gpr[i] = read_register(i);
  cpu.pc = 0x80000000;
  cpu.mstatus = read_csr(0x300);
  cpu.mtvec   = read_csr(0x305);
  cpu.mepc    = read_csr(0x341);
  cpu.mcause  = read_csr(0x342);
  ref_difftest_regcpy(&cpu, DIFFTEST_TO_REF);
}

bool check_difftest(uint32_t npc_pc, uint32_t npc_next_pc, bool skip_compare) {
  if (ref_difftest_exec == NULL) {
    printf("Warning: ref_difftest_exec is NULL, cannot perform difftest check.\n"); 
    return true;
  }
  if (skip_compare) {
    diff_context_t dut_cpu;
    svSetScope(svGetScopeFromName("TOP.top"));
    for (int i = 0; i < 32; i++) dut_cpu.gpr[i] = read_register(i);
    dut_cpu.pc = npc_next_pc;
    dut_cpu.mstatus = read_csr(0x300);
    dut_cpu.mtvec   = read_csr(0x305);
    dut_cpu.mepc    = read_csr(0x341);
    dut_cpu.mcause  = read_csr(0x342);
    ref_difftest_regcpy(&dut_cpu, DIFFTEST_TO_REF);
    return true;
  }

  ref_difftest_exec(1);

  diff_context_t ref_cpu;
  ref_difftest_regcpy(&ref_cpu, DIFFTEST_TO_DUT);
  bool match = true;
  svSetScope(svGetScopeFromName("TOP.top"));
  for (int i = 0; i < 32; i++) {
    uint32_t my_reg = read_register(i);
    if (ref_cpu.gpr[i] != my_reg) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("Reg %02d differ! ref=0x%08x, my_reg=0x%08x\n", i, ref_cpu.gpr[i], my_reg);
      match = false;
    }
  }
  if (ref_cpu.pc != npc_next_pc) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("PC differ! ref=0x%08x, my_npc=0x%08x\n", ref_cpu.pc, npc_next_pc);
      match = false;
  }
  uint32_t my_mstatus = read_csr(0x300);
  uint32_t my_mtvec   = read_csr(0x305);
  uint32_t my_mepc    = read_csr(0x341);
  uint32_t my_mcause  = read_csr(0x342);

  if (ref_cpu.mstatus != my_mstatus) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("CSR mstatus differ! ref=0x%08x, my_csr=0x%08x\n", ref_cpu.mstatus, my_mstatus);
      match = false;
  }
  if (ref_cpu.mtvec != my_mtvec) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("CSR mtvec differ! ref=0x%08x, my_csr=0x%08x\n", ref_cpu.mtvec, my_mtvec);
      match = false;
  }
  if (ref_cpu.mepc != my_mepc) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("CSR mepc differ! ref=0x%08x, my_csr=0x%08x\n", ref_cpu.mepc, my_mepc);
      match = false;
  }
  if (ref_cpu.mcause != my_mcause) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("CSR mcause differ! ref=0x%08x, my_csr=0x%08x\n", ref_cpu.mcause, my_mcause);
      match = false;
  }
  return match;
}

