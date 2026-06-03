#include "include/sim_env.h"
#include "include/difftest.h"
#include "include/dpi_callbacks.h"
#include "include/trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>

// Color definitions for output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

SimEnv::SimEnv()
  : contextp_(NULL)
  , top_(NULL)
  , tfp_(NULL)
  , difftest_(NULL)
  , difftest_enabled_(false)
  , sim_time_(0)
  , ebreak_triggered_(false)
  , finished_(false)
  , ebreak_a0_(-1)
  , max_sim_time_(10000000)
  , waveform_enabled_(true)
  , wave_file_("wave.fst")
{
  // Create DiffTest instance
  difftest_ = new DiffTest();
  set_sim_env_instance(this);
  set_diff_test_instance(difftest_);
}

SimEnv::~SimEnv() {
  delete difftest_;
}

//初始化
bool SimEnv::init(int argc, char** argv) {
  printf(COLOR_CYAN "[INIT] Initializing simulation environment..." COLOR_RESET "\n");

  // Initialize itrace system
  if (!init_trace()) {
    printf(COLOR_RED "[ERROR] Failed to initialize trace system" COLOR_RESET "\n");
    return false;
  }

  // --- Parse command-line arguments ---
  const char *img_path   = "/root/UCAS_ysyx/npc/test_csrc/char-test.bin";
  bool no_diff  = false;
  bool no_wave  = false;
  bool no_limit = false;

  const struct option long_options[] = {
    {"no-diff",   no_argument, NULL, 'd'},
    {"no-wave",   no_argument, NULL, 'w'},
    {"no-limit",  no_argument, NULL, 'l'},
    {"help",      no_argument, NULL, 'h'},
    {0, 0, 0, 0}
  };
  const char *optstring = "-dwlh";

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
    switch (opt) {
      case 'd':
        no_diff = true;
        printf(COLOR_CYAN "[TIPS] " COLOR_RESET "Disable differential testing (DiffTest)\n");
        break;
      case 'w':
        no_wave = true;
        printf(COLOR_CYAN "[TIPS] " COLOR_RESET "Disable waveform recording\n");
        break;
      case 'l':
        no_limit = true;
        printf(COLOR_CYAN "[TIPS] " COLOR_RESET "Disable max cycle limit\n");
        break;
      case 1:  // non-option argument → image path
        img_path = optarg;
        break;
      case 'h':
      case '?':
      default:
        printf("Usage: %s [IMAGE] [OPTIONS...]\n", argv[0]);
        printf(COLOR_CYAN "Arguments:\n" COLOR_RESET);
        printf("  IMAGE            Binary image to load into flash\n");
        printf(COLOR_CYAN "Options:\n" COLOR_RESET);
        printf("  --no-diff        Disable differential testing\n");
        printf("  --no-wave        Disable waveform recording\n");
        printf("  --no-limit       Disable max cycle limit\n");
        printf("  -h, --help       Display this help\n");
        return false;
    }
  }

  if (no_limit) {
    max_sim_time_ = UINT64_MAX;
  }

  // Initialize flash (erased state 0xFF), then load boot image at offset 0
  init_flash();

  // Initialize PSRAM (cleared to zero)
  psram_.assign(kPsramSize, 0);
  printf("[INIT] PSRAM initialized: %zu bytes\n", psram_.size());

  // Load boot image into SPI flash at offset 0 (CPU boots from 0x30000000 via XIP)
  if (!load_flash_image(img_path, 0)) {
    printf(COLOR_RED "[ERROR] Failed to load flash boot image" COLOR_RESET "\n");
    return false;
  }

  // Initialize Verilator
  init_verilator();

  // Initialize waveform recording (skip if --no-wave)
  if (waveform_enabled_ && !no_wave) {
    init_waveform();
  }

  // Initialize DiffTest with NEMU (skip if --no-diff)
  if (!no_diff) {
    const char *nemu_so = "/root/UCAS_ysyx/nemu/build/riscv32-nemu-interpreter-so";
    if (difftest_->init(nemu_so)) {
      difftest_->sync_mrom(kFlashXipBase, flash_.data(), flash_.size());
      difftest_enabled_ = true;
    }
  }

  // Perform reset
  do_reset(10);
  printf( COLOR_CYAN "[INIT] Initialization complete" COLOR_RESET"\n");
  return true;
}


//运行仿真
int SimEnv::run() {
  printf("[INIT] Starting simulation...\n");

  while (!contextp_->gotFinish() && sim_time_ < max_sim_time_) {
    if (!tick()) {
      break;
    }

    if(ebreak_triggered_){
      break;
    }
  }

  if(ebreak_triggered_ && ebreak_a0_ == 0){
    return 0;
  }else if(ebreak_triggered_){
    return 1;
  }else if(sim_time_ >= max_sim_time_){
    return 2;
  } else{
    return 3;
  }
}

void SimEnv::cleanup() {
  // Close trace systems
  mtrace_close();
  close_trace();

  if (tfp_) {
    tfp_->flush();
    tfp_->close();
    delete tfp_;
    tfp_ = NULL;
  }

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

void SimEnv::init_verilator() {
  contextp_ = new VerilatedContext;

  // Enable trace before creating top
  contextp_->traceEverOn(true);

  top_ = new VysyxSoCFull(contextp_);

  // Initialize signals
  top_->clock = 0;
  top_->reset = 1;
}

void SimEnv::init_waveform() {
  tfp_ = new VerilatedFstC;
  top_->trace(tfp_, 99);

  tfp_->open(wave_file_.c_str());
  printf("[INIT] Waveform recording enabled: %s\n", wave_file_.c_str());
}

void SimEnv::do_reset(int cycles) {
  printf("[INIT] Performing reset (%d cycles)...\n", cycles);

  top_->reset = 1;
  for (int i = 0; i < cycles; ++i) {
    top_->clock = 0;
    top_->eval();
    if (tfp_) tfp_->dump(contextp_->time());
    contextp_->timeInc(1);

    top_->clock = 1;
    top_->eval();
    if (tfp_) tfp_->dump(contextp_->time());
    contextp_->timeInc(1);
  }
  top_->reset = 0;
}

bool SimEnv::tick() {
  // Rising edge
  top_->clock = 0;
  top_->eval();
  if (tfp_) tfp_->dump(contextp_->time());
  contextp_->timeInc(1);

  // Falling edge
  top_->clock = 1;
  top_->eval();
  if (tfp_) tfp_->dump(contextp_->time());
  contextp_->timeInc(1);

  sim_time_ = contextp_->time();

  // Check for ebreak
  if (stop_flag_) {
    return false;
  }

  return true;
}


bool& SimEnv_set_stop_flag(SimEnv* env){
  return env->stop_flag_;
}

bool SimEnv::load_flash_image(const char *path, uint32_t offset) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    perror("fopen flash image");
    return false;
  }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);

  if (size <= 0 || (uint32_t)(offset + size) > kFlashSize) {
    fprintf(stderr, "[ERROR] Flash image too large: %ld bytes (offset 0x%x, max 0x%x)\n",
            size, offset, kFlashSize);
    fclose(fp);
    return false;
  }

  size_t ret = fread(flash_.data() + offset, 1, static_cast<size_t>(size), fp);
  (void)ret;
  fclose(fp);

  printf("[INIT] Loaded flash boot image: %s, size=%ld, XIP addr=0x%08x\n",
         path, size, kFlashXipBase + offset);
  return true;
}

void SimEnv::init_flash() {
  // Initialize flash: all 0xFF (erased state), then load_flash_image()
  // overwrites the beginning with the boot image.
  flash_.assign(kFlashSize, 0xFF);
  printf("[INIT] Flash initialized: %zu bytes, XIP base=0x%08x\n",
         flash_.size(), kFlashXipBase);
}

std::vector<uint8_t>& SimEnv_get_flash(SimEnv* env) {
  return env->flash_;
}

std::vector<uint8_t>& SimEnv_get_psram(SimEnv* env) {
  return env->psram_;
}
