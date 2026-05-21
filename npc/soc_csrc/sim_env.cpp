#include "include/sim_env.h"
#include "include/difftest.h"
#include "include/dpi_callbacks.h"
#include "include/trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Color definitions for output
#define COLOR_GREEN "\033[32m"
#define COLOR_RED   "\033[31m"
#define COLOR_CYAN  "\033[36m"
#define COLOR_RESET "\033[0m"

SimEnv::SimEnv()
  : contextp_(NULL)
  , top_(NULL)
  , tfp_(NULL)
  , mrom_loaded_(false)
  , difftest_(NULL)
  , difftest_enabled_(false)
  , sim_time_(0)
  , ebreak_triggered_(false)
  , finished_(false)
  , ebreak_a0_(-1)
  , max_sim_time_(1000000)
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
  
  // Determine MROM image path
  const char *mrom_path = (argc > 1) ? argv[1] : "/root/UCAS_ysyx/npc/test_csrc/char-test.bin";
  
  // Load MROM image
  if (!load_mrom_image(mrom_path)) {
    printf(COLOR_RED "[ERROR] Failed to load MROM image" COLOR_RESET "\n");
    return false;
  }
  
  // Initialize Verilator
  init_verilator();
  
  // Initialize waveform recording
  if (waveform_enabled_) {
    init_waveform();
  }
  
  // Initialize DiffTest with NEMU
  const char *nemu_so = "/root/UCAS_ysyx/nemu/build/riscv32-nemu-interpreter-so";
  if (difftest_->init(nemu_so)) {
    // Sync MROM to NEMU
    difftest_->sync_mrom(kMromBase, mrom_image_.data(), mrom_image_.size());
    difftest_enabled_ = true;
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
  }else{
    return 2;
  }
}

void SimEnv::cleanup() {
  // Close trace system
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

bool SimEnv::load_mrom_image(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    perror("fopen mrom image");
    return false;
  }
  
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  
  if (size < 0 || size > static_cast<long>(kMromSize)) {
    fprintf(stderr, "[ERROR] MROM image too large: %ld bytes (max 0x%x)\n",
            size, kMromSize);
    fclose(fp);
    return false;
  }
  
  mrom_image_.assign(kMromSize, 0);
  size_t ret = fread(mrom_image_.data(), 1, static_cast<size_t>(size), fp);
  (void)ret;
  fclose(fp);
  
  mrom_loaded_ = true;
  printf("[INIT] Load MROM image: %s, size=%ld, base=0x%08x\n", 
         path, size, kMromBase);
  return true;
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

