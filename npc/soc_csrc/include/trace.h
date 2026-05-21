#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdint.h>
#include <stdbool.h>

// Initialize trace system (capstone, log files)
bool init_trace();

// Close trace system and cleanup resources
void close_trace();

// Log one instruction to itrace
// @param pc: program counter
// @param inst: instruction word
// @param print_to_term: whether to print to terminal
// @param force_print: force print regardless of other settings
void log_itrace(uint32_t pc, uint32_t inst, bool print_to_term, bool force_print);

// Display recent instruction trace (for error reporting)
void display_recent_itrace();

#endif // __TRACE_H__
