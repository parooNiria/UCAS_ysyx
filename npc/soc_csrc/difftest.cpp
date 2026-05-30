#include "include/difftest.h"
#include "include/trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Color definitions for output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

static const char *regs[32] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};
// Global instance for DPI callbacks
static DiffTest* g_diff_test = NULL;

DiffTest::DiffTest()
  : nemu_handle_(NULL)
  , enabled_(false)
  , inst_count_(0)
  , has_error_(false)
{
  g_diff_test = this;
}

DiffTest::~DiffTest() {
  cleanup();
}

bool DiffTest::init(const char *ref_so_file) {
  printf("[DIFFTEST] Loading NEMU shared library: %s\n", ref_so_file);
  nemu_handle_ = dlopen(ref_so_file, RTLD_LAZY);
  if (!nemu_handle_) {
    printf(COLOR_CYAN "[WARN] Failed to load NEMU: %s, DiffTest disabled" COLOR_RESET "\n",
           dlerror());
    return false;
  }
  // Load difftest_init
  difftest_init_t nemu_difftest_init = 
    (difftest_init_t)dlsym(nemu_handle_, "difftest_init");
  if (!nemu_difftest_init) {
    printf(COLOR_CYAN "[WARN] difftest_init not found, DiffTest disabled" COLOR_RESET "\n");
    return false;
  }
  
  // Load difftest_memcpy
  nemu_difftest_memcpy_ = 
    (difftest_memcpy_t)dlsym(nemu_handle_, "difftest_memcpy");
  assert(nemu_difftest_memcpy_);
  
  // Load difftest_regcpy
  nemu_difftest_regcpy_ = 
    (difftest_regcpy_t)dlsym(nemu_handle_, "difftest_regcpy");
  assert(nemu_difftest_regcpy_);
  
  // Load difftest_exec
  nemu_difftest_exec_ = 
    (difftest_exec_t)dlsym(nemu_handle_, "difftest_exec");
  assert(nemu_difftest_exec_);
  // Initialize NEMU
  nemu_difftest_init(0);
  enabled_ = true; 
  printf( "[DIFFTEST] DiffTest enabled with NEMU\n");
  return true;
}

void DiffTest::cleanup() {
  if (nemu_handle_) {
    dlclose(nemu_handle_);
    nemu_handle_ = NULL;
  }
}

void DiffTest::sync_mrom(uint32_t addr, const void *buf, size_t size) {
  if (!enabled_ || !nemu_difftest_memcpy_) {
    return;
  }
  
  nemu_difftest_memcpy_(addr, const_cast<void*>(buf), size, DIFFTEST_TO_REF);
  printf("[DIFFTEST] Synced MROM to NEMU (%zu bytes)\n", size);
}

bool DiffTest::step(const RiscvRegs &npc_regs, uint32_t inst, int last_pc, int device_type) {
  if (!enabled_) {
    return true;  // No error if disabled
  }

  // Itrace: log the committed instruction
  log_itrace(last_pc, inst, false, false);

  // Device access: NEMU doesn't model peripherals (UART, SPI, GPIO, etc.)
  // Skip comparison and sync NPC's register state to NEMU
  if (device_type) {
    // Sync NPC register state to NEMU so they stay consistent
    // NEMU does NOT execute this instruction — we just overwrite its state
    nemu_difftest_regcpy_((void*)&npc_regs, DIFFTEST_TO_REF);
    inst_count_++;
    return true;
  }

  // Normal path: NEMU executes one instruction
  nemu_difftest_exec_(1);

  // Step 2: Read NEMU register state
  RiscvRegs nemu_regs;
  memset(&nemu_regs, 0, sizeof(nemu_regs));
  nemu_difftest_regcpy_(&nemu_regs, DIFFTEST_TO_DUT);

  // Step 3: Compare NPC and NEMU state
  inst_count_++;
  bool match = compare_regs(npc_regs, nemu_regs, inst, last_pc);

  if (!match) {
    has_error_ = true;
    display_error_state(npc_regs, nemu_regs, inst, last_pc);
    return false;
  }

  return true;
}

bool DiffTest::compare_regs(const RiscvRegs &npc, const RiscvRegs &nemu,int last_inst, int last_pc) {
  bool match = true;
  for (int i = 0; i < 32; i++) {
    if (npc.gpr[i] != nemu.gpr[i]) {
      printf(COLOR_RED "[DIFFTEST_ERROR] GPR[%02d] differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, i, npc.gpr[i], nemu.gpr[i]);
      match = false;
    }
  }
  if (npc.pc != nemu.pc) {
    printf(COLOR_RED "[DIFFTEST_ERROR] NPC differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, npc.pc, nemu.pc);
    match = false;
  }
  if (npc.mstatus != nemu.mstatus) {
    printf(COLOR_RED "[DIFFTEST_ERROR] mstatus differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, npc.mstatus, nemu.mstatus);
    match = false;
  }
  if (npc.mtvec != nemu.mtvec) {
    printf(COLOR_RED "[DIFFTEST_ERROR] mtvec differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, npc.mtvec, nemu.mtvec);
    match = false;
  }
  if (npc.mepc != nemu.mepc) {
    printf(COLOR_RED "[DIFFTEST_ERROR] mepc differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, npc.mepc, nemu.mepc);
    match = false;
  }
  if (npc.mcause != nemu.mcause) {
    printf(COLOR_RED "[DIFFTEST_ERROR] mcause differ! NPC=0x%08x, NEMU=0x%08x\n" COLOR_RESET, npc.mcause, nemu.mcause);
    match = false;
  } 
  if(!match) {
    printf(COLOR_RED "[DIFFTEST] State mismatch at instruction 0x%08x (inst=0x%08x)" COLOR_RESET "\n", last_pc, last_inst);
  }
  return match;
}

void DiffTest::display_error_state(const RiscvRegs &npc_regs, const RiscvRegs &nemu_regs, 
                                   uint32_t inst, int last_pc) {
  printf("\n");
  printf("====================================== CPU Registers ======================================\n");
  for (int i = 0; i < 32; i++) {
    printf("%4s: 0x%08x\t", regs[i], npc_regs.gpr[i]);
    if ((i + 1) % 4 == 0) printf("\n");
  }
  printf("====================================== CSR Registers ======================================\n");
  printf("mstatus: 0x%08x\tmtvec: 0x%08x\tmepc: 0x%08x\tmcause: 0x%08x\n", 
         npc_regs.mstatus,npc_regs.mtvec, npc_regs.mepc, npc_regs.mcause);
  printf("===========================================================================================\n");
  
  // Display recent instructions from itrace
  display_recent_itrace();
  printf("\n");
}

// Global accessor for DPI callbacks
DiffTest* get_diff_test_instance() {
  return g_diff_test;
}

void DiffTest::set_nemu_init(RiscvRegs *regs) {
  if (!enabled_ || !nemu_difftest_regcpy_) {
    return;
  }
  nemu_difftest_regcpy_(regs, DIFFTEST_TO_REF);
}
