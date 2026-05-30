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
  bool load_mrom_image(const char *path);
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

  // Verilator components
  VerilatedContext* contextp_;
  VysyxSoCFull* top_;
  VerilatedFstC* tfp_;
  
  // Memory
  std::vector<uint8_t> mrom_image_;
  bool mrom_loaded_;

  // Flash storage (simulated SPI NOR flash)
  std::vector<uint8_t> flash_;
  static constexpr uint32_t kFlashBase = 0x00000000u;
  static constexpr uint32_t kFlashSize = 0x01000000u;  // 16MB
  
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
  int max_sim_time_;
  bool waveform_enabled_;
  std::string wave_file_;
};

// Accessor functions for DPI callbacks
bool& SimEnv_get_ebreak_triggered(SimEnv* env);
int& SimEnv_get_ebreak_a0(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env);
bool& SimEnv_set_stop_flag(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env);

#endif // __SIM_ENV_H__
