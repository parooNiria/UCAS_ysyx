#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

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

static inline bool in_pmem(uint32_t addr) {
  return addr >= kPmemBase &&
         (static_cast<uint64_t>(addr) + 3) < (static_cast<uint64_t>(kPmemBase) + kPmemSize);
}

extern "C" int pmem_read(int raddr) {
  uint32_t addr = static_cast<uint32_t>(raddr) & ~0x3u;
  
  // 注意：组合逻辑期间的 DPI-C 调用会有大量的伪越界地址毛刺
  // 不要在这里触发 g_mem_assert_fail 或者报错，直接静默返回0即可
  if (!in_pmem(addr)) {
    return 0;
  }
  uint32_t offset = addr - kPmemBase;
  uint32_t data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  return static_cast<int>(data);
}

extern "C" void pmem_write(int waddr, int wdata, char wmask) {
  uint32_t addr = static_cast<uint32_t>(waddr) & ~0x3u;
  uint8_t mask = static_cast<uint8_t>(wmask);
  
  // 写操作在 always @(posedge clk) 中触发，必须是稳定且合法的
  // 如果写越界，则是真实的程序错误
  if (!in_pmem(addr)) {
    printf("[ERROR] pmem_write addr=0x%08x out of range\n", addr);
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
}

extern "C" void ebreak_notify() {
  g_ebreak = true;
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
  load_img((argc >= 2) ? argv[1] : nullptr);

  VerilatedContext* contextp = new VerilatedContext;
  contextp->commandArgs(argc, argv);
  Vtop* top = new Vtop(contextp);

  Verilated::traceEverOn(true);
  VerilatedFstC* tfp = new VerilatedFstC;
  top->trace(tfp, 99);
  tfp->open("wave.fst");

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
  
  uint64_t cycles = 0;
  const uint64_t max_cycles = 1000000;

  while (!contextp->gotFinish() && !g_ebreak && !g_mem_assert_fail && cycles < max_cycles) {
    top->clk = 1; top->eval();
    uint32_t pc = top->pc;

    if (!in_pmem(pc)) {
      printf("\033[1;31m[BAD] fetch pc=0x%08x out of range\033[0m\n", pc);
      g_mem_assert_fail = true;
      break;
    }
    
    // 1. 获取当前周期稳定的指令
    top->inst = pmem_read(pc);

    // 2. 刷新组合逻辑 (由于内部使用了DPI-C触发pmem_read，这里会自动获取相关内存数据且自动忽略毛刺)
    top->eval();
    tfp->dump(contextp->time());
    contextp->timeInc(1);
    // 4. 时钟下降沿
    top->clk = 0;
    top->eval();
    tfp->dump(contextp->time());
    contextp->timeInc(1);

    cycles++;
  }

  if (g_ebreak) {
    printf("\033[1;32m[GOOD] hit good trap\033[0m\n");
  } else if (g_mem_assert_fail) {
    printf("\033[1;31m[FAIL] Memory error\033[0m\n");
  } else if (cycles >= max_cycles) {
    printf("\033[1;33m[WARN] Timeout\033[0m\n");
  }

  tfp->flush();
  top->final();
  tfp->close();

  delete tfp;
  delete top;
  delete contextp;
  return 0;
}
