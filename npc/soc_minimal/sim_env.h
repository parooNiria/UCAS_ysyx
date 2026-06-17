#ifndef __SIM_ENV_H__
#define __SIM_ENV_H__

#include <verilated.h>
#include <nvboard.h>
#include "VysyxSoCFull.h"
#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>

// Minimal simulation environment — no DiffTest, no waveform, no trace.
// Memory arrays, DPI-C callbacks, and NVBoard.
class SimEnv {
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

  bool init(int argc, char** argv);
  int  run();
  void cleanup();

  uint64_t sim_time() const { return sim_time_; }

  VerilatedContext* contextp_;
  VysyxSoCFull*     top_;

  // MROM (kept for DPI compatibility)
  std::vector<uint8_t> mrom_image_;

  // Flash (SPI NOR, 16 MB) — primary boot via XIP
  std::vector<uint8_t> flash_;

  // PSRAM (QSPI IS66WVS4M8ALL, 4 MB)
  std::vector<uint8_t> psram_;

  // SDRAM (MT48LC16M16A2, 32 MB)
  std::vector<uint8_t> sdram_;

  // Address-space constants
  static constexpr uint32_t kPsramBase   = 0x80000000u;
  static constexpr uint32_t kPsramSize   = 0x00400000u;
  static constexpr uint32_t kSdramBase   = 0xa0000000u;
  static constexpr uint32_t kSdramSize   = 0x04000000u;
  static constexpr uint32_t kFlashBase   = 0x00000000u;
  static constexpr uint32_t kFlashSize   = 0x01000000u;
  static constexpr uint32_t kFlashXipBase= 0x30000000u;

  // State
  uint64_t sim_time_;
  bool     ebreak_triggered_;
  bool     finished_;
  int      ebreak_a0_;
  bool     stop_flag_;
  uint64_t max_sim_time_;

private:
  bool load_flash_image(const char *path, uint32_t offset);
  void init_flash();
  void init_verilator();
  void do_reset(int cycles);
  bool tick();
};

bool& SimEnv_get_ebreak_triggered(SimEnv* env);
int&  SimEnv_get_ebreak_a0(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_mrom_image(SimEnv* env);
bool& SimEnv_set_stop_flag(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_psram(SimEnv* env);
std::vector<uint8_t>& SimEnv_get_sdram(SimEnv* env);

#endif
