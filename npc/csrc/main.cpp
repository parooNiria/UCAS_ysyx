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

uint32_t pmem_read(uint32_t addr) {
  addr = addr & ~0x3u;
  if (!g_mem_assert_en) return 0;
  if (!in_pmem(addr)) {
    printf("[ERROR] pmem_read addr=0x%08x out of range\n", addr);
    g_mem_assert_fail = true;
    return 0;
  }
  uint32_t offset = addr - kPmemBase;
  uint32_t data = 0;
  data |= pmem[offset+0] << 0;
  data |= pmem[offset+1] << 8;
  data |= pmem[offset+2] << 16;
  data |= pmem[offset+3] << 24;
  return data;
}

void pmem_write(uint32_t addr, uint32_t data, uint8_t mask) {
  addr = addr & ~0x3u;
  if (!g_mem_assert_en) return;
  if (!in_pmem(addr)) {
    printf("[ERROR] pmem_write addr=0x%08x out of range\n", addr);
    g_mem_assert_fail = true;
    return;
  }
  uint32_t offset = addr - kPmemBase;
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
  fread(pmem, 1, size, fp);
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
  top->ram_rdata = 0;
  g_mem_assert_en = false;

  // 复位
  for (int i=0; i<3; i++) {
    top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
    top->clk = 1; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  }
  
  top->rst = 0; // Release reset
  top->clk = 0; top->eval(); tfp->dump(contextp->time()); contextp->timeInc(1);
  
  g_mem_assert_en = true;

  uint64_t cycles = 0;
  const uint64_t max_cycles = 1000000;

  while (!contextp->gotFinish() && !g_ebreak && !g_mem_assert_fail && cycles < max_cycles) {
    top->clk = 1; top->eval();
    uint32_t pc = top->pc;

    // 1. 根据稳定且不变的此周期PC去取指令
    if (!in_pmem(pc)) {
      printf("\033[1;31m[BAD] pc=0x%08x out of range\033[0m\n", pc);
      g_mem_assert_fail = true;
      break;
    }
    top->inst = pmem_read(pc);

    // 2. 运算产生的第一阶段组合逻辑结果（此时已消除绝大部份毛刺）
    top->eval();

    // 3. 内存系统根据组合逻辑生成的稳定 ram_addr/ram_valid 等信息，对数据内存进行访问
    if (top->ram_valid && !top->ram_wen) {
      top->ram_rdata = pmem_read(top->ram_addr);
    } else {
      top->ram_rdata = 0;
    }

    // 4. 将读取的读内存数据再次流入计算，得到最终完整的正确计算结果
    top->eval();

    // 5. 同步写入：此时组合逻辑完全稳定，执行一次真实的物理内存写入
    if (top->ram_valid && top->ram_wen && !top->rst) {
      pmem_write(top->ram_addr, top->ram_wdata, top->ram_wmask);
    }

    tfp->dump(contextp->time());
    contextp->timeInc(1);

    // --- 时钟下降沿 ---
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
