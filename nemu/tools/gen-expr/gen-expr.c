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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static size_t buf_p = 0;

static int choose(int n) {
  return rand() % n;
}

static void append_to_buf(const char *fmt, ...) {
  if (buf_p >= sizeof(buf) - 1) return;

  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + buf_p, sizeof(buf) - buf_p, fmt, ap);
  va_end(ap);

  if (n < 0) return;
  if ((size_t)n >= sizeof(buf) - buf_p) {
    buf_p = sizeof(buf) - 1;
    buf[buf_p] = '\0';
  } else {
    buf_p += (size_t)n;
  }
}

static void gen_num(bool non_zero) {
  uint32_t v = non_zero ? (uint32_t)(choose(100) + 1) : (uint32_t)choose(100);
  if (choose(100) < 50) {
    append_to_buf("%u", v);
  } else {
    append_to_buf("0x%x", v);
  }
}

static void gen_rand_expr_rec(int depth) {
  const int MAX_DEPTH = 4;

  if (depth >= MAX_DEPTH || choose(100) < 35) {
    gen_num(false);
    return;
  }

  int t = choose(2);
  if (t == 0) {
    append_to_buf("(");
    gen_rand_expr_rec(depth + 1);
    append_to_buf(")");
    return;
  }

  gen_rand_expr_rec(depth + 1);
  int op = choose(7);
  if (op == 0) append_to_buf(" + ");
  else if (op == 1) append_to_buf(" - ");
  else if (op == 2) append_to_buf(" * ");
  else if (op == 3) append_to_buf(" / ");
  else if (op == 4) append_to_buf(" == ");
  else if (op == 5) append_to_buf(" != ");
  else append_to_buf(" && ");

  if (op == 3) {
    gen_num(true);  // avoid division by zero
  } else {
    gen_rand_expr_rec(depth + 1);
  }
}

static void gen_rand_expr() {
  buf_p = 0;
  buf[0] = '\0';
  gen_rand_expr_rec(0);
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    unsigned result;
    ret = fscanf(fp, "%u", &result);
    pclose(fp);

    if (ret != 1) continue;

    printf("%u %s\n", result, buf);
  }
  return 0;
}
