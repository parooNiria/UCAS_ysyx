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
#include <memory/vaddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NEQ, TK_AND,
  TK_DNUM, TK_HNUM,
  /* TODO: Add more token types */
  TK_REG,
  TK_DEREF
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"&&", TK_AND},
  {"==", TK_EQ},        // equal
  {"!=", TK_NEQ},
  {"\\+", '+'},         // plus
  {"-", '-'},          // minus
  {"\\*", '*'},         // multiply
  {"/", '/'},           // divide
  {"\\(", '('},         // left parenthese
  {"\\)", ')'},         // right parenthese
  {"0[xX][0-9a-fA-F]+u?", TK_HNUM}, // hexadecimal number
  {"[0-9]+u?", TK_DNUM},            // decimal number
  {"\\$([A-Za-z0-9]+)", TK_REG}, // register, e.g. $x10/$a0
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
//编译正则规则
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[1024] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        log_write("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        if(substr_len >= 32) {
          assert(0);
        }
        if (nr_token >= (int)ARRLEN(tokens)) {
          printf("too many tokens in expression\n");
          return false;
        }
        switch (rules[i].token_type) {
          case TK_NOTYPE: break;  
          case TK_REG:
          {
            tokens[nr_token].type = TK_REG;
            strncpy(tokens[nr_token].str, substr_start + 1, substr_len - 1);
            tokens[nr_token].str[substr_len - 1] = '\0';
            nr_token ++;
            break;
          }
          case TK_DNUM:
          case TK_HNUM:
            tokens[nr_token].type = TK_DNUM;
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token ++;
            break;
          case '+': case '-': case '*': case '/': case '(': case ')':
          case TK_EQ: case TK_NEQ: case TK_AND:
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = rules[i].token_type;
            tokens[nr_token].str[1] = '\0';
            nr_token ++;
            break;
          default: panic("unknown token type: %s", rules[i].regex);
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '*') {
      if (i == 0 || !(tokens[i - 1].type == TK_DNUM || tokens[i - 1].type == ')')) {
        tokens[i].type = TK_DEREF;
      }
    }
  }

  return true;
}

static int precedence(int type) {
  switch (type) {
    case TK_AND: return 1;
    case TK_EQ:
    case TK_NEQ: return 2;
    case '+':
    case '-': return 3;
    case '*':
    case '/': return 4;
    default: return 0;
  }
}

static int op_sel(int s,int e){
  int level = 0;
  int best_pos = -1;
  int best_prec = 100;

  for(int i = e; i >= s; i --){
    if(tokens[i].type == ')') level ++;
    else if(tokens[i].type == '(') level --;
    else if(level == 0) {
      int prec = precedence(tokens[i].type);
      if (prec > 0 && prec < best_prec) {
        best_prec = prec;
        best_pos = i;
      }
    }
  }
  return best_pos;
}

static bool check_parentheses(int s, int e) {
  if (tokens[s].type != '(' || tokens[e].type != ')') return false;

  int level = 0;
  for (int i = s; i <= e; i ++) {
    if (tokens[i].type == '(') level ++;
    else if (tokens[i].type == ')') level --;

    if (level < 0) return false;
    if (level == 0 && i < e) return false;
  }
  return level == 0;
}

static word_t eval_expr(int s, int e, bool *success) {
  if(s > e || e >= nr_token) {
    *success = false;
    return 0;
  }
  else if(s == e) {
    if (tokens[s].type != TK_DNUM&&tokens[s].type != TK_HNUM&&tokens[s].type != TK_REG) {
      *success = false;
      return 0;
    }

    if(tokens[s].type == TK_REG) {
      return isa_reg_str2val(tokens[s].str, success);
    }
    *success = true;
    return strtoull(tokens[s].str, NULL, 0);
  }
  else {
    // Try binary ops first — this ensures deref is only evaluated
    // when the entire [s, e] range is a pure *expr, e.g. *0x80000000
    int op_pos = op_sel(s, e);
    if (op_pos >= 0) {
      word_t val1 = eval_expr(s, op_pos - 1, success);
      if (!*success) return 0;
      word_t val2 = eval_expr(op_pos + 1, e, success);
      if (!*success) return 0;

      *success = true;
      switch (tokens[op_pos].type) {
        case '+': return val1 + val2;
        case '-': return val1 - val2;
        case '*': return val1 * val2;
        case '/':
          if (val2 == 0) {
            *success = false;
            return 0;
          }
          return val1 / val2;
        case TK_EQ: return val1 == val2;
        case TK_NEQ: return val1 != val2;
        case TK_AND: return (val1 != 0) && (val2 != 0);
        default:
          *success = false;
          return 0;
      }
    }
    else if(check_parentheses(s, e)) {
      return eval_expr(s + 1, e - 1, success);
    }
    else if(tokens[s].type == TK_DEREF) {
      word_t addr;
      if(s+1<=e&&tokens[s+1].type == '(') {
        int level = 1;
        int i;
        for(i = s+2; i <= e; i ++){
          if(tokens[i].type == '(') level ++;
          else if(tokens[i].type == ')') level --;
          if(level == 0) break;
        }
        if(i > e) {
          *success = false;
          return 0;
        }
        addr = eval_expr(s + 1, i, success);
        if (!*success) return 0;
      }
      else {
        addr = eval_expr(s + 1, s + 1, success);
        if (!*success) return 0;
      }
      *success = true;
      return vaddr_read(addr, sizeof(word_t));
    }
    else {
      *success = false;
      return 0;
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  if (nr_token == 0) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  word_t result = eval_expr(0, nr_token - 1, success);
  return result;
}


