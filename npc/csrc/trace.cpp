#include "include/trace.h"
#include <capstone/capstone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *log_fp_mtrace = NULL;
static FILE *log_fp_itrace = NULL;
static csh capstone_handle;

#define RECENT_ITRACE_SIZE 16
static char recent_itrace[RECENT_ITRACE_SIZE][128];
static int recent_itrace_idx = 0;

bool init_trace() {
  if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &capstone_handle) != CS_ERR_OK) {
    printf("Failed to initialize capstone\n");
    return false;
  }

  log_fp_itrace = fopen("build/npc-log-itrace.txt", "w");
  if (!log_fp_itrace) {
    printf("Failed to open itrace log file\n");
    return false;
  }
  log_fp_mtrace = fopen("build/npc-log-mtrace.txt", "w");
  if (!log_fp_mtrace) {
    printf("Failed to open mtrace log file\n");
    return false;
  }
  return true;
}

void close_trace() {
  if (log_fp_itrace) fclose(log_fp_itrace);
  if (log_fp_mtrace) fclose(log_fp_mtrace);
  cs_close(&capstone_handle);
}

void log_itrace(uint32_t pc, uint32_t inst, bool print_to_term, bool force_print) {
  cs_insn *insn;
  size_t count = cs_disasm(capstone_handle, (const uint8_t *)&inst, 4, pc, 0, &insn);
  
  char buf[128];
  
  if (count > 0) {
    snprintf(buf, sizeof(buf), "pc: 0x%08x | inst: 0x%08x | %s\t%s", pc, inst, insn[0].mnemonic, insn[0].op_str);
    cs_free(insn, count);
  } else {
    snprintf(buf, sizeof(buf), "pc: 0x%08x | inst: 0x%08x | ERROR", pc, inst);
  }

  strncpy(recent_itrace[recent_itrace_idx], buf, sizeof(recent_itrace[0]));
  recent_itrace_idx = (recent_itrace_idx + 1) % RECENT_ITRACE_SIZE;

  if (log_fp_itrace) {
    fprintf(log_fp_itrace, "[ITRACE] %s\n", buf);
  }
  if (print_to_term || force_print) {
    printf("[ITRACE] %s\n", buf);
  }
}

void display_recent_itrace() {
  printf("\n\033[1;31m[Recent Instructions]\033[0m\n");
  int start_idx = recent_itrace_idx;
  for (int i = 0; i < RECENT_ITRACE_SIZE; i++) {
    int idx = (start_idx + i) % RECENT_ITRACE_SIZE;
    if (recent_itrace[idx][0] != '\0') {
      if (i == RECENT_ITRACE_SIZE - 1) {
        printf("--> %s\n", recent_itrace[idx]);
      } else {
        printf("    %s\n", recent_itrace[idx]);
      }
    }
  }
  printf("\n");
}


void log_mtrace_read(uint32_t addr, int data) {
  if (log_fp_mtrace) {
    fprintf(log_fp_mtrace, "[MTACE] R addr=0x%08x data=0x%08x\n", addr, data);
  }
}

void log_mtrace_write(uint32_t addr, uint32_t data, uint8_t mask) {
  if (log_fp_mtrace) {
    fprintf(log_fp_mtrace, "[MTACE] W addr=0x%08x data=0x%08x mask=0x%02x\n", addr, data, mask);
  }
}

void log_mtrace_err(const char *msg, uint32_t addr) {
  if (log_fp_mtrace) {
    fprintf(log_fp_mtrace, "[ERROR] %s addr=0x%08x out of range\n", msg, addr);
  }
}
