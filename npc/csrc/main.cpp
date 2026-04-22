#include <verilated.h>
#include <verilated_fst_c.h>
#include "Vtop.h"
#include "svdpi.h"
#include "include/Context.h"
#include "include/common.h"
#include "include/memory.h"
#include "include/trace.h"
#include <vector>

bool g_ebreak = false;
bool g_mem_assert_fail = false;
int inst_count = 0;

extern "C" void ebreak_notify() {
  g_ebreak = true;
}

int main(int argc, char** argv) {
  if (argc > 3) {
    printf("Usage: %s [image.bin] [ref_so_file]\n", argv[0]);
    return 1;
  }
  long img_size = load_img((argc >= 2) ? argv[1] : nullptr);

  const char *ref_so_file = (argc >= 3) ? argv[2] : "/home/stu/ysyx-workbench/nemu/build/riscv32-nemu-interpreter-so";

  VerilatedContext* contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  Vtop* top = new Vtop(contextp);


  Verilated::traceEverOn(true);
  VerilatedFstC* tfp = new VerilatedFstC;
  top->trace(tfp, 99);
  tfp->open("wave.fst");

  if (!init_trace()) {
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

  init_difftest(ref_so_file, img_size, 0);
  uint64_t cycles = 0;
  const uint64_t max_cycles = 1000000;
  int has_max_cycles = 0;
  bool sdb_mode = true;
  uint64_t sdb_step_count = 0;
  uint32_t last_pc = 0;

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

    top->clk = 1; 
    top->eval();
    if (cycles > 0) {
      if (!check_difftest(last_pc, top->pc)) {
        svSetScope(svGetScopeFromName("TOP.top"));
        display_trap_info(last_pc, read_register(10), "Difftest Failed");
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
    bool print_to_term = sdb_mode && sdb_step_count == 0;
    bool force_print = sdb_step_count > 0 || (sdb_mode && sdb_step_count == 0);
    log_itrace(pc, top->inst, print_to_term, force_print);
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
      display_trap_info(last_pc, a0, "Hit Bad Trap (a0 != 0)");
    }
  } else if (g_mem_assert_fail) {
    svSetScope(svGetScopeFromName("TOP.top"));
    display_trap_info(last_pc, read_register(10), "Memory Access Out of Bounds");
  } else if (cycles >= max_cycles && has_max_cycles) {
    svSetScope(svGetScopeFromName("TOP.top"));
    display_trap_info(last_pc, read_register(10), "Simulator Timeout");
  }

  tfp->flush();
  top->final();
  tfp->close();

  close_trace();

  delete tfp;
  delete top;
  delete contextp;
  return 0;
}
