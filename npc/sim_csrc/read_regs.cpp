// Helper implementations to read registers and CSRs from the Verilated DUT
#include "include/common.h"
#include "Vtop.h"
#include <svdpi.h>
#include <stdint.h>

extern Vtop *top;

extern "C" int read_register(int idx) {
  if (!top) return 0;
  switch (idx) {
    case 0: return 0;
    case 1: return top->io_dbg_rf_1;
    case 2: return top->io_dbg_rf_2;
    case 3: return top->io_dbg_rf_3;
    case 4: return top->io_dbg_rf_4;
    case 5: return top->io_dbg_rf_5;
    case 6: return top->io_dbg_rf_6;
    case 7: return top->io_dbg_rf_7;
    case 8: return top->io_dbg_rf_8;
    case 9: return top->io_dbg_rf_9;
    case 10: return top->io_dbg_rf_10;
    case 11: return top->io_dbg_rf_11;
    case 12: return top->io_dbg_rf_12;
    case 13: return top->io_dbg_rf_13;
    case 14: return top->io_dbg_rf_14;
    case 15: return top->io_dbg_rf_15;
    case 16: return top->io_dbg_rf_16;
    case 17: return top->io_dbg_rf_17;
    case 18: return top->io_dbg_rf_18;
    case 19: return top->io_dbg_rf_19;
    case 20: return top->io_dbg_rf_20;
    case 21: return top->io_dbg_rf_21;
    case 22: return top->io_dbg_rf_22;
    case 23: return top->io_dbg_rf_23;
    case 24: return top->io_dbg_rf_24;
    case 25: return top->io_dbg_rf_25;
    case 26: return top->io_dbg_rf_26;
    case 27: return top->io_dbg_rf_27;
    case 28: return top->io_dbg_rf_28;
    case 29: return top->io_dbg_rf_29;
    case 30: return top->io_dbg_rf_30;
    case 31: return top->io_dbg_rf_31;
    default: return 0;
  }
}

extern "C" int read_csr(int addr) {
  if (!top) return 0;
  switch (addr) {
    case 0x300: return top->io_dbg_mstatus;
    case 0x305: return top->io_dbg_mtvec;
    case 0x341: return top->io_dbg_mepc;
    case 0x342: return top->io_dbg_mcause;
    default: return 0;
  }
}
