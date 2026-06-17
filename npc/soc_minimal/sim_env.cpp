#include "sim_env.h"
#include "dpi_callbacks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <nvboard.h>

void nvboard_bind_all_pins(VysyxSoCFull* top);

#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

SimEnv::SimEnv()
  : contextp_(NULL)
  , top_(NULL)
  , sim_time_(0)
  , ebreak_triggered_(false)
  , finished_(false)
  , ebreak_a0_(-1)
  , max_sim_time_(10000000)
  , stop_flag_(false)
{
  set_sim_env_instance(this);
}

SimEnv::~SimEnv() {}

// ── Initialisation ──────────────────────────────────────────────────────────

bool SimEnv::init(int argc, char** argv) {
  printf(COLOR_CYAN "[INIT] Initializing minimal simulation environment..." COLOR_RESET "\n");

  // Parse command-line: only the image path
  const char *img_path = NULL;  // must be provided on command line
  bool no_limit = false;

  const struct option long_options[] = {
    {"no-limit",  no_argument, NULL, 'l'},
    {"help",      no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "-lh", long_options, NULL)) != -1) {
    switch (opt) {
      case 'l':
        no_limit = true;
        break;
      case 1:  // non-option arg → image path
        img_path = optarg;
        break;
      case 'h':
      case '?':
      default:
        printf("Usage: %s [IMAGE] [--no-limit]\n", argv[0]);
        printf("  IMAGE        Binary image to load into flash\n");
        printf("  --no-limit   Disable max cycle limit\n");
        printf("  -h           Display this help\n");
        return false;
    }
  }

  if (!img_path) {
    printf("Usage: %s IMAGE [--no-limit]\n", argv[0]);
    printf("  IMAGE    Binary image to load into flash (required)\n");
    return false;
  }

  if (no_limit) max_sim_time_ = UINT64_MAX;

  // Initialise flash (0xFF erased), load boot image at offset 0
  init_flash();

  // Initialise PSRAM
  psram_.assign(kPsramSize, 0);
  printf("[INIT] PSRAM initialized: %zu bytes\n", psram_.size());

  // Initialise SDRAM
  sdram_.assign(kSdramSize, 0);
  printf("[INIT] SDRAM initialized: %zu bytes\n", sdram_.size());

  // Load boot image into SPI flash at offset 0
  if (!load_flash_image(img_path, 0)) {
    printf(COLOR_RED "[ERROR] Failed to load flash boot image" COLOR_RESET "\n");
    return false;
  }

  // Initialise Verilator
  init_verilator();

  // Initialise NVBoard
  nvboard_bind_all_pins(top_);
  nvboard_init(0);

  // Reset
  do_reset(10);
  printf(COLOR_CYAN "[INIT] Initialization complete" COLOR_RESET "\n");
  return true;
}

// ── Run ─────────────────────────────────────────────────────────────────────

int SimEnv::run() {
  printf("[SIM] Starting simulation...\n");

  while (!contextp_->gotFinish() && sim_time_ < max_sim_time_) {
    if (!tick()) break;
    nvboard_update();
    if (ebreak_triggered_) break;
  }

  if (ebreak_triggered_ && ebreak_a0_ == 0) return 0;
  if (ebreak_triggered_)                  return 1;
  if (sim_time_ >= max_sim_time_)         return 2;
  return 3;
}

// ── Cleanup ─────────────────────────────────────────────────────────────────

void SimEnv::cleanup() {
  nvboard_quit();
  if (top_) {
    top_->final();
    delete top_;
    top_ = NULL;
  }
  if (contextp_) {
    delete contextp_;
    contextp_ = NULL;
  }
}

// ── Verilator ───────────────────────────────────────────────────────────────

void SimEnv::init_verilator() {
  contextp_ = new VerilatedContext;
  contextp_->traceEverOn(false);   // no waveform
  top_ = new VysyxSoCFull(contextp_);
  top_->clock = 0;
  top_->reset = 1;
}

void SimEnv::do_reset(int cycles) {
  printf("[INIT] Performing reset (%d cycles)...\n", cycles);
  top_->reset = 1;
  for (int i = 0; i < cycles; ++i) {
    top_->clock = 0; top_->eval(); contextp_->timeInc(1);
    top_->clock = 1; top_->eval(); contextp_->timeInc(1);
  }
  top_->reset = 0;
}

bool SimEnv::tick() {
  top_->clock = 0; top_->eval(); contextp_->timeInc(1);
  top_->clock = 1; top_->eval(); contextp_->timeInc(1);
  sim_time_ = contextp_->time();

  if (stop_flag_) return false;
  return true;
}

// ── Accessors ───────────────────────────────────────────────────────────────

bool& SimEnv_set_stop_flag(SimEnv* env) { return env->stop_flag_; }

// ── Flash ───────────────────────────────────────────────────────────────────

bool SimEnv::load_flash_image(const char *path, uint32_t offset) {
  FILE *fp = fopen(path, "rb");
  if (!fp) { perror("fopen flash image"); return false; }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);

  if (size <= 0 || (uint32_t)(offset + size) > kFlashSize) {
    fprintf(stderr, "[ERROR] Flash image too large: %ld bytes (offset 0x%x, max 0x%x)\n",
            size, offset, kFlashSize);
    fclose(fp);
    return false;
  }

  fread(flash_.data() + offset, 1, (size_t)size, fp);
  fclose(fp);
  printf("[INIT] Loaded flash boot image: %s, size=%ld\n", path, size);
  return true;
}

void SimEnv::init_flash() {
  flash_.assign(kFlashSize, 0xFF);
  printf("[INIT] Flash initialized: %zu bytes\n", flash_.size());
}

std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env)  { return env->flash_; }
std::vector<uint8_t>& SimEnv_get_psram(SimEnv* env)  { return env->psram_; }
std::vector<uint8_t>& SimEnv_get_sdram(SimEnv* env)  { return env->sdram_; }
