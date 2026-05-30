#ifndef __DIFFTEST_H__
#define __DIFFTEST_H__

#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

// DiffTest direction enum
enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

// RISC-V register structure for difftest (must match NEMU's CPU_state)
struct RiscvRegs {
  uint32_t gpr[32];    // General purpose registers
  uint32_t mstatus;    // Machine status CSR
  uint32_t mepc;       // Machine exception program counter
  uint32_t mcause;     // Machine cause register
  uint32_t mtvec;      // Machine trap-vector base address
  uint32_t pc;         // Program counter
};

// DiffTest function pointer types
typedef void (*difftest_memcpy_t)(uint64_t addr, void *buf, size_t n, bool direction);
typedef void (*difftest_regcpy_t)(void *dut, bool direction);
typedef void (*difftest_exec_t)(uint64_t n);
typedef void (*difftest_init_t)(int port);

// DiffTest class - handles comparison with NEMU
class DiffTest {
  friend DiffTest* get_diff_test_instance();
public:
  DiffTest();
  ~DiffTest();

  // Initialize DiffTest with NEMU shared library
  bool init(const char *ref_so_file);

  void set_nemu_init(RiscvRegs *regs);
  
  // Cleanup resources
  void cleanup();
  
  // Sync MROM content to NEMU
  void sync_mrom(uint32_t addr, const void *buf, size_t size);
  
  // Step: NEMU executes one instruction and compare with NPC
  // device_type: non-zero means this instruction accessed a device/peripheral
  //   that NEMU doesn't model — skip comparison and sync NPC->NEMU instead
  // Returns: true if no error, false if mismatch detected
  bool step(const RiscvRegs &npc_regs, uint32_t inst, int last_pc, int device_type);
  
  // Check if DiffTest is enabled
  bool is_enabled() const { return enabled_; }
  
  // Get instruction count
  uint64_t get_inst_count() const { return inst_count_; }
  
  // Check if error has been detected
  bool has_error() const { return has_error_; }
  
  // Display error state with register comparison and recent instructions
  void display_error_state(const RiscvRegs &npc_regs, const RiscvRegs &nemu_regs, 
                          uint32_t inst, int last_pc);

private:
  // Compare NPC and NEMU register states
  // Returns: true if match, false if mismatch
  bool compare_regs(const RiscvRegs &npc, const RiscvRegs &nemu, int last_inst, int last_pc);

  // NEMU shared library handle
  void* nemu_handle_;
  
  // DiffTest function pointers
  bool enabled_;
  difftest_memcpy_t nemu_difftest_memcpy_;
  difftest_regcpy_t nemu_difftest_regcpy_;
  difftest_exec_t nemu_difftest_exec_;
  
  // Statistics
  uint64_t inst_count_;
  
  // Error tracking
  bool has_error_;
};

// Global accessor for DPI callbacks
DiffTest* get_diff_test_instance();

#endif // __DIFFTEST_H__
