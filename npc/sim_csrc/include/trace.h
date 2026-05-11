#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdint.h>
#include <stdbool.h>

bool init_trace();
void close_trace();
void log_itrace(uint32_t pc, uint32_t inst, bool print_to_term, bool force_print);
void display_recent_itrace();

void log_mtrace_read(uint32_t addr, int data);
void log_mtrace_write(uint32_t addr, uint32_t data, uint8_t mask);
void log_mtrace_err(const char *msg, uint32_t addr);

#endif
