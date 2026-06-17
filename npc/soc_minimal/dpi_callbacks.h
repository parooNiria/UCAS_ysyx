#ifndef __DPI_CALLBACKS_H__
#define __DPI_CALLBACKS_H__

#include "sim_env.h"
#include <stdint.h>
#include <vector>

#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

constexpr uint32_t kMromBase = 0x20000000u;
constexpr uint32_t kMromSize = 0x1000u;

class SimEnv;

// Set global SimEnv instance for DPI callbacks
void set_sim_env_instance(SimEnv* env);

// ── DPI-C callbacks used by Verilog ──
extern "C" {
  // Memory reads / writes
  void flash_read     (int32_t addr, int32_t *data);
  void psram_read     (int32_t addr, int32_t *data);
  void psram_write    (int32_t addr, int32_t data, int32_t mask);
  void sdram_dpi_read (int addr, int *data);
  void sdram_dpi_write(int addr, int data, int mask);
  void mrom_read      (int32_t addr, int32_t *data);

  // Simulation control
  void dpi_ebreak(int reg_a0);

  // Stubs — RTL instantiates these DPI modules but we don't use them
  void dpi_commit(int dbg_mstatus, int dbg_mtvec, int dbg_mepc, int dbg_mcause,
                  int r0, int r1, int r2, int r3, int r4, int r5, int r6, int r7,
                  int r8, int r9, int r10,int r11,int r12,int r13,int r14,int r15,
                  int r16,int r17,int r18,int r19,int r20,int r21,int r22,int r23,
                  int r24,int r25,int r26,int r27,int r28,int r29,int r30,int r31,
                  int last_inst, int next_pc, int last_pc, int device_type);
  void dpi_init  (int dbg_mstatus, int dbg_mtvec, int dbg_mepc, int dbg_mcause, int dbg_pc);
  void dpi_perf_event(int ifu_fetch, int ifu_stall_ar, int ifu_stall_r, int ifu_stall_bp,
                      int idu_compute, int idu_branch, int idu_jump, int idu_load,
                      int idu_store, int idu_csr, int idu_system,
                      int exu_compute, int exu_load_issue, int exu_store_issue,
                      int lsu_load_done, int lsu_store_done, int wbu_commit,
                      int icache_access, int icache_hit, int icache_miss_cycle);
}

#endif
