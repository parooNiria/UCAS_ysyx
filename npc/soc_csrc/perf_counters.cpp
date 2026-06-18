// ===========================================================================
// Performance Counters — all counting done in C++ via DPI-C
// Called every clock cycle from PerfEventDPI BlackBox.
// Report called from SimEnv::run() at simulation end.
// ===========================================================================
#include <cstdint>
#include <cstdio>

// ── Cycle counter ──
static uint64_t perf_cycle = 0;

// ── IFU counters ──
static uint64_t cnt_ifu_fetch       = 0;

// ── IDU instruction category counters ──
static uint64_t cnt_idu_compute     = 0;
static uint64_t cnt_idu_branch      = 0;
static uint64_t cnt_idu_jump        = 0;
static uint64_t cnt_idu_load        = 0;
static uint64_t cnt_idu_store       = 0;
static uint64_t cnt_idu_csr         = 0;
static uint64_t cnt_idu_system      = 0;

// ── EXU counters ──
static uint64_t cnt_exu_compute     = 0;

// ── LSU counters ──
static uint64_t cnt_lsu_load_done   = 0;
static uint64_t cnt_lsu_store_done  = 0;

// ── LSU latency tracking ──
static uint64_t lsu_load_issue_cycle  = 0;
static bool     lsu_load_pending      = false;
static uint64_t lsu_load_latency_sum  = 0;
static uint64_t lsu_load_latency_cnt  = 0;

static uint64_t lsu_store_issue_cycle = 0;
static bool     lsu_store_pending     = false;
static uint64_t lsu_store_latency_sum = 0;
static uint64_t lsu_store_latency_cnt = 0;

// ── ICache counters (for AMAT) ──
static uint64_t cnt_icache_access     = 0;
static uint64_t cnt_icache_hit        = 0;
static uint64_t cnt_icache_miss_cycles = 0;

// ── Per-category cycle tracking ──
#define CAT_QUEUE_DEPTH 16
static uint64_t cat_queue_cycle[CAT_QUEUE_DEPTH] = {0};
static int      cat_queue_cat[CAT_QUEUE_DEPTH]   = {0};
static bool     cat_queue_valid[CAT_QUEUE_DEPTH] = {false};
static int      cat_enq_ptr = 0;
static int      cat_deq_ptr = 0;

static uint64_t cat_compute_cycles = 0;
static uint64_t cat_branch_cycles  = 0;
static uint64_t cat_jump_cycles    = 0;
static uint64_t cat_load_cycles    = 0;
static uint64_t cat_store_cycles   = 0;
static uint64_t cat_csr_cycles     = 0;
static uint64_t cat_system_cycles  = 0;

static void cat_enqueue(int cat) {
    if (cat_enq_ptr == cat_deq_ptr && cat_queue_valid[cat_deq_ptr]) return;
    cat_queue_cycle[cat_enq_ptr] = perf_cycle;
    cat_queue_cat[cat_enq_ptr]   = cat;
    cat_queue_valid[cat_enq_ptr] = true;
    cat_enq_ptr = (cat_enq_ptr + 1) % CAT_QUEUE_DEPTH;
}

static void cat_dequeue() {
    if (!cat_queue_valid[cat_deq_ptr]) return;
    uint64_t latency = perf_cycle - cat_queue_cycle[cat_deq_ptr];
    int cat = cat_queue_cat[cat_deq_ptr];
    switch (cat) {
        case 0: cat_compute_cycles += latency; break;
        case 1: cat_branch_cycles  += latency; break;
        case 2: cat_jump_cycles    += latency; break;
        case 3: cat_load_cycles    += latency; break;
        case 4: cat_store_cycles   += latency; break;
        case 5: cat_csr_cycles     += latency; break;
        case 6: cat_system_cycles  += latency; break;
    }
    cat_queue_valid[cat_deq_ptr] = false;
    cat_deq_ptr = (cat_deq_ptr + 1) % CAT_QUEUE_DEPTH;
}

// ── Print analysis report ──
void perf_print_report() {
    uint64_t total_idu = cnt_idu_compute + cnt_idu_branch + cnt_idu_jump +
                         cnt_idu_load + cnt_idu_store + cnt_idu_csr + cnt_idu_system;
    double ipc = (perf_cycle > 0) ? (double)total_idu / (double)perf_cycle : 0.0;

    printf("\n");
    printf("\033[36m==================== Performance Counter Report ====================\033[0m\n");
    printf("  Total cycles:        %llu\n", (unsigned long long)perf_cycle);
    printf("  Total instructions:  %llu\n", (unsigned long long)total_idu);
    printf("  IPC:                 %.4f\n", ipc);

    // ── Instruction mix ──
    printf("\033[36m--- Instruction Category Distribution ---\033[0m\n");
    if (total_idu > 0) {
        printf("  Compute  (ALU/R/I/U): %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_compute, 100.0 * cnt_idu_compute / total_idu);
        printf("  Branch   (B-type)   : %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_branch,  100.0 * cnt_idu_branch  / total_idu);
        printf("  Jump     (JAL/JALR) : %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_jump,    100.0 * cnt_idu_jump    / total_idu);
        printf("  Load                : %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_load,    100.0 * cnt_idu_load    / total_idu);
        printf("  Store               : %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_store,   100.0 * cnt_idu_store   / total_idu);
        printf("  CSR                 : %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_csr,     100.0 * cnt_idu_csr     / total_idu);
        printf("  System  (ECALL/EBRK): %8llu  (%5.1f%%)\n",
               (unsigned long long)cnt_idu_system,  100.0 * cnt_idu_system  / total_idu);
    }

    // ── Per-category average cycles ──
    printf("\033[36m--- Avg Execution Cycles per Category ---\033[0m\n");
    if (cnt_idu_compute > 0)
        printf("  Compute  : avg %6.2f cycles\n", (double)cat_compute_cycles / cnt_idu_compute);
    if (cnt_idu_branch > 0)
        printf("  Branch   : avg %6.2f cycles\n", (double)cat_branch_cycles  / cnt_idu_branch);
    if (cnt_idu_jump > 0)
        printf("  Jump     : avg %6.2f cycles\n", (double)cat_jump_cycles    / cnt_idu_jump);
    if (cnt_idu_load > 0)
        printf("  Load     : avg %6.2f cycles\n", (double)cat_load_cycles    / cnt_idu_load);
    if (cnt_idu_store > 0)
        printf("  Store    : avg %6.2f cycles\n", (double)cat_store_cycles   / cnt_idu_store);
    if (cnt_idu_csr > 0)
        printf("  CSR      : avg %6.2f cycles\n", (double)cat_csr_cycles     / cnt_idu_csr);
    if (cnt_idu_system > 0)
        printf("  System   : avg %6.2f cycles\n", (double)cat_system_cycles  / cnt_idu_system);

    // ── ICache AMAT ──
    uint64_t icache_misses = cnt_icache_access - cnt_icache_hit;
    double icache_hit_rate = (cnt_icache_access > 0)
        ? (double)cnt_icache_hit / (double)cnt_icache_access : 0.0;
    double icache_miss_rate = (cnt_icache_access > 0)
        ? (double)icache_misses / (double)cnt_icache_access : 0.0;
    double icache_miss_penalty = (icache_misses > 0)
        ? (double)cnt_icache_miss_cycles / (double)icache_misses : 0.0;
    double icache_amat = 2.0 + icache_miss_rate * icache_miss_penalty;

    printf("\033[36m--- ICache AMAT ---\033[0m\n");
    printf("  Cache accesses:     %llu\n", (unsigned long long)cnt_icache_access);
    printf("  Cache hits:         %llu\n", (unsigned long long)cnt_icache_hit);
    printf("  Cache misses:       %llu\n", (unsigned long long)icache_misses);
    printf("  Hit rate:           %5.1f%%\n", 100.0 * icache_hit_rate);
    printf("  Miss rate:          %5.1f%%\n", 100.0 * icache_miss_rate);
    printf("  Miss penalty:       %6.2f cycles\n", icache_miss_penalty);
    printf("  AMAT:               %6.2f cycles\n", icache_amat);

    // ── LSU latency ──
    printf("\033[36m--- LSU Memory Access Latency ---\033[0m\n");
    if (cnt_lsu_load_done > 0) {
        printf("  Loads completed :  %llu\n", (unsigned long long)cnt_lsu_load_done);
        printf("  Avg load latency:  %.2f cycles\n",
               (double)lsu_load_latency_sum / lsu_load_latency_cnt);
    }
    if (cnt_lsu_store_done > 0) {
        printf("  Stores completed:  %llu\n", (unsigned long long)cnt_lsu_store_done);
        printf("  Avg store latency: %.2f cycles\n",
               (double)lsu_store_latency_sum / lsu_store_latency_cnt);
    }

    printf("\033[36m====================================================================\033[0m\n");
    printf("\n");
}

// ===========================================================================
// DPI-C: called every clock cycle from Verilog PerfEventDPI
// ===========================================================================
extern "C" void dpi_perf_event(
    int ifu_fetch,
    int idu_compute,
    int idu_branch,
    int idu_jump,
    int idu_load,
    int idu_store,
    int idu_csr,
    int idu_system,
    int exu_compute,
    int exu_load_issue,
    int exu_store_issue,
    int lsu_load_done,
    int lsu_store_done,
    int wbu_commit,
    int icache_access,
    int icache_hit,
    int icache_miss_cycle)
{
    perf_cycle++;

    // ── IFU events ──
    if (ifu_fetch)    cnt_ifu_fetch++;

    // ── ICache events ──
    if (icache_access)     cnt_icache_access++;
    if (icache_hit)        cnt_icache_hit++;
    if (icache_miss_cycle) cnt_icache_miss_cycles++;

    // ── IDU decode: instruction categories ──
    bool idu_fire = false;
    int  idu_cat  = -1;
    if (idu_compute) { cnt_idu_compute++; idu_fire = true; idu_cat = 0; }
    if (idu_branch)  { cnt_idu_branch++;  idu_fire = true; idu_cat = 1; }
    if (idu_jump)    { cnt_idu_jump++;    idu_fire = true; idu_cat = 2; }
    if (idu_load)    { cnt_idu_load++;    idu_fire = true; idu_cat = 3; }
    if (idu_store)   { cnt_idu_store++;   idu_fire = true; idu_cat = 4; }
    if (idu_csr)     { cnt_idu_csr++;     idu_fire = true; idu_cat = 5; }
    if (idu_system)  { cnt_idu_system++;  idu_fire = true; idu_cat = 6; }
    if (idu_fire)    { cat_enqueue(idu_cat); }

    // ── EXU events ──
    if (exu_compute) cnt_exu_compute++;

    // ── LSU load latency tracking ──
    if (exu_load_issue) {
        lsu_load_issue_cycle = perf_cycle;
        lsu_load_pending = true;
    }
    if (lsu_load_done) {
        cnt_lsu_load_done++;
        if (lsu_load_pending) {
            lsu_load_latency_sum += (perf_cycle - lsu_load_issue_cycle);
            lsu_load_latency_cnt++;
            lsu_load_pending = false;
        }
    }
    // ── LSU store latency tracking ──
    if (exu_store_issue) {
        lsu_store_issue_cycle = perf_cycle;
        lsu_store_pending = true;
    }
    if (lsu_store_done) {
        cnt_lsu_store_done++;
        if (lsu_store_pending) {
            lsu_store_latency_sum += (perf_cycle - lsu_store_issue_cycle);
            lsu_store_latency_cnt++;
            lsu_store_pending = false;
        }
    }

    // ── WBU commit: dequeue for per-category latency ──
    if (wbu_commit) {
        cat_dequeue();
    }
}
