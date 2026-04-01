#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <svdpi.h>
#include <capstone/capstone.h>

#include "Vtop.h"
#include "verilated.h"
#include "verilated_fst_c.h"

static bool g_ebreak = false;
static bool g_mem_assert_en = false;
static bool g_mem_assert_fail = false;

#ifndef PMEM_BASE
#define PMEM_BASE 0x80000000u
#endif

#ifndef PMEM_SIZE
#define PMEM_SIZE (128u * 1024u * 1024u)
#endif

static constexpr uint32_t kPmemBase = static_cast<uint32_t>(PMEM_BASE);
static constexpr uint32_t kPmemSize = static_cast<uint32_t>(PMEM_SIZE);
static uint8_t pmem[kPmemSize] = {};

#include <dlfcn.h>
#include <vector>

void (*ref_difftest_memcpy)(uint32_t addr, void *buf, size_t n, bool direction) = NULL;
void (*ref_difftest_regcpy)(void *dut, bool direction) = NULL;
void (*ref_difftest_exec)(uint64_t n) = NULL;
void (*ref_difftest_raise_intr)(uint64_t NO) = NULL;
void (*ref_difftest_init)(int port) = NULL;

enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

struct diff_context_t {
  uint32_t gpr[32];
  uint32_t pc;
};

extern "C" int read_register(int idx);
extern "C" int read_pc();
int inst_count = 0;

void init_difftest(const char *ref_so_file, long img_size, int port) {
  assert(ref_so_file != NULL);
  void *handle = dlopen(ref_so_file, RTLD_LAZY);
  assert(handle);

  ref_difftest_memcpy = (void (*)(uint32_t, void*, size_t, bool))dlsym(handle, "difftest_memcpy");
  assert(ref_difftest_memcpy);

  ref_difftest_regcpy = (void (*)(void*, bool))dlsym(handle, "difftest_regcpy");
  assert(ref_difftest_regcpy);

  ref_difftest_exec = (void (*)(uint64_t))dlsym(handle, "difftest_exec");
  assert(ref_difftest_exec);

  ref_difftest_raise_intr = (void (*)(uint64_t))dlsym(handle, "difftest_raise_intr");
  assert(ref_difftest_raise_intr);

  ref_difftest_init = (void (*)(int))dlsym(handle, "difftest_init");
  assert(ref_difftest_init);

  ref_difftest_init(port);
  ref_difftest_memcpy(0x80000000, pmem, img_size, DIFFTEST_TO_REF);

  diff_context_t cpu;
  svSetScope(svGetScopeFromName("TOP.top"));
  for (int i = 0; i < 32; i++) cpu.gpr[i] = read_register(i);
  cpu.pc = 0x80000000;
  ref_difftest_regcpy(&cpu, DIFFTEST_TO_REF);
}

bool check_difftest(uint32_t npc_pc, uint32_t npc_next_pc) {
  if (ref_difftest_exec == NULL) return true;
  ref_difftest_exec(1);
  diff_context_t ref_cpu;
  ref_difftest_regcpy(&ref_cpu, DIFFTEST_TO_DUT);
  bool match = true;
  svSetScope(svGetScopeFromName("TOP.top"));
  for (int i = 0; i < 32; i++) {
    uint32_t my_reg = read_register(i);
    if (ref_cpu.gpr[i] != my_reg) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("Reg %02d differ! ref=0x%08x, my_reg=0x%08x\n", i, ref_cpu.gpr[i], my_reg);
      match = false;
    }
  }
  if (ref_cpu.pc != npc_next_pc) {
      if (match) printf("\nDifftest failed at pc=0x%08x!\n", npc_pc);
      printf("PC differ! ref=0x%08x, npc=0x%08x\n", ref_cpu.pc, npc_next_pc);
      match = false;
  }
  return match;
}

FILE *log_fp_mtrace = NULL;

static inline bool in_pmem(uint32_t addr) {
  return addr >= kPmemBase &&
         (static_cast<uint64_t>(addr) + 3) < (static_cast<uint64_t>(kPmemBase) + kPmemSize);
}

int pmem_read(int raddr) {
  uint32_t addr = static_cast<uint32_t>(raddr) & ~0x3u;
  if (addr < kPmemBase || addr >= kPmemBase + kPmemSize) {
    fprintf(log_fp_mtrace,"[ERROR] pmem_read addr=0x%08x out of range\n", addr);
    g_mem_assert_fail = true;
    return -1;
  }
  uint32_t offset = addr - kPmemBase;
  int data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  fprintf(log_fp_mtrace,"[MTACE] R addr=0x%08x data=0x%08x\n", addr, data);
  return data;
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) {
  uint32_t addr = static_cast<uint32_t>(waddr) & ~0x3u;
  uint8_t mask = static_cast<uint8_t>(wmask);
  
  if (!in_pmem(addr)) {
    printf("[ERROR] pmem_write addr=0x%08x out of range\n", addr);
    fprintf(log_fp_mtrace,"[ERROR] pmem_write addr=0x%08x out of range\n", addr);
    g_mem_assert_fail = true;
    return;
  }
  uint32_t offset = addr - kPmemBase;
  uint32_t data = static_cast<uint32_t>(wdata);
  for (int i = 0; i < 4; i++) {
    if ((mask >> i) & 1) {
      pmem[offset+i] = (data >> (i*8)) & 0xff;
    }
  }
  fprintf(log_fp_mtrace,"[MTACE] W addr=0x%08x data=0x%08x mask=0x%02x\n", addr, data, mask);
}

int pmem_read_internal(uint32_t addr) {
  if (!in_pmem(addr)) {
    fprintf(log_fp_mtrace,"[ERROR] pmem_read_internal addr=0x%08x out of range\n", addr);
    return -1;
  }
  uint32_t offset = addr - kPmemBase;
  int data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  return data;
}

extern "C" void ebreak_notify() {
  g_ebreak = true;
}

extern "C" int read_register(int idx);

void print_registers() {
  svSetScope(svGetScopeFromName("TOP.top"));
  for (int i = 0; i < 32; i++) {
    printf("x%02d = 0x%08x%s", i, read_register(i), (i % 4 == 3) ? "\n" : "\t");
  }
}

void scan_memory(uint32_t addr, int len) {
  for (int i = 0; i < len; i++) {
    if (i % 4 == 0) printf("\n0x%08x: ", addr + i * 4);
    printf("0x%08x ", pmem_read_internal(addr + i * 4));
  }
  printf("\n");
}

static long load_img(const char *img_file) {
  if (!img_file) return 0;
  FILE *fp = fopen(img_file, "rb");
  if (!fp) { perror("fopen failed"); exit(1); }

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  rewind(fp);
  size_t ret = fread(pmem, 1, size, fp);
  (void)ret;
  fclose(fp);
  printf("Load image: %s, size=%ld, base=0x%08x\n", img_file, size, kPmemBase);
  return size;
}

int main(int argc, char** argv) {
  if (argc > 2) {
    printf("Usage: %s [image.bin]\n", argv[0]);
    return 1;
  }
  long img_size = load_img((argc >= 2) ? argv[1] : nullptr);

  VerilatedContext* contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  Vtop* top = new Vtop(contextp);

  init_difftest("/home/stu/ysyx-workbench/nemu/build/riscv32-nemu-interpreter-so", img_size, 0);

  Verilated::traceEverOn(true);
  VerilatedFstC* tfp = new VerilatedFstC;
  top->trace(tfp, 99);
  tfp->open("wave.fst");

  csh capstone_handle;
  if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &capstone_handle) != CS_ERR_OK) {
    printf("Failed to initialize capstone\n");
    return -1;
  }

  FILE *log_fp_itrace = fopen("build/npc-log-itrace.txt", "w");
  if (!log_fp_itrace) {
    printf("Failed to open log file\n");
    return -1;
  }
  log_fp_mtrace = fopen("build/npc-log-mtrace.txt", "w");
  if (!log_fp_mtrace) {
    printf("Failed to open log file\n");
    return -1;
  }
  top->clk = 0;
  top->rst = 1;
  top->inst = 0;

  // 复位
  for (int i=0; i<3; i++) {
    top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
    top->clk = 1; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  }
  
  top->rst = 0; // Release reset
  top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  
  // 滤除掉复位后 valid 还没拉高时的不工作周期
  top->clk = 1; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  top->clk = 1; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);


  uint64_t cycles = 0;
  const uint64_t max_cycles = 1000000;
  
  bool sdb_mode = true;
  uint64_t sdb_step_count = 0;
  uint32_t last_pc = 0;

  while (!contextp->gotFinish() && !g_ebreak && !g_mem_assert_fail && cycles < max_cycles) {
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

    top->clk = 1; 
    top->eval();
    if (cycles > 0) {
      if (!check_difftest(last_pc, top->pc)) {
        break;
      }
    }

    uint32_t pc = top->pc;
    last_pc = pc;

    if (!in_pmem(pc)) {
      printf("\033[1;31m[BAD] fetch pc=0x%08x out of range\033[0m\n", pc);
      g_mem_assert_fail = true;
      break;
    }
    
    // 1. 获取当前周期稳定的指令
    top->inst = pmem_read_internal(top->pc);

    // 2. 刷新组合逻辑，计算出内存读使能和地址
    top->eval();

    // 3. 处理数据内存的组合逻辑读，只有在读使能时才读取数据
    if (top->ram_ren) {
        top->ram_rdata = pmem_read(top->ram_addr);
        // 读取数据后，再次刷新组合逻辑，因为读出来的数据会影响到写回寄存器的数据
        top->eval();
    } else {
        top->ram_rdata = 0;
    }

    tfp->dump(contextp->time());
    contextp->timeInc(1);

    // ITRACE BEGIN
    cs_insn *insn;
    size_t count = cs_disasm(capstone_handle, (const uint8_t *)&top->inst, 4, pc, 0, &insn);
    if (count > 0) {
      fprintf(log_fp_itrace, "[ITRACE] pc: 0x%08x | inst: 0x%08x | %s\t%s\n", pc, top->inst, insn[0].mnemonic, insn[0].op_str);
      if (sdb_mode) { // 如果在 sdb_mode 就避免刷屏，或者可以自行选择是否全部打印
        printf("[ITRACE] pc: 0x%08x | inst: 0x%08x | %s\t%s\n", pc, top->inst, insn[0].mnemonic, insn[0].op_str);
      } else if (sdb_step_count > 0 || (sdb_mode && sdb_step_count == 0)) { // 单步调试时打印
        printf("[ITRACE] pc: 0x%08x | inst: 0x%08x | %s\t%s\n", pc, top->inst, insn[0].mnemonic, insn[0].op_str);
      }
      cs_free(insn, count);
    } else {
      fprintf(log_fp_itrace, "[ITRACE] pc: 0x%08x | inst: 0x%08x | ERROR\n", pc, top->inst);
      printf("[ITRACE] pc: 0x%08x | inst: 0x%08x | ERROR\n", pc, top->inst);
    }
    // ITRACE END

    // 4. 时钟下降沿
    top->clk = 0;
    top->eval();
    tfp->dump(contextp->time());
    contextp->timeInc(1);
    inst_count++;
    cycles++;
  }

  if (g_ebreak) {
    svSetScope(svGetScopeFromName("TOP.top"));
    int a0 = read_register(10);
    if (a0 == 0) {
      printf("\033[1;32m[GOOD] hit good trap\033[0m\n");
    } else {
      printf("\033[1;31m[BAD] hit bad trap (a0=0x%x)\033[0m\n", a0);
    }
  } else if (g_mem_assert_fail) {
    printf("\033[1;31m[FAIL] Memory error\033[0m\n");
  } else if (cycles >= max_cycles) {
    printf("\033[1;33m[WARN] Timeout\033[0m\n");
  }

  tfp->flush();
  top->final();
  tfp->close();

  if (log_fp_itrace) fclose(log_fp_itrace);
  if (log_fp_mtrace) fclose(log_fp_mtrace);
  cs_close(&capstone_handle);

  delete tfp;
  delete top;
  delete contextp;
  return 0;
}
