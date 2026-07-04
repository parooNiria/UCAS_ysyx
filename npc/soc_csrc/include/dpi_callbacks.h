#ifndef __DPI_CALLBACKS_H__
#define __DPI_CALLBACKS_H__

#include "sim_env.h"
#include <stdint.h>
#include <vector>

// Color definitions for output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

// Address space definitions
constexpr uint32_t kMromBase = 0x20000000u;
constexpr uint32_t kMromSize = 0x1000u;
constexpr uint32_t kSramBase = 0x0f000000u;
constexpr uint32_t kSramSize = 0x2000u;

// Forward declaration
class SimEnv;

// Accessor functions (implemented in dpi_callbacks.cpp)
bool& SimEnv_get_ebreak_triggered(SimEnv* env);
int& SimEnv_get_ebreak_a0(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env);

// Set global SimEnv instance
void set_sim_env_instance(SimEnv* env);
void set_diff_test_instance(DiffTest* diff_test);

// Memory trace (mtrace) — close log file on shutdown
void mtrace_close();

// Data trace (dtrace) — for offline cache simulation
void dtrace_close();
void dtrace_set_sim_env(SimEnv* env);
  
// DPI callback declarations (extern "C" for Verilator)
extern "C" {
  void dpi_ebreak(int reg_a0);
  void mrom_read(int32_t addr, int32_t *data);
  void flash_read(int32_t addr, int32_t *data);
  void psram_read(int32_t addr, int32_t *data);
  void psram_write(int32_t addr, int32_t data, int32_t mask);
  void sdram_dpi_read(int addr, int *data);
  void sdram_dpi_write(int addr, int data, int mask);
}

#endif // __DPI_CALLBACKS_H__
