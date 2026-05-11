#include <verilated.h>
#include <verilated_fst_c.h>
#include "Vtop.h"
#include "svdpi.h"
#include "include/Context.h"
#include "include/common.h"
#include "include/axi_memory.h"
#include "include/memory.h"
#include "include/trace.h"
#include <vector>
#include <getopt.h>
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_YELLOW  "\033[33m"
#define ANSI_COLOR_CYAN    "\033[36m"
#define ANSI_COLOR_RESET   "\033[0m"
bool g_ebreak = false;
bool g_mem_assert_fail = false;
int inst_count = 0;

// Global pointer to the top-level Verilated module so helper functions
// (e.g. read_register/read_csr) can access DUT signals.
Vtop *top = nullptr;

char *image_file = NULL;    // 保存 IMAGE 路径
char *ref_file = NULL;      // 保存 REF 路径
int disable_ref = 0;        // 是否指定不开启 REF (--no-ref)
int disable_itrace = 0;     // 是否指定不开启 itrace (--no-itrace)
int disable_mtrace = 0;     // 是否指定不开启 mtrace (--no-mtrace)
int disable_sdb = 0;        // 是否指定不进入 sdb (--no-sdb)
int disable_wave = 0;       // 是否指定不开启 waveform (--no-wave)
int commit_compare = 0;
int device_accessed = 0;  // 标志是否发生过 mmio 访问
int wrong_happened = 0;
static int parse_args(int argc, char *argv[]) {
    const struct option long_options[] = {
        {"no-ref",    no_argument, NULL, 'r'}, // 指定不开启 REF
        {"no-itrace", no_argument, NULL, 't'}, // 指定不开启 itrace
        {"no-mtrace", no_argument, NULL, 'm'}, // 指定不开启 mtrace
        {"no-sdb",    no_argument, NULL, 's'}, // 指定不进入 sdb
        {"no-wave",   no_argument, NULL, 'w'}, // 指定不开启 waveform 记录
        {"help",      no_argument, NULL, 'h'}, // 帮助信息
        {0, 0, 0, 0}                          // 结束标记
    };

    // 2. 定义短选项字符串
    // 开头的 '-' 是一个 GNU 扩展特性：它会让 getopt_long 把不带横杠的参数（如 IMAGE, REF）
    // 当作选项来处理，并返回字符码 1。
    const char *optstring = "-rtmsw"; 



  int opt;
  while ((opt = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
      switch (opt) {
          case 'r':
              disable_ref = 1;
              printf(ANSI_COLOR_CYAN "[TIPS] " ANSI_COLOR_RESET "Disable differential testing (DiffTest)\n");
              break;
          case 't':
              disable_itrace = 1;
              printf(ANSI_COLOR_CYAN "[TIPS] " ANSI_COLOR_RESET "Disable instruction trace (itrace)\n");
              break;
          case 'm':
              disable_mtrace = 1;
              printf(ANSI_COLOR_CYAN "[TIPS] " ANSI_COLOR_RESET "Disable memory trace (mtrace)\n");
              break;
          case 'w':                  
              disable_wave = 1;
              printf(ANSI_COLOR_CYAN "[TIPS] " ANSI_COLOR_RESET "Disable waveform recording\n");
              break;
          case 's':
              disable_sdb = 1;
              printf(ANSI_COLOR_CYAN "[TIPS] " ANSI_COLOR_RESET "Skip SDB interactive mode\n");
              break;
          case 1: 
              if (image_file == NULL) {
                  image_file = optarg;
              } else if (ref_file == NULL) {
                  ref_file = optarg;
              }
              break;
          case 'h':
          case '?':
          default:
              printf("Usage: %s [IMAGE] [REF] [OPTIONS...]\n", argv[0]);
              printf(ANSI_COLOR_CYAN "Arguments:\n" ANSI_COLOR_RESET);
              printf("  IMAGE            Specify the binary image file to run\n");
              printf("  REF              Specify the reference model (.so) for DiffTest\n");
              printf(ANSI_COLOR_CYAN "Options:\n" ANSI_COLOR_RESET);
              printf("  --no-ref         Disable differential testing (DiffTest)\n");
              printf("  --no-itrace      Disable instruction trace (itrace)\n");
              printf("  --no-mtrace      Disable memory trace (mtrace)\n");
              printf("  --no-wave        Disable waveform recording\n");
              printf("  --no-sdb         Skip SDB interactive mode\n");
              printf("  -h, --help       Display this help message\n");
              exit(0);
      }
  }
      return 0;
}

int main(int argc, char** argv) {
  
  parse_args(argc, argv);
  if (image_file == NULL) {
      printf("Error: No IMAGE file specified.\n");
      printf("Usage: %s [IMAGE] [REF] [OPTIONS...]\n", argv[0]);
      exit(1);
  }
  long img_size = load_img(image_file);
  const char* ref_so_file = disable_ref ? NULL : ref_file;
  bool enable_difftest = !disable_ref;
  if (enable_difftest && ref_so_file == NULL) {
      printf("Error: DiffTest enabled but no REF file specified.\n");
      printf("Usage: %s [IMAGE] [REF] [OPTIONS...]\n", argv[0]);
      exit(1);
  }



  VerilatedContext* contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  top = new Vtop(contextp);
  AxiLiteMemory axi_mem;


  VerilatedFstC* tfp = NULL;
  if (!disable_wave) {
    Verilated::traceEverOn(true);
    tfp = new VerilatedFstC;
    top->trace(tfp, 99);
    tfp->open("wave.fst");
  }

  if (!init_trace()) {
    return -1;
  }
  top->clock = 0;
  top->reset = 1;
  axi_mem.drive(top);

  // 复位
  for (int i=0; i<3; i++) {
    top->clock = 0;
    top->eval();
    if (tfp) tfp->dump(contextp->time());
    contextp->timeInc(1);

    top->clock = 1;
    top->eval();
    if (tfp) tfp->dump(contextp->time());
    contextp->timeInc(1);
  }
  
  top->clock = 0;
  top->eval();
  axi_mem.sample(top);
  if (tfp) tfp->dump(contextp->time());
  contextp->timeInc(1);
  top->reset = 0; // Release reset
  // 滤除掉复位后 valid 还没拉高时的不工作周期
  if (enable_difftest) {
    init_difftest(ref_so_file, img_size, 0);
  }
  uint64_t cycles = 0;
  const uint64_t max_cycles = 1000000;
  int has_max_cycles = 0;
  bool sdb_mode = !disable_sdb;
  uint64_t sdb_step_count = 0;
  uint32_t last_pc = 0x80000000;


  while (!contextp->gotFinish() && !g_ebreak && !g_mem_assert_fail && (cycles < max_cycles || !has_max_cycles)) {
    if (sdb_mode && sdb_step_count == 0) {
      char buf[256];
      printf("(npc) ");
      if (!fgets(buf, sizeof(buf), stdin)) break;
      char *cmd = strtok(buf, " \n");
      if (!cmd) continue;

      if (strcmp(cmd, "c") == 0) {
        sdb_mode = false;
      } else if (strcmp(cmd, "q") == 0) {
        break;
      } else if (strcmp(cmd, "si") == 0) {
        char *arg = strtok(NULL, " \n");
        sdb_step_count = arg ? strtoull(arg, NULL, 10) : 1;
      } else if (strcmp(cmd, "info") == 0) {
        char *arg = strtok(NULL, " \n");
        if (arg && strcmp(arg, "r") == 0) print_registers();
        continue;
      } else if (strcmp(cmd, "x") == 0) {
        char *arg1 = strtok(NULL, " \n");
        char *arg2 = strtok(NULL, " \n");
        if (arg1 && arg2) {
          int len = atoi(arg1);
          uint32_t addr = strtoul(arg2, NULL, 16);
          scan_memory(addr, len);
        }
        continue;
      } else {
        printf("Unknown command '%s'\n", cmd);
        continue;
      }
    }

    if (sdb_step_count > 0) {
      sdb_step_count--;
    }


    top->clock = 1;
    top->eval();
    axi_mem.drive(top);
    top->eval();
    if (top->io_ebreak) {
      g_ebreak = true;
    }

    if (top->io_commit_valid) {
      uint32_t pc = last_pc;
      last_pc = top->io_pc;
      if (tfp) tfp->dump(contextp->time());
      contextp->timeInc(1);

      // ITRACE BEGIN
      bool print_to_term = sdb_mode && sdb_step_count == 0;
      bool force_print = sdb_step_count > 0 || (sdb_mode && sdb_step_count == 0);
      log_itrace(pc, top->io_inst, print_to_term, force_print);
      // ITRACE END
    } else {
      if (tfp) tfp->dump(contextp->time());
      contextp->timeInc(1);
    }

    if (enable_difftest && cycles > 0 && commit_compare) {
      if (!check_difftest(last_pc, top->io_pc, device_accessed)) {
        if (tfp) tfp->dump(contextp->time());
        svSetScope(svGetScopeFromName("TOP.top"));
        display_trap_info(last_pc, read_register(10), "Difftest Failed");
        wrong_happened = 1;
        break;
      }
      commit_compare = 0;
      device_accessed = 0; // 复位 mmio 访问标志
    }
    // 4. 时钟下降沿
    top->clock = 0;
    top->eval();
    axi_mem.sample(top);
    if (tfp) tfp->dump(contextp->time());
    contextp->timeInc(1);
    inst_count++;
    cycles++;
    commit_compare = top->io_commit_valid;
    device_accessed = top->io_device_access;
  }

  if (g_ebreak) {
    svSetScope(svGetScopeFromName("TOP.top"));
    int a0 = read_register(10);
    if (a0 == 0) {
      printf("\033[1;32m[GOOD] hit good trap\033[0m\n");
    } else {
      display_trap_info(last_pc, a0, "Hit Bad Trap (a0 != 0)");
      wrong_happened = 1;
    }
  } else if (g_mem_assert_fail) {
    svSetScope(svGetScopeFromName("TOP.top"));
    display_trap_info(last_pc, read_register(10), "Memory Access Out of Bounds");
    wrong_happened = 1;
  } else if (cycles >= max_cycles && has_max_cycles) {
    svSetScope(svGetScopeFromName("TOP.top"));
    display_trap_info(last_pc, read_register(10), "Simulator Timeout");
    wrong_happened = 1; 
  }

  if (tfp) {
    tfp->flush();
    tfp->close();
    delete tfp;
  }
  top->final();

  close_trace();

  delete top;
  delete contextp;
  if(wrong_happened) {
    return -1;
  }
  return 0;
}
