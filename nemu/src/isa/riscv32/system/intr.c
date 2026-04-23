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
#define MSTATUS_MIE_MASK     (1 << 3)
#define MSTATUS_MPIE_MASK    (1 << 7)
#define MSTATUS_MPP_MASK     (3 << 11)
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */
  cpu.mepc = epc;
  cpu.mcause = NO;
  cpu.mstatus &= ~MSTATUS_MPIE_MASK;                // 先清 MPIE 位
  cpu.mstatus = (cpu.mstatus & ~MSTATUS_MPIE_MASK) | ((cpu.mstatus & MSTATUS_MIE_MASK) << 4);
  cpu.mstatus &= ~MSTATUS_MIE_MASK;
  cpu.mstatus &= ~MSTATUS_MPP_MASK;
  cpu.mstatus |= (3 << 11);  // MPP = M-mode
#ifdef CONFIG_ETRACE
  log_write("Exception triggered: mcause = 0x%x, mepc = " FMT_WORD " -> mtvec = " FMT_WORD, NO, epc, cpu.mtvec);
#endif
  return cpu.mtvec;
}

word_t isa_mret() {
    cpu.mstatus = (cpu.mstatus & ~MSTATUS_MIE_MASK) 
                | ((cpu.mstatus & MSTATUS_MPIE_MASK) >> 4);
    cpu.mstatus |= MSTATUS_MPIE_MASK;

    cpu.mstatus &= ~MSTATUS_MPP_MASK;

#ifdef CONFIG_ETRACE
    log_write("Return from exception: mstatus = " FMT_WORD ", returning to mepc = " FMT_WORD, cpu.mstatus, cpu.mepc);
#endif

    // 返回异常返回地址
    return cpu.mepc;
}

word_t isa_query_intr() {
  return INTR_EMPTY;
}
