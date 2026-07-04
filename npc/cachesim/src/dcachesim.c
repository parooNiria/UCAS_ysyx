// ===========================================================================
// dcachesim — Offline Data Cache Simulator
// Reads dtrace logs (from NPC DtraceDPI) and simulates a set-associative
// data cache. Supports write-through + write-allocate policies.
//
// Usage: dcachesim <trace_file> [nways] [nsets] [blocksize] [hit_lat] [miss_pen] [policy]
//   policy: 0 = write-through + no-write-allocate (default)
//           1 = write-back    + write-allocate
// ===========================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

// ---------------------------------------------------------------------------
// Cache data structures
// ---------------------------------------------------------------------------
typedef struct {
    int      nways, nsets, block_size;
    int      offset_bits, index_bits;
    uint32_t *tags;
    uint8_t  *valid;
    uint8_t  *dirty;        // only used for write-back policy
    int      repl_ptr;      // round-robin replacement counter

    // Statistics
    uint64_t access_cnt;
    uint64_t load_cnt,  load_hit,  load_miss;
    uint64_t store_cnt, store_hit, store_miss;
    uint64_t writeback_cnt; // dirty evictions (write-back only)

    // Timing
    int hit_lat, miss_pen;
} DCache;

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
static int log2_ceil(int x) {
    int v = 1, r = 0;
    while (v < x) { v <<= 1; r++; }
    return r;
}

// ---------------------------------------------------------------------------
// Cache init / free
// ---------------------------------------------------------------------------
void dcache_init(DCache *c, int nways, int nsets, int blk,
                 int hit_lat, int miss_pen, int policy) {
    (void)policy; // stored for future use
    c->nways  = nways;
    c->nsets  = nsets;
    c->block_size = blk;
    c->offset_bits = log2_ceil(blk);
    c->index_bits  = log2_ceil(nsets);

    int total = nways * nsets;
    c->tags  = (uint32_t*)calloc(total, sizeof(uint32_t));
    c->valid = (uint8_t*) calloc(total, sizeof(uint8_t));
    c->dirty = (uint8_t*) calloc(total, sizeof(uint8_t));
    c->repl_ptr = 0;

    c->access_cnt = c->load_cnt = c->load_hit = c->load_miss = 0;
    c->store_cnt  = c->store_hit = c->store_miss = 0;
    c->writeback_cnt = 0;

    c->hit_lat  = hit_lat;
    c->miss_pen = miss_pen;
}

void dcache_free(DCache *c) {
    free(c->tags); free(c->valid); free(c->dirty);
}

// ---------------------------------------------------------------------------
// Tag / index extraction
// ---------------------------------------------------------------------------
static inline uint32_t cache_tag(const DCache *c, uint32_t addr) {
    return addr >> (c->offset_bits + c->index_bits);
}
static inline uint32_t cache_index(const DCache *c, uint32_t addr) {
    return (addr >> c->offset_bits) & ((1u << c->index_bits) - 1);
}

// ---------------------------------------------------------------------------
// Cache lookup — returns way index if hit, -1 if miss
// ---------------------------------------------------------------------------
static int cache_lookup(const DCache *c, uint32_t addr) {
    uint32_t tag   = cache_tag(c, addr);
    uint32_t index = cache_index(c, addr);
    for (int w = 0; w < c->nways; w++) {
        int slot = w * c->nsets + (int)index;
        if (c->valid[slot] && c->tags[slot] == tag) {
            return w;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Cache fill — allocate a line for addr (evict victim if needed)
// Returns non-zero if a dirty line was written back.
// ---------------------------------------------------------------------------
static int cache_fill(DCache *c, uint32_t addr, int write_back) {
    uint32_t tag   = cache_tag(c, addr);
    uint32_t index = cache_index(c, addr);
    int victim_way = c->repl_ptr;
    c->repl_ptr = (c->repl_ptr + 1) % c->nways;

    int slot = victim_way * c->nsets + (int)index;
    int wb = 0;

    if (write_back && c->valid[slot] && c->dirty[slot]) {
        wb = 1;  // dirty eviction → writeback
    }

    c->tags[slot]  = tag;
    c->valid[slot] = 1;
    c->dirty[slot] = 0;
    return wb;
}

// ---------------------------------------------------------------------------
// Read access
// Returns: 1 = hit, 0 = miss
// ---------------------------------------------------------------------------
static int dcache_read(DCache *c, uint32_t addr) {
    c->access_cnt++;
    c->load_cnt++;

    int way = cache_lookup(c, addr);
    if (way >= 0) {
        c->load_hit++;
        return 1;
    }

    // Read miss → allocate
    c->load_miss++;
    c->writeback_cnt += cache_fill(c, addr, 0 /* write-back flag not needed
        for read-allocate; but we track dirty anyway for policy 1 */);
    return 0;
}

// ---------------------------------------------------------------------------
// Write access (write-through, no-write-allocate)
// Returns: 1 = cache hit, 0 = cache miss (always goes to memory)
// ---------------------------------------------------------------------------
static int dcache_write_wt(DCache *c, uint32_t addr) {
    c->access_cnt++;
    c->store_cnt++;

    int way = cache_lookup(c, addr);
    if (way >= 0) {
        c->store_hit++;
        return 1;
    }

    // Write miss → no allocate in WT mode
    c->store_miss++;
    return 0;
}

// ---------------------------------------------------------------------------
// Write access (write-back, write-allocate)
// Returns: 1 = cache hit, 0 = cache miss
// ---------------------------------------------------------------------------
static int dcache_write_wb(DCache *c, uint32_t addr) {
    c->access_cnt++;
    c->store_cnt++;

    int way = cache_lookup(c, addr);
    if (way >= 0) {
        c->store_hit++;
        // Mark dirty
        uint32_t index = cache_index(c, addr);
        int slot = way * c->nsets + (int)index;
        c->dirty[slot] = 1;
        return 1;
    }

    // Write miss → allocate, mark dirty
    c->store_miss++;
    c->writeback_cnt += cache_fill(c, addr, 1 /* write-back */);
    // Mark the newly allocated line dirty
    uint32_t index = cache_index(c, addr);
    for (int w = 0; w < c->nways; w++) {
        int slot = w * c->nsets + (int)index;
        if (c->valid[slot] && c->tags[slot] == cache_tag(c, addr)) {
            c->dirty[slot] = 1;
            break;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Timing computation
// ---------------------------------------------------------------------------
double dcache_amat(const DCache *c, int policy) {
    if (c->access_cnt == 0) return 0.0;

    if (policy == 0) {
        // Write-through + no-write-allocate:
        //   load hit  = hit_lat,  load miss = miss_pen
        //   store      = miss_pen (always goes to memory)
        uint64_t total_cycles =
            c->load_hit  * c->hit_lat +
            c->load_miss * c->miss_pen +
            c->store_cnt * c->miss_pen;
        return (double)total_cycles / c->access_cnt;
    } else {
        // Write-back + write-allocate:
        //   load hit   = hit_lat,  load miss  = miss_pen
        //   store hit  = hit_lat,  store miss = miss_pen
        //   + writeback penalty for dirty evictions
        uint64_t total_cycles =
            (c->load_hit  + c->store_hit)  * c->hit_lat +
            (c->load_miss + c->store_miss) * c->miss_pen +
            c->writeback_cnt * c->miss_pen;  // additional writeback cycles
        return (double)total_cycles / c->access_cnt;
    }
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------
void dcache_report(const DCache *c, int policy) {
    double load_mr  = c->load_cnt  ? (double)c->load_miss  / c->load_cnt  : 0.0;
    double store_mr = c->store_cnt ? (double)c->store_miss / c->store_cnt : 0.0;
    double total_mr = c->access_cnt ? (double)(c->load_miss + c->store_miss) / c->access_cnt : 0.0;
    double amat = dcache_amat(c, policy);
    uint64_t total_hits = c->load_hit + c->store_hit;
    uint64_t total_misses = c->load_miss + c->store_miss;

    const char *pol_name = (policy == 0) ?
        "Write-through + No-write-allocate" :
        "Write-back + Write-allocate";

    fprintf(stderr,
        "========================================\n"
        "DCacheSim Results\n"
        "  Config:     %d-way, %d sets, %dB blocks  (%d KB)\n"
        "  Policy:     %s\n"
        "  Latency:    hit=%d cycles  miss_penalty=%d cycles\n"
        "----------------------------------------\n"
        "  Total accesses: %" PRIu64 "\n"
        "    Loads:        %" PRIu64 "  (hits: %" PRIu64 ", misses: %" PRIu64 ", MR: %.2f%%)\n"
        "    Stores:       %" PRIu64 "  (hits: %" PRIu64 ", misses: %" PRIu64 ", MR: %.2f%%)\n"
        "  Overall hits:   %" PRIu64 "\n"
        "  Overall misses: %" PRIu64 "\n"
        "  Overall MR:     %.2f%%\n"
        "  Writebacks:     %" PRIu64 "\n"
        "  AMAT:           %.4f cycles\n"
        "========================================\n",
        c->nways, c->nsets, c->block_size,
        (c->nways * c->nsets * c->block_size) / 1024,
        pol_name,
        c->hit_lat, c->miss_pen,
        c->access_cnt,
        c->load_cnt, c->load_hit, c->load_miss, load_mr * 100.0,
        c->store_cnt, c->store_hit, c->store_miss, store_mr * 100.0,
        total_hits,
        total_misses,
        total_mr * 100.0,
        c->writeback_cnt,
        amat);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <trace_file> [nways=%d] [nsets=%d] [blocksize=%d] [hit_lat=%d] [miss_pen=%d] [policy=%d]\n"
            "  policy: 0=write-through+no-write-allocate (default)\n"
            "          1=write-back+write-allocate\n",
            argv[0], 2, 64, 32, 1, 50, 0);
        return 1;
    }

    const char *fname = argv[1];
    int nways    = argc > 2 ? atoi(argv[2]) : 2;
    int nsets    = argc > 3 ? atoi(argv[3]) : 64;
    int blk      = argc > 4 ? atoi(argv[4]) : 32;
    int hit_lat  = argc > 5 ? atoi(argv[5]) : 1;
    int miss_pen = argc > 6 ? atoi(argv[6]) : 50;
    int policy   = argc > 7 ? atoi(argv[7]) : 0;

    DCache cache;
    dcache_init(&cache, nways, nsets, blk, hit_lat, miss_pen, policy);

    FILE *fp = fopen(fname, "r");
    if (!fp) { perror(fname); return 1; }

    char line[512];
    uint64_t line_no = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char type;
        unsigned int addr;
        int size;
        unsigned int wdata, wstrb;

        // Parse: [<cycle>] <type> <addr> <size> [<wdata> <wstrb>]
        int n = sscanf(line, " [%*d] %c 0x%x %d 0x%x 0x%x",
                       &type, &addr, &size, &wdata, &wstrb);

        if (n < 3) {
            // Try without the leading bracket+cycle format
            n = sscanf(line, "%c 0x%x %d 0x%x 0x%x",
                       &type, &addr, &size, &wdata, &wstrb);
            if (n < 3) {
                fprintf(stderr, "Warning: skipping malformed line %" PRIu64 ": %s", line_no, line);
                continue;
            }
        }

        // Only simulate accesses to cacheable memory regions
        // PSRAM:  0x80000000 - 0x803FFFFF (4MB)
        // SDRAM:  0xA0000000 - 0xA3FFFFFF (32MB)
        // Flash:  0x30000000 - 0x30FFFFFF (XIP, read-only, typically not cached)
        int cacheable = (addr >= 0x80000000u && addr < 0x80400000u) ||
                        (addr >= 0xA0000000u && addr < 0xA4000000u);
        if (!cacheable) continue;

        if (type == 'L') {
            dcache_read(&cache, (uint32_t)addr);
        } else if (type == 'S') {
            if (policy == 0) {
                dcache_write_wt(&cache, (uint32_t)addr);
            } else {
                dcache_write_wb(&cache, (uint32_t)addr);
            }
        }
    }
    fclose(fp);

    // stdout: AMAT value only (for scripting / DSE)
    printf("%.4f\n", dcache_amat(&cache, policy));
    fflush(stdout);

    // stderr: detailed report
    dcache_report(&cache, policy);

    dcache_free(&cache);
    return 0;
}
