#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct {
    int nways, nsets, block_size;
    int offset_bits, index_bits, tag_bits;
    uint32_t *tags;       // [nways * nsets]
    uint8_t  *valid;      // [nways * nsets]  bitmask
    int repl_ptr;

    uint64_t access_cnt, hit_cnt, miss_cnt;
    int hit_lat, miss_pen;
} ChampCache;

static int log2_ceil(int x) {
    int v = 1, r = 0;
    while (v < x) { v <<= 1; r++; }
    return r;
}

void cache_init(ChampCache *c, int nways, int nsets, int blk, int hit_lat, int miss_pen) {
    c->nways  = nways;  c->nsets = nsets;  c->block_size = blk;
    c->offset_bits = log2_ceil(blk);
    c->index_bits  = log2_ceil(nsets);
    c->tag_bits    = 32 - c->offset_bits - c->index_bits;

    int total = nways * nsets;
    c->tags  = calloc(total, sizeof(uint32_t));
    c->valid = calloc(total, sizeof(uint8_t));
    c->repl_ptr = 0;
    c->access_cnt = c->hit_cnt = c->miss_cnt = 0;
    c->hit_lat = hit_lat;  c->miss_pen = miss_pen;
}

void cache_free(ChampCache *c) {
    free(c->tags); free(c->valid);
}

/* 一次 cache 访问, 返回 1=hit 0=miss */
int cache_access(ChampCache *c, uint32_t addr) {
    c->access_cnt++;
    uint32_t tag   = addr >> (c->offset_bits + c->index_bits);
    uint32_t index = (addr >> c->offset_bits) & ((1u << c->index_bits) - 1);

    for (int w = 0; w < c->nways; w++) {
        int slot = w * c->nsets + index;
        if (c->valid[slot] && c->tags[slot] == tag) {
            c->hit_cnt++;
            return 1;
        }
    }
    /* miss → round-robin replace */
    c->miss_cnt++;
    int refill_way = c->repl_ptr;
    c->repl_ptr = (c->repl_ptr + 1) % c->nways;
    int slot = refill_way * c->nsets + index;
    c->tags[slot]  = tag;
    c->valid[slot] = 1;
    return 0;
}

void cache_reset(ChampCache *c) {
    int total = c->nways * c->nsets;
    memset(c->tags,  0, total * sizeof(uint32_t));
    memset(c->valid, 0, total * sizeof(uint8_t));
    c->repl_ptr = 0;
    c->access_cnt = c->hit_cnt = c->miss_cnt = 0;
}

double cache_amat(const ChampCache *c) {
    if (c->access_cnt == 0) return 0.0;
    double miss_rate = (double)c->miss_cnt / c->access_cnt;
    return c->hit_lat + miss_rate * c->miss_pen;
}

uint64_t cache_tmt(const ChampCache *c) {
    return c->miss_cnt * c->miss_pen;
}

void cache_report(const ChampCache *c) {
    double mr = c->access_cnt ? (double)c->miss_cnt / c->access_cnt : 0.0;
    fprintf(stderr,
        "========================================\n"
        "ChampCache Results\n"
        "  Config:  %d-way, %d sets, %dB blocks\n"
        "  Latency: hit=%d miss_penalty=%d\n"
        "----------------------------------------\n"
        "  accesses:  %" PRIu64 "\n"
        "  hits:      %" PRIu64 "\n"
        "  misses:    %" PRIu64 "\n"
        "  miss rate: %.4f%%\n"
        "  TMT:       %" PRIu64 " cycles\n"
        "  AMAT:      %.4f cycles\n"
        "========================================\n",
        c->nways, c->nsets, c->block_size,
        c->hit_lat, c->miss_pen,
        c->access_cnt, c->hit_cnt, c->miss_cnt,
        mr * 100.0,
        cache_tmt(c),
        cache_amat(c));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, " %s <trace_file> [nways=%d] [nsets=%d] [blocksize=%d] [hit_lat=%d] [miss_pen=%d]\n"
                "  RTL 2KB default: 1-way 64-set 32B\n"
                "  RTL 1KB:         1-way 32-set 32B\n"
                "  RTL 512B:        1-way 16-set 32B\n",
                argv[0], 1, 64, 32, 2, 50);
        return 1;
    }

    const char *fname    = argv[1];
    int nways     = argc > 2 ? atoi(argv[2]) : 1;
    int nsets     = argc > 3 ? atoi(argv[3]) : 64;
    int blk       = argc > 4 ? atoi(argv[4]) : 32;
    int hit_lat   = argc > 5 ? atoi(argv[5]) : 2;
    int miss_pen  = argc > 6 ? atoi(argv[6]) : 50;

    ChampCache cache;
    cache_init(&cache, nways, nsets, blk, hit_lat, miss_pen);

    FILE *fp = fopen(fname, "r");
    if (!fp) { perror(fname); return 1; }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        uint32_t addr;
        /* 自动识别 NPC itrace 格式: [ITRACE] pc: 0x<hex> | ... */
        if (strncmp(line, "[ITRACE]", 8) == 0) {
            char *pc_str = strstr(line, "pc: 0x");
            if (!pc_str) { pc_str = strstr(line, "pc: "); }
            if (!pc_str) continue;
            pc_str += (pc_str[4] == '0' && pc_str[5] == 'x') ? 6 : 4;
            addr = (uint32_t)strtoul(pc_str, NULL, 16);
        } else {
            addr = (uint32_t)strtoul(line, NULL, 16);
        }
        cache_access(&cache, addr);
    }
    fclose(fp);

    /* stdout: 仅 AMAT 数值 (方便脚本 pipe) */
    printf("%.4f\n", cache_amat(&cache));
    fflush(stdout);
    /* stderr: 详细报告 */
    cache_report(&cache);

    cache_free(&cache);
    return 0;
}
