#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern bool g_ebreak;
extern bool g_mem_assert_fail;
extern int inst_count;

#ifdef __cplusplus
extern "C" {
#endif

int read_register(int idx);
int read_csr(int addr);
void print_registers();
void display_trap_info(uint32_t pc, int a0, const char *reason);

// difftest
void init_difftest(const char *ref_so_file, long img_size, int port);
bool check_difftest(uint32_t npc_pc, uint32_t npc_next_pc,bool skip_compare);

// ebreak
void ebreak_notify();

#ifdef __cplusplus
}
#endif

#endif
