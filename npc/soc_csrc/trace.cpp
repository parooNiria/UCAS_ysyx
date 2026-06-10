#include "include/trace.h"
#include <capstone/capstone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *log_fp_itrace = NULL;
static csh capstone_handle;

#define RECENT_ITRACE_SIZE 16
static char recent_itrace[RECENT_ITRACE_SIZE][128];
static int recent_itrace_idx = 0;

// Control flags
static int disable_itrace = 0;

void set_disable_trace(bool v) { disable_itrace = v ? 1 : 0; }
bool get_disable_trace()       { return disable_itrace != 0; }

bool init_trace() {
  // Initialize Capstone for RISC-V 32-bit
  if (cs_open(CS_ARCH_RISCV, CS_MODE_RISCV32, &capstone_handle) != CS_ERR_OK) {
    printf("Failed to initialize capstone\n");
    return false;
  }
  
  // Open itrace log file
  if (!disable_itrace) {
    log_fp_itrace = fopen("build/npc-log-itrace.txt", "w");
    if (!log_fp_itrace) {
      printf("Failed to open itrace log file\n");
      return false;
    }
  }
  
  printf("[TRACE] Itrace system initialized\n");
  return true;
}

void close_trace() {
  if (log_fp_itrace) fclose(log_fp_itrace);
  cs_close(&capstone_handle);
}

void log_itrace(uint32_t pc, uint32_t inst, bool print_to_term, bool force_print) {
  cs_insn *insn;
  size_t count = cs_disasm(capstone_handle, (const uint8_t *)&inst, 4, pc, 0, &insn);
  
  char buf[256];
  
  if (count > 0) {
    snprintf(buf, sizeof(buf), "pc: 0x%08x | inst: 0x%08x | %s\t%s", 
             pc, inst, insn[0].mnemonic, insn[0].op_str);
    cs_free(insn, count);
  } else {
    snprintf(buf, sizeof(buf), "pc: 0x%08x | inst: 0x%08x | ERROR", pc, inst);
  }

  // Store in circular buffer
  strncpy(recent_itrace[recent_itrace_idx], buf, sizeof(recent_itrace[0]) - 1);
  recent_itrace[recent_itrace_idx][sizeof(recent_itrace[0]) - 1] = '\0';
  recent_itrace_idx = (recent_itrace_idx + 1) % RECENT_ITRACE_SIZE;

  // Write to log file
  if (log_fp_itrace) {
    fprintf(log_fp_itrace, "[ITRACE] %s\n", buf);
  }
  
  // Print to terminal if requested
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

