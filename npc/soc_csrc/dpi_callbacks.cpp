#include "include/dpi_callbacks.h"
#include "include/difftest.h"
#include <stdio.h>
#include "include/sim_env.h"
// Global instance set by SimEnv
static SimEnv* g_sim_env = NULL;
static DiffTest* g_Difftest = NULL;

// External accessor functions
bool& SimEnv_get_ebreak_triggered(SimEnv* env) { return env->ebreak_triggered_; }
int& SimEnv_get_ebreak_a0(SimEnv* env) { return env->ebreak_a0_; }
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env) { return env->mrom_image_; }

// DPI ebreak callback (extern "C" for C linkage)
extern "C" void dpi_ebreak(int reg_a0) {
  if (g_sim_env) {
    SimEnv_get_ebreak_triggered(g_sim_env) = true;
    SimEnv_get_ebreak_a0(g_sim_env) = reg_a0;
    printf("\n[EBREAK] DPI ebreak called with reg_a0 = %d! \n", reg_a0);
  }
}

extern "C" void dpi_commit(int dbg_mstatus, int dbg_mtvec, int dbg_mepc, int dbg_mcause,
    int dbg_rf0, int dbg_rf1, int dbg_rf2, int dbg_rf3, int dbg_rf4, int dbg_rf5, int dbg_rf6,
    int dbg_rf7, int dbg_rf8, int dbg_rf9, int dbg_rf10, int dbg_rf11, int dbg_rf12, int dbg_rf13, int dbg_rf14, int dbg_rf15,
    int dbg_rf16, int dbg_rf17, int dbg_rf18, int dbg_rf19, int dbg_rf20, int dbg_rf21, int dbg_rf22, int dbg_rf23, int dbg_rf24, int dbg_rf25, int dbg_rf26, int dbg_rf27, int dbg_rf28, int dbg_rf29, int dbg_rf30, int dbg_rf31,
    int last_inst,int next_pc,int last_pc, int device_type) {
      if (g_Difftest && g_Difftest->is_enabled()) {
    RiscvRegs npc_regs;
    npc_regs.gpr[0] = dbg_rf0;
    npc_regs.gpr[1] = dbg_rf1;
    npc_regs.gpr[2] = dbg_rf2;
    npc_regs.gpr[3] = dbg_rf3;
    npc_regs.gpr[4] = dbg_rf4;
    npc_regs.gpr[5] = dbg_rf5;
    npc_regs.gpr[6] = dbg_rf6;
    npc_regs.gpr[7] = dbg_rf7;
    npc_regs.gpr[8] = dbg_rf8;
    npc_regs.gpr[9] = dbg_rf9;
    npc_regs.gpr[10] = dbg_rf10;
    npc_regs.gpr[11] = dbg_rf11;
    npc_regs.gpr[12] = dbg_rf12;
    npc_regs.gpr[13] = dbg_rf13;
    npc_regs.gpr[14] = dbg_rf14;
    npc_regs.gpr[15] = dbg_rf15;
    npc_regs.gpr[16] = dbg_rf16;
    npc_regs.gpr[17] = dbg_rf17;
    npc_regs.gpr[18] = dbg_rf18;
    npc_regs.gpr[19] = dbg_rf19;
    npc_regs.gpr[20] = dbg_rf20;
    npc_regs.gpr[21] = dbg_rf21;
    npc_regs.gpr[22] = dbg_rf22;
    npc_regs.gpr[23] = dbg_rf23;
    npc_regs.gpr[24] = dbg_rf24;
    npc_regs.gpr[25] = dbg_rf25;
    npc_regs.gpr[26] = dbg_rf26;
    npc_regs.gpr[27] = dbg_rf27;
    npc_regs.gpr[28] = dbg_rf28;
    npc_regs.gpr[29] = dbg_rf29;
    npc_regs.gpr[30] = dbg_rf30;
    npc_regs.gpr[31] = dbg_rf31;
    npc_regs.mstatus = dbg_mstatus;
    npc_regs.mtvec = dbg_mtvec;
    npc_regs.mepc = dbg_mepc;
    npc_regs.mcause = dbg_mcause;
    npc_regs.pc = next_pc;

    // Call step and check for errors
    if (!g_Difftest->step(npc_regs, last_inst, last_pc, device_type)) {
      // Error detected - set stop flag in SimEnv
      if (g_sim_env) {
        SimEnv_set_stop_flag(g_sim_env) = true;
      }
    }
  }
}

// MROM read for DPI - reads from loaded MROM image
extern "C" void mrom_read(int32_t addr, int32_t *data) {
  if (!g_sim_env || !data) {
    *data = 0;
    return;
  }
  
  std::vector<uint8_t>& mrom = SimEnv_get_mrom_image(g_sim_env);
  
  // Boundary check
  if (addr < kMromBase || addr + 3 >= kMromBase + kMromSize) {
    printf(COLOR_RED "[MROM] Read out of bounds: 0x%08x" COLOR_RESET "\n", addr);
    *data = 0;
    return;
  }
  int32_t addr_aligned = addr & ~3;
  uint32_t offset = addr_aligned - kMromBase;
  *data = (int32_t)(
    mrom[offset + 0] |
    (mrom[offset + 1] << 8) |
    (mrom[offset + 2] << 16) |
    (mrom[offset + 3] << 24)
  );
}

extern "C" void dpi_init(int dbg_mstatus, int dbg_mtvec, int dbg_mepc, int dbg_mcause,int dbg_pc) {
  if (g_Difftest && g_Difftest->is_enabled()) {
    RiscvRegs npc_regs;
    npc_regs.mstatus = dbg_mstatus;
    npc_regs.mtvec = dbg_mtvec;
    npc_regs.mepc = dbg_mepc;
    npc_regs.mcause = dbg_mcause;
    for(int i=0; i<32; i++) {
      npc_regs.gpr[i] = 0;
    }
    npc_regs.pc = dbg_pc;
    g_Difftest->set_nemu_init(&npc_regs);
  }
}
// Flash read DPI callback
// Reads 32-bit data from the simulated flash array
extern "C" void flash_read(int32_t addr, int32_t *data) {
  if (!g_sim_env || !data) {
    *data = 0;
    return;
  }

  std::vector<uint8_t>& flash = SimEnv_get_flash(g_sim_env);
  uint32_t flash_addr = static_cast<uint32_t>(addr) & ~3;  // Align to 4 bytes

  // Boundary check
  constexpr uint32_t kFlashSize = 0x01000000u;  // 16MB
  if (flash_addr + 3 >= kFlashSize) {
    printf(COLOR_RED "[FLASH] Read out of bounds: 0x%08x" COLOR_RESET "\n", flash_addr);
    *data = 0xFFFFFFFFu;  // Return all 1s for out-of-bounds (like erased flash)
    return;
  }

  // Read 32-bit word from flash (little-endian)
  *data = (int32_t)(
    flash[flash_addr + 0] |
    (flash[flash_addr + 1] << 8) |
    (flash[flash_addr + 2] << 16) |
    (flash[flash_addr + 3] << 24)
  );
}

// Set global SimEnv instance
void set_sim_env_instance(SimEnv* env) {
  g_sim_env = env;
}

void set_diff_test_instance(DiffTest* diff_test) {
  g_Difftest = diff_test;
}
