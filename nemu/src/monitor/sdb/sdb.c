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
#include <cpu/cpu.h>
#include <memory/vaddr.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <utils.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  int n = 1;
  if (args != NULL) {
    sscanf(args, "%d", &n);
  }
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Usage: info [r|w]\n");
    return 0;
  }
  switch (args[0]) {
    case 'r': isa_reg_display(); break;
    case 'w': wp_display(); break;
    default: printf("Unknown subcommand '%c'\n", args[0]);
  }
  return 0;
}

static int cmd_q(char *args) {
  set_nemu_state(NEMU_QUIT, cpu.pc, 0);
  return -1;
}

static int cmd_x(char *args) {
  int n;
  vaddr_t addr;
  if (args == NULL) {
    printf("Usage: x N EXPR\n");
    return 0;
  }
  char *n_str = strtok(args, " ");
  char *addr_expr = NULL;
  if (n_str != NULL) {
    addr_expr = n_str + strlen(n_str) + 1;
    while (*addr_expr == ' ') addr_expr ++;
    if (*addr_expr == '\0') addr_expr = NULL;
  }

  if (n_str == NULL || addr_expr == NULL || sscanf(n_str, "%d", &n) != 1) {
    printf("Usage: x N EXPR\n");
    return 0;
  }

  bool success = false;
  addr = (vaddr_t)expr(addr_expr, &success);
  if (!success) {
    printf("Invalid expression\n");
    return 0;
  }

  for (int i = 0; i < n; i ++) {
    printf(FMT_WORD ": " FMT_WORD "\n",
        (word_t)(addr + i * sizeof(word_t)),
        vaddr_read(addr + i * sizeof(word_t), sizeof(word_t)));
  }
  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }
  bool success = false;
  word_t result = expr(args, &success);
  if (success) {
    printf(FMT_WORD "\n", result);
  }
  else {
    printf("Invalid expression\n");
  }
  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) {
    printf("Usage: w EXPR\n");
    return 0;
  }
  wp_add(args);
  return 0;
}

static int cmd_d(char *args) {
  if (args == NULL) {
    printf("Usage: d N\n");
    return 0;
  }
  int no = -1;
  if (sscanf(args, "%d", &no) != 1) {
    printf("Usage: d N\n");
    return 0;
  }
  wp_delete(no);
  return 0;
}

static int cmd_test_expr(char *args) {
  const char *file_path = (args == NULL) ? "tools/gen-expr/input" : args;
  FILE *fp = fopen(file_path, "r");
  if (fp == NULL) {
    printf("Can not open %s\n", file_path);
    return 0;
  }

  int total = 0, pass = 0;
  char *line = NULL;
  size_t cap = 0;
  while (getline(&line, &cap, fp) != -1) {
    char *saveptr = NULL;
    char *expected_str = strtok_r(line, " \t\n", &saveptr);
    char *e = saveptr;

    if (expected_str == NULL || e == NULL) {
      continue;
    }

    while (*e == ' ' || *e == '\t') e ++;
    char *end = e + strlen(e) - 1;
    while (end >= e && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
      *end = '\0';
      end --;
    }
    if (*e == '\0') continue;

    unsigned expected = (unsigned)strtoul(expected_str, NULL, 10);

    bool success = false;
    word_t got = expr(e, &success);
    total ++;

    if (success && (unsigned)got == expected) {
      pass ++;
    } else {
      printf("[FAIL] expr: %s\n", e);
      printf("       expect=%u got=%u success=%d\n", expected, (unsigned)got, success ? 1 : 0);
    }
  }
  free(line);
  fclose(fp);

  printf("expr test done: %d/%d passed\n", pass, total);
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  {"si","Step through the program n instructions (default 1 instruction)", cmd_si},
  {"info","Print the status", cmd_info},
  {"p","Evaluate the expression EXPR and print the result", cmd_p},
  {"x","Scan the memory", cmd_x},
  {"w","Set a watchpoint", cmd_w},
  {"d","Delete a watchpoint by number", cmd_d},
  {"testexpr","Test expr() with generated input file", cmd_test_expr},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
