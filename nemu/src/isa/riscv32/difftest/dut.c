/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/difftest.h>
#include "../local-include/reg.h"

bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  bool match = true;
  int reg_num = MUXDEF(CONFIG_RVE, 16, 32);
  for (int i = 0; i < reg_num; i++) {
    if (ref_r->gpr[i] != cpu.gpr[i]) {
      match = false;
      printf("Difftest fail at pc = " FMT_WORD "\n", pc);
      printf("  Register %s differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", reg_name(i), ref_r->gpr[i], cpu.gpr[i]);
    }
  }
  if (ref_r->pc != cpu.pc) {
    match = false;
    printf("Difftest fail at pc = " FMT_WORD "\n", pc);
    printf("  PC differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", ref_r->pc, cpu.pc);
  }
  if (ref_r->mstatus != cpu.mstatus) {
    match = false;
    printf("Difftest fail at pc = " FMT_WORD "\n", pc);
    printf("  mstatus differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", ref_r->mstatus, cpu.mstatus);
  }
  if (ref_r->mepc != cpu.mepc) {
    match = false;
    printf("Difftest fail at pc = " FMT_WORD "\n", pc);
    printf("  mepc differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", ref_r->mepc, cpu.mepc);
  }
  if (ref_r->mcause != cpu.mcause) {
    match = false;
    printf("Difftest fail at pc = " FMT_WORD "\n", pc);
    printf("  mcause differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", ref_r->mcause, cpu.mcause);
  }
  if (ref_r->mtvec != cpu.mtvec) {
    match = false;
    printf("Difftest fail at pc = " FMT_WORD "\n", pc);
    printf("  mtvec differs: ref = " FMT_WORD ", nemu = " FMT_WORD "\n", ref_r->mtvec, cpu.mtvec);
  }
  return match;
}

void isa_difftest_attach() {
}
