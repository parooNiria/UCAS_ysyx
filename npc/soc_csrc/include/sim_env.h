#ifndef __SIM_ENV_H__
#define __SIM_ENV_H__

#include <verilated.h>
#include <verilated_fst_c.h>
#include "VysyxSoCFull.h"
#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>

// Forward declarations
class DiffTest;

// 仿真环境，主要的类
class SimEnv {
  friend class DiffTest;
  friend bool& SimEnv_get_ebreak_triggered(SimEnv* env);
  friend int& SimEnv_get_ebreak_a0(SimEnv* env);
  friend std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env);
  friend bool& SimEnv_set_stop_flag(SimEnv* env);
  friend std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env);
  friend std::vector<uint8_t>& SimEnv_get_psram(SimEnv* env);
  friend std::vector<uint8_t>& SimEnv_get_sdram(SimEnv* env);

public:
  SimEnv();
  ~SimEnv();

  // Initialize simulation environment
  bool init(int argc, char** argv);
  // Run simulation
  int run();
  // Cleanup
  void cleanup();

private:
  // Memory management
  bool load_flash_image(const char *path, uint32_t offset);
  void init_flash();
  
  // Verilator simulation
  void init_verilator();
  void do_reset(int cycles);
  bool tick();
  
  // Waveform recording
  void init_waveform();
  
  // State tracking
  bool check_ebreak();
  int get_exit_code() const;

public:
  // Simulation time accessor (for DPI-C mtrace)
  uint64_t sim_time() const { return sim_time_; }

  // Verilator components
  VerilatedContext* contextp_;
  VysyxSoCFull* top_;
  VerilatedFstC* tfp_;
  
  // Memory (MROM kept for DPI callback compatibility with AXI4MROM HW)
  std::vector<uint8_t> mrom_image_;

  // Flash storage (simulated SPI NOR flash) — primary boot device via XIP
  std::vector<uint8_t> flash_;

  // PSRAM storage (simulated QSPI PSRAM IS66WVS4M8ALL, 4MB)
  std::vector<uint8_t> psram_;
  static constexpr uint32_t kPsramBase = 0x80000000u;  // PSRAM base address
  static constexpr uint32_t kPsramSize = 0x00400000u;  // 4MB

  // SDRAM storage (simulated MT48LC16M16A2, 16M x 16 = 32MB)
  // Internal DPI addr = {bank[1:0], row[12:0], col[8:0]} (24-bit word address)
  std::vector<uint8_t> sdram_;
  static constexpr uint32_t kSdramBase = 0xa0000000u;  // SDRAM base in SoC address space
  static constexpr uint32_t kSdramSize = 0x02000000u;  // 32MB
  static constexpr uint32_t kFlashBase = 0x00000000u;
  static constexpr uint32_t kFlashSize = 0x01000000u;  // 16MB
  static constexpr uint32_t kFlashXipBase = 0x30000000u;  // XIP address in SoC
  
  // DiffTest
  DiffTest* difftest_;
  bool difftest_enabled_;
  
  // Simulation state
  uint64_t sim_time_;
  bool ebreak_triggered_;
  bool finished_;
  int ebreak_a0_;
  bool stop_flag_;
  
  // Configuration
  uint64_t max_sim_time_;
  bool waveform_enabled_;
  bool nvboard_enabled_;
  std::string wave_file_;
};

// Accessor functions for DPI callbacks
bool& SimEnv_get_ebreak_triggered(SimEnv* env);
int& SimEnv_get_ebreak_a0(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env);
bool& SimEnv_set_stop_flag(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_psram(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_sdram(SimEnv* env);

#endif // __SIM_ENV_H__
