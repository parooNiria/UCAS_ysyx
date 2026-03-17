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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char expr[256];
  word_t old_val;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
static WP* new_wp();
static void free_wp(WP *wp);

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
static WP* new_wp() {
  if (free_ == NULL) {
    printf("No free watchpoint.\n");
    assert(0);
    return NULL;
  }
  WP *wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;
  return wp;
}

static void free_wp(WP *wp) {
  if (wp == NULL) return;
  WP *prev = NULL, *cur = head;
  while (cur != NULL) {
    if (cur == wp) {
      if (prev == NULL) head = cur->next;
      else prev->next = cur->next;
      cur->next = free_;
      free_ = cur;
      return;
    }
    prev = cur;
    cur = cur->next;
  }
  printf("Watchpoint %d not found.\n", wp->NO);
  assert(0);
}

void wp_display() {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("Num\tValue\t\tExpr\n");
  for (WP *p = head; p != NULL; p = p->next) {
    printf("%d\t" FMT_WORD "\t%s\n", p->NO, p->old_val, p->expr);
  }
}

void wp_add(const char *e) {
  if (e == NULL || *e == '\0') {
    printf("Usage: w EXPR\n");
    return;
  }

  bool success = false;
  word_t val = expr((char *)e, &success);
  if (!success) {
    printf("Bad expression: %s\n", e);
    return;
  }

  WP *wp = new_wp();
  strncpy(wp->expr, e, sizeof(wp->expr) - 1);
  wp->expr[sizeof(wp->expr) - 1] = '\0';
  wp->old_val = val;

  printf("Watchpoint %d: %s = " FMT_WORD "\n", wp->NO, wp->expr, wp->old_val);
}

void wp_delete(int no) {
  for (WP *p = head; p != NULL; p = p->next) {
    if (p->NO == no) {
      free_wp(p);
      printf("Watchpoint %d deleted.\n", no);
      return;
    }
  }
  printf("No watchpoint number %d.\n", no);
}

bool wp_check_update() {
  bool changed = false;
  for (WP *p = head; p != NULL; p = p->next) {
    bool success = false;
    word_t new_val = expr(p->expr, &success);
    if (!success) continue;

    if (new_val != p->old_val) {
      changed = true;
      printf("Watchpoint %d triggered: %s\n", p->NO, p->expr);
      printf("Old value = " FMT_WORD "\n", p->old_val);
      printf("New value = " FMT_WORD "\n", new_val);
      p->old_val = new_val;
    }
  }
  return changed;
}
