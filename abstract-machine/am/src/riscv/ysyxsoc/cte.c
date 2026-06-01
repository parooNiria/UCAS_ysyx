#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

static void __am_panic_on_return() {
  assert("kernel context returns");
}

typedef struct {
  void (*entry)(void *);
  void *arg;
} __am_kcontext_boot_t;

static void __am_kcontext_bootstrap(void *opaque) {
  __am_kcontext_boot_t *boot = (__am_kcontext_boot_t *)opaque;
  boot->entry(boot->arg);
  __am_panic_on_return();
}

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case 11:
        ev.event =  EVENT_YIELD;
        c->mepc += 4;
        break;
      default: 
      printf("Unhandled mcause: %lu \n", c->mcause); 
      ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  uintptr_t top = (uintptr_t)kstack.end;
  top &= ~((uintptr_t)sizeof(uintptr_t) - 1u);

  uintptr_t ctx_addr = top - sizeof(Context);
  ctx_addr &= ~((uintptr_t)sizeof(uintptr_t) - 1u);
  Context *ctx = (Context *)ctx_addr;

  uintptr_t boot_addr = ctx_addr - sizeof(__am_kcontext_boot_t);
  boot_addr &= ~((uintptr_t)sizeof(uintptr_t) - 1u);
  __am_kcontext_boot_t *boot = (__am_kcontext_boot_t *)boot_addr;

  if (kstack.start != NULL) {
    assert(boot_addr >= (uintptr_t)kstack.start);
  }

  boot->entry = entry;
  boot->arg = arg;

  *ctx = (Context){0};

  uintptr_t mstatus = 0;
  asm volatile("csrr %0, mstatus" : "=r"(mstatus));

  // Ensure mret returns to M-mode and enables interrupt from MPIE.
  const uintptr_t MSTATUS_MPP = (uintptr_t)(3u << 11);
  const uintptr_t MSTATUS_MPIE = (uintptr_t)(1u << 7);
  mstatus = (mstatus & ~((uintptr_t)(3u << 11))) | MSTATUS_MPP | MSTATUS_MPIE;

  ctx->mstatus = mstatus;
  ctx->mepc = (uintptr_t)__am_kcontext_bootstrap;
  ctx->mcause = 0;
  ctx->pdir = NULL;

  // a0 = stack-resident bootstrap record, ra = panic handler if bootstrap returns
  ctx->gpr[10] = (uintptr_t)boot;
  ctx->gpr[1] = (uintptr_t)__am_panic_on_return;

  return ctx;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  uintptr_t mstatus = 0;
  asm volatile("csrr %0, mstatus" : "=r"(mstatus));
  return (mstatus & (uintptr_t)(1u << 3)) != 0;
}

void iset(bool enable) {
  if (enable) {
    asm volatile("csrsi mstatus, 0x8");
  } else {
    asm volatile("csrci mstatus, 0x8");
  }
}