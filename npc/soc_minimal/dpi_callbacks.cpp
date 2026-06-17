#include "dpi_callbacks.h"
#include "sim_env.h"
#include <stdio.h>

static SimEnv* g_sim_env = NULL;

void set_sim_env_instance(SimEnv* env) { g_sim_env = env; }

// ── Accessors ───────────────────────────────────────────────────────────────

bool& SimEnv_get_ebreak_triggered(SimEnv* env) { return env->ebreak_triggered_; }
int&  SimEnv_get_ebreak_a0(SimEnv* env)       { return env->ebreak_a0_; }
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env) { return env->mrom_image_; }

// ═══════════════════════════════════════════════════════════════════════════════
//  REAL DPI CALLBACKS (memory + ebreak)
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" void dpi_ebreak(int reg_a0) {
  if (g_sim_env) {
    SimEnv_get_ebreak_triggered(g_sim_env) = true;
    SimEnv_get_ebreak_a0(g_sim_env) = reg_a0;
    printf("\n[EBREAK] a0 = %d\n", reg_a0);
  }
}

// ── Flash XIP read ──────────────────────────────────────────────────────────

extern "C" void flash_read(int32_t addr, int32_t *data) {
  if (!g_sim_env || !data) { *data = 0; return; }

  std::vector<uint8_t>& flash = SimEnv_get_flash(g_sim_env);
  uint32_t flash_addr = (uint32_t)addr & ~3u;

  static constexpr uint32_t kFlSize = 0x01000000u;  // 16 MB
  if (flash_addr + 3 >= kFlSize) {
    *data = -1;   // 0xFFFFFFFF — erased flash state
    return;
  }

  *data = (int32_t)(flash[flash_addr] | (flash[flash_addr+1] << 8) |
                    (flash[flash_addr+2] << 16) | (flash[flash_addr+3] << 24));
}

// ── PSRAM ───────────────────────────────────────────────────────────────────

extern "C" void psram_read(int32_t addr, int32_t *data) {
  if (!g_sim_env || !data) { *data = 0; return; }

  std::vector<uint8_t>& psram = SimEnv_get_psram(g_sim_env);
  uint32_t pa = (uint32_t)addr & ~3u;
  if (pa + 3 < psram.size())
    *data = (int32_t)(psram[pa] | (psram[pa+1] << 8) | (psram[pa+2] << 16) | (psram[pa+3] << 24));
  else
    *data = 0;
}

extern "C" void psram_write(int32_t addr, int32_t data, int32_t mask) {
  if (!g_sim_env) return;

  std::vector<uint8_t>& psram = SimEnv_get_psram(g_sim_env);
  uint32_t pa = (uint32_t)addr & ~3u;
  if (pa + 3 < psram.size()) {
    if (mask & 1) psram[pa]   = (uint8_t)(data & 0xFF);
    if (mask & 2) psram[pa+1] = (uint8_t)((data >> 8) & 0xFF);
    if (mask & 4) psram[pa+2] = (uint8_t)((data >> 16) & 0xFF);
    if (mask & 8) psram[pa+3] = (uint8_t)((data >> 24) & 0xFF);
  }
}

// ── SDRAM (16-bit word-addressed) ───────────────────────────────────────────

extern "C" void sdram_dpi_read(int addr, int *data) {
  if (!g_sim_env || !data) { *data = 0; return; }

  std::vector<uint8_t>& sdram = SimEnv_get_sdram(g_sim_env);
  uint32_t byte_off = (uint32_t)addr * 2;
  if (byte_off + 1 < sdram.size())
    *data = (int32_t)(sdram[byte_off] | (sdram[byte_off+1] << 8));
  else
    *data = 0;
}

extern "C" void sdram_dpi_write(int addr, int data, int mask) {
  if (!g_sim_env) return;

  std::vector<uint8_t>& sdram = SimEnv_get_sdram(g_sim_env);
  uint32_t byte_off = (uint32_t)addr * 2;
  if (byte_off + 1 < sdram.size()) {
    // mask bit 0 = disable DQ[7:0], bit 1 = disable DQ[15:8]
    if (!(mask & 1)) sdram[byte_off]     = (uint8_t)(data & 0xFF);
    if (!(mask & 2)) sdram[byte_off + 1] = (uint8_t)((data >> 8) & 0xFF);
  }
}

// ── MROM ────────────────────────────────────────────────────────────────────

extern "C" void mrom_read(int32_t addr, int32_t *data) {
  if (!g_sim_env || !data) { *data = 0; return; }

  std::vector<uint8_t>& mrom = SimEnv_get_mrom_image(g_sim_env);
  if ((uint32_t)addr < kMromBase || (uint32_t)addr + 3 >= kMromBase + kMromSize) {
    *data = 0;
    return;
  }
  uint32_t off = ((uint32_t)addr & ~3u) - kMromBase;
  *data = (int32_t)(mrom[off] | (mrom[off+1] << 8) | (mrom[off+2] << 16) | (mrom[off+3] << 24));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  STUBS — RTL instantiates these DPI modules; we provide no-op bodies
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" void dpi_commit(
    int dbg_mstatus, int dbg_mtvec, int dbg_mepc, int dbg_mcause,
    int r0, int r1, int r2, int r3, int r4, int r5, int r6, int r7,
    int r8, int r9, int r10,int r11,int r12,int r13,int r14,int r15,
    int r16,int r17,int r18,int r19,int r20,int r21,int r22,int r23,
    int r24,int r25,int r26,int r27,int r28,int r29,int r30,int r31,
    int last_inst, int next_pc, int last_pc, int device_type) {
  (void)dbg_mstatus; (void)dbg_mtvec; (void)dbg_mepc; (void)dbg_mcause;
  (void)r0; (void)r1; (void)r2; (void)r3; (void)r4; (void)r5; (void)r6; (void)r7;
  (void)r8; (void)r9; (void)r10;(void)r11;(void)r12;(void)r13;(void)r14;(void)r15;
  (void)r16;(void)r17;(void)r18;(void)r19;(void)r20;(void)r21;(void)r22;(void)r23;
  (void)r24;(void)r25;(void)r26;(void)r27;(void)r28;(void)r29;(void)r30;(void)r31;
  (void)last_inst; (void)next_pc; (void)last_pc; (void)device_type;
}

extern "C" void dpi_init(int dbg_mstatus, int dbg_mtvec, int dbg_mepc,
                          int dbg_mcause, int dbg_pc) {
  (void)dbg_mstatus; (void)dbg_mtvec; (void)dbg_mepc;
  (void)dbg_mcause; (void)dbg_pc;
}

extern "C" void dpi_perf_event(
    int ifu_fetch, int ifu_stall_ar, int ifu_stall_r, int ifu_stall_bp,
    int idu_compute, int idu_branch, int idu_jump, int idu_load, int idu_store,
    int idu_csr, int idu_system,
    int exu_compute, int exu_load_issue, int exu_store_issue,
    int lsu_load_done, int lsu_store_done, int wbu_commit,
    int icache_access, int icache_hit, int icache_miss_cycle) {
  (void)ifu_fetch; (void)ifu_stall_ar; (void)ifu_stall_r; (void)ifu_stall_bp;
  (void)idu_compute; (void)idu_branch; (void)idu_jump; (void)idu_load; (void)idu_store;
  (void)idu_csr; (void)idu_system;
  (void)exu_compute; (void)exu_load_issue; (void)exu_store_issue;
  (void)lsu_load_done; (void)lsu_store_done; (void)wbu_commit;
  (void)icache_access; (void)icache_hit; (void)icache_miss_cycle;
}
