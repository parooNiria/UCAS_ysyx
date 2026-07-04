// ===========================================================================
// Data Trace (dtrace) — DPI-C callback for offline cache simulation
// Logs every data memory access (load/store) from the CPU pipeline.
// Format: [<cycle>] <type> <addr> <size> [<wdata> <wstrb>]
// ===========================================================================
#include "include/dpi_callbacks.h"
#include "include/trace.h"
#include "include/sim_env.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Global SimEnv pointer (owned by dpi_callbacks.cpp)
// ---------------------------------------------------------------------------
extern SimEnv* g_sim_env_dpi;  // declared in dpi_callbacks.cpp as static; we need our own

// We use the one from dpi_callbacks.cpp via the setter pattern.
// Actually, dpi_callbacks.cpp has a static g_sim_env — we can't extern it.
// Instead, implement our own static and set it from sim_env init.

static SimEnv* g_dtrace_sim_env = NULL;
void dtrace_set_sim_env(SimEnv* env) { g_dtrace_sim_env = env; }

// ---------------------------------------------------------------------------
// Memory trace (dtrace) — logs data accesses for cache simulation
// ---------------------------------------------------------------------------
static FILE *dtrace_fp = NULL;

static void dtrace_init() {
  if (get_disable_trace()) return;
  if (!dtrace_fp) {
    dtrace_fp = fopen("build/npc-log-dtrace.txt", "w");
    if (dtrace_fp) {
      fprintf(dtrace_fp,
        "# DTRACE — Data memory access trace for offline cache simulation\n"
        "# Format: [<cycle>] <type> <addr> <size> [<wdata> <wstrb>]\n"
        "#   type: L=load, S=store\n"
        "#   size: 1=byte, 2=half, 4=word\n"
        "#   wdata/wstrb: only present for stores (wstrb = byte mask, 0xf = all bytes)\n"
        "\n");
      fflush(dtrace_fp);
    }
  }
}

void dtrace_close() {
  if (dtrace_fp) {
    fclose(dtrace_fp);
    dtrace_fp = NULL;
  }
}

// ---------------------------------------------------------------------------
// DPI-C callback: called on every posedge clock
// Only writes a line when load_valid or store_valid is asserted.
// ---------------------------------------------------------------------------
extern "C" void dpi_dtrace_event(int load_valid, int store_valid,
                                  int addr, int mem_size,
                                  int wdata, int wstrb) {
  // Fast path: nothing to log this cycle
  if (!load_valid && !store_valid) return;

  dtrace_init();
  if (!dtrace_fp) return;

  uint64_t t = g_dtrace_sim_env ? g_dtrace_sim_env->sim_time() : 0;

  // Convert func3 to byte count: 0=byte(1), 1=half(2), 2=word(4), others=4
  int size_bytes;
  switch (mem_size & 0x7) {
    case 0: size_bytes = 1; break;  // lb/sb
    case 1: size_bytes = 2; break;  // lh/sh
    case 2: size_bytes = 4; break;  // lw/sw
    default: size_bytes = 4; break; // lbu/lhu (still 4-byte aligned access)
  }

  if (load_valid) {
    fprintf(dtrace_fp, "[%10llu] L 0x%08x %d\n",
            (unsigned long long)t, (unsigned int)addr, size_bytes);
  } else if (store_valid) {
    fprintf(dtrace_fp, "[%10llu] S 0x%08x %d 0x%08x 0x%x\n",
            (unsigned long long)t, (unsigned int)addr, size_bytes,
            (unsigned int)wdata, (unsigned int)(wstrb & 0xF));
  }
  fflush(dtrace_fp);
}
