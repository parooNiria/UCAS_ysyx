// ===========================================================================
// branchsim — Branch Predictor Simulator
//
// Reads an itrace log and evaluates multiple branch prediction algorithms.
// Extracts branch instructions and determines taken/not-taken from the PC
// sequence (next PC != current PC + 4 → taken).
//
// Predictors evaluated:
//   1. Always Not-Taken     (static, current pipeline default)
//   2. Always Taken         (static)
//   3. BTFN                 (static: backward-taken, forward-not-taken)
//   4. 1-bit BHT            (dynamic: last-outcome per branch)
//   5. 2-bit BHT            (dynamic: 2-bit saturating counter)
//   6. Gshare               (dynamic: XOR of PC and global history)
//
// Usage:
//   branchsim <itrace_file>
//   branchsim <itrace_file> --json   (machine-readable output)
// ===========================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define BHT_ENTRIES      256   // Must be power of 2
#define BHT_INDEX_MASK   0xFF  // (BHT_ENTRIES - 1)

#define PHT_ENTRIES      512   // Gshare pattern history table entries
#define PHT_INDEX_MASK   0x1FF
#define GHR_BITS         9     // Global history register width

// ---------------------------------------------------------------------------
// Branch types
// ---------------------------------------------------------------------------
typedef enum {
    BR_NONE     = 0,   // not a branch
    BR_COND     = 1,   // conditional branch (B-type)
    BR_JAL      = 2,   // unconditional jump (JAL)
    BR_JALR     = 3    // unconditional jump (JALR)
} BrType;

// ---------------------------------------------------------------------------
// Predictor state
// ---------------------------------------------------------------------------

// 1-bit BHT: one bit per entry
static uint8_t  bht_1bit[BHT_ENTRIES];

// 2-bit BHT: two bits per entry (00=SNT, 01=WNT, 10=WT, 11=ST)
static uint8_t  bht_2bit[BHT_ENTRIES];

// Gshare: 2-bit counters indexed by (PC ^ GHR)
static uint8_t  gshare_pht[PHT_ENTRIES];
static uint32_t gshare_ghr;   // global history register

// ---------------------------------------------------------------------------
// Statistics per predictor
// ---------------------------------------------------------------------------
typedef struct {
    const char *name;
    uint64_t    total;         // total predictions made
    uint64_t    correct;       // correct predictions
    uint64_t    taken_pred;    // predicted-taken count
    uint64_t    ntaken_pred;   // predicted-not-taken count
} PredStats;

static PredStats stats[6];
static uint64_t  total_branches;
static uint64_t  total_cond;
static uint64_t  total_jal;
static uint64_t  total_jalr;
static uint64_t  total_cond_taken;
static uint64_t  total_cond_ntaken;

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
static void init_predictors(void) {
    // Initialize 2-bit counters to "Weakly Not-Taken" (01) for a neutral start
    for (int i = 0; i < BHT_ENTRIES; i++) {
        bht_1bit[i]  = 0;
        bht_2bit[i]  = 1;  // WNT
    }
    for (int i = 0; i < PHT_ENTRIES; i++) {
        gshare_pht[i] = 1; // WNT
    }
    gshare_ghr = 0;

    memset(stats, 0, sizeof(stats));

    stats[0].name = "Always Not-Taken";
    stats[1].name = "Always Taken";
    stats[2].name = "BTFN (Back-T Forward-NT)";
    stats[3].name = "1-bit BHT";
    stats[4].name = "2-bit BHT";
    stats[5].name = "Gshare";
    total_branches  = 0;
    total_cond      = 0;
    total_jal       = 0;
    total_jalr      = 0;
    total_cond_taken  = 0;
    total_cond_ntaken = 0;
}

// ---------------------------------------------------------------------------
// Instruction decode helpers (opcode-based, robust against capstone style)
// ---------------------------------------------------------------------------

static BrType classify_branch(uint32_t inst) {
    uint32_t opcode = inst & 0x7F;

    if (opcode == 0x63) return BR_COND;   // B-type: beq, bne, blt, bge, bltu, bgeu
    if (opcode == 0x6F) return BR_JAL;    // J-type: jal
    if (opcode == 0x67) return BR_JALR;   // I-type: jalr

    return BR_NONE;
}

// Extract B-immediate for B-type branches
static int32_t b_imm(uint32_t inst) {
    int32_t imm = 0;
    imm |= ((inst >>  7) & 0x1)  << 11;  // imm[11]
    imm |= ((inst >> 25) & 0x3F) << 5;   // imm[10:5]
    imm |= ((inst >>  8) & 0xF)  << 1;   // imm[4:1]
    imm |= ((inst >> 31) & 0x1)  << 12;  // imm[12]
    // Sign-extend from bit 12
    if (imm & 0x1000) imm |= 0xFFFFE000;
    return imm;
}

// Extract J-immediate for JAL
static int32_t j_imm(uint32_t inst) {
    int32_t imm = 0;
    imm |= ((inst >> 21) & 0x3FF) << 1;  // imm[10:1]
    imm |= ((inst >> 20) & 0x1)   << 11; // imm[11]
    imm |= ((inst >> 12) & 0xFF)  << 12; // imm[19:12]
    imm |= ((inst >> 31) & 0x1)   << 20; // imm[20]
    // Sign-extend from bit 20
    if (imm & 0x100000) imm |= 0xFFE00000;
    return imm;
}

// ---------------------------------------------------------------------------
// Prediction algorithms
// ---------------------------------------------------------------------------

// 0: Always Not-Taken
static int predict_always_nt(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)pc; (void)inst; (void)target;
    return 0;
}

// 1: Always Taken
static int predict_always_t(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)pc; (void)inst; (void)target;
    return 1;
}

// 2: BTFN — backward branches (target < pc) predicted taken; forward → not-taken
static int predict_btfn(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)inst;
    // For B-type: target is computed as pc + imm
    // Backward: target < pc → taken; Forward: target >= pc → not-taken
    return (target < pc) ? 1 : 0;
}

// 3: 1-bit BHT — last outcome per branch
static int predict_1bit(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)inst; (void)target;
    uint32_t idx = (pc >> 2) & BHT_INDEX_MASK;
    return bht_1bit[idx];
}

// 4: 2-bit BHT — 2-bit saturating counter
static int predict_2bit(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)inst; (void)target;
    uint32_t idx = (pc >> 2) & BHT_INDEX_MASK;
    // Predict taken if counter >= 2 (states 10=WT, 11=ST)
    return (bht_2bit[idx] >= 2) ? 1 : 0;
}

// 5: Gshare — (PC XOR GHR) indexes 2-bit counters
static int predict_gshare(uint32_t pc, uint32_t inst, uint32_t target) {
    (void)inst; (void)target;
    uint32_t idx = ((pc >> 2) ^ gshare_ghr) & PHT_INDEX_MASK;
    return (gshare_pht[idx] >= 2) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Update predictors with actual outcome
// ---------------------------------------------------------------------------

static void update_1bit(uint32_t pc, int taken) {
    uint32_t idx = (pc >> 2) & BHT_INDEX_MASK;
    bht_1bit[idx] = (uint8_t)taken;
}

static void update_2bit(uint32_t pc, int taken) {
    uint32_t idx = (pc >> 2) & BHT_INDEX_MASK;
    if (taken) {
        if (bht_2bit[idx] < 3) bht_2bit[idx]++;
    } else {
        if (bht_2bit[idx] > 0) bht_2bit[idx]--;
    }
}

static void update_gshare(uint32_t pc, int taken) {
    uint32_t idx = ((pc >> 2) ^ gshare_ghr) & PHT_INDEX_MASK;
    if (taken) {
        if (gshare_pht[idx] < 3) gshare_pht[idx]++;
    } else {
        if (gshare_pht[idx] > 0) gshare_pht[idx]--;
    }
    // Shift outcome into GHR
    gshare_ghr = ((gshare_ghr << 1) | (uint32_t)(taken ? 1 : 0)) & ((1u << GHR_BITS) - 1);
}

// ---------------------------------------------------------------------------
// Run all predictions for one branch, update stats
// ---------------------------------------------------------------------------
static void eval_branch(uint32_t pc, uint32_t inst, uint32_t target,
                         int actually_taken, BrType br_type) {

    total_branches++;
    if (br_type == BR_COND) {
        total_cond++;
        if (actually_taken) total_cond_taken++;
        else                total_cond_ntaken++;
    } else if (br_type == BR_JAL) {
        total_jal++;
    } else if (br_type == BR_JALR) {
        total_jalr++;
    }

    // Only evaluate prediction for conditional branches.
    // JAL and JALR are always-taken (100% predictable).
    if (br_type != BR_COND) return;

    // Array of prediction functions
    typedef int (*pred_fn)(uint32_t, uint32_t, uint32_t);
    pred_fn predictors[] = {
        predict_always_nt,   // 0
        predict_always_t,    // 1
        predict_btfn,        // 2
        predict_1bit,        // 3
        predict_2bit,        // 4
        predict_gshare       // 5
    };

    for (int i = 0; i < 6; i++) {
        int pred = predictors[i](pc, inst, target);
        stats[i].total++;
        if (pred == actually_taken) stats[i].correct++;
        if (pred) stats[i].taken_pred++;
        else      stats[i].ntaken_pred++;
    }

    // Update dynamic predictors (after prediction)
    update_1bit(pc, actually_taken);
    update_2bit(pc, actually_taken);
    update_gshare(pc, actually_taken);
}

// ---------------------------------------------------------------------------
// Parse itrace line → pc, inst
// Format: [ITRACE] pc: 0x<hex> | inst: 0x<hex> | <mnemonic> <operands>
// ---------------------------------------------------------------------------
static int parse_itrace_line(const char *line, uint32_t *pc, uint32_t *inst) {
    // Find "pc: 0x"
    const char *pc_pos = strstr(line, "pc: 0x");
    if (!pc_pos) {
        pc_pos = strstr(line, "pc: ");
        if (!pc_pos) return -1;
        *pc = (uint32_t)strtoul(pc_pos + 4, NULL, 16);
    } else {
        *pc = (uint32_t)strtoul(pc_pos + 6, NULL, 16);
    }

    // Find "inst: 0x"
    const char *inst_pos = strstr(line, "inst: 0x");
    if (!inst_pos) {
        inst_pos = strstr(line, "inst: ");
        if (!inst_pos) return -1;
        *inst = (uint32_t)strtoul(inst_pos + 6, NULL, 16);
    } else {
        *inst = (uint32_t)strtoul(inst_pos + 8, NULL, 16);
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------
static void print_report(int json) {
    if (json) {
        printf("{\n");
        printf("  \"total_branches\": %" PRIu64 ",\n", total_branches);
        printf("  \"conditional\": {\n");
        printf("    \"total\": %" PRIu64 ",\n", total_cond);
        printf("    \"taken\": %" PRIu64 ",\n", total_cond_taken);
        printf("    \"not_taken\": %" PRIu64 "\n", total_cond_ntaken);
        printf("  },\n");
        printf("  \"unconditional\": {\n");
        printf("    \"jal\": %" PRIu64 ",\n", total_jal);
        printf("    \"jalr\": %" PRIu64 "\n", total_jalr);
        printf("  },\n");
        printf("  \"predictors\": [\n");
        for (int i = 0; i < 6; i++) {
            double acc = stats[i].total ? (double)stats[i].correct / stats[i].total * 100.0 : 0.0;
            printf("    {\n");
            printf("      \"name\": \"%s\",\n", stats[i].name);
            printf("      \"total\": %" PRIu64 ",\n", stats[i].total);
            printf("      \"correct\": %" PRIu64 ",\n", stats[i].correct);
            printf("      \"accuracy\": %.4f\n", acc);
            printf("    }%s\n", (i < 5) ? "," : "");
        }
        printf("  ]\n");
        printf("}\n");
    } else {
        fprintf(stderr,
            "============================================\n"
            "BranchSim — Branch Predictor Evaluation\n"
            "============================================\n"
            "Total branch instructions: %" PRIu64 "\n"
            "  Conditional (B-type):   %" PRIu64 "  (taken: %" PRIu64 ", not-taken: %" PRIu64 ")\n"
            "  Unconditional JAL:      %" PRIu64 "\n"
            "  Unconditional JALR:     %" PRIu64 "\n"
            "--------------------------------------------\n",
            total_branches,
            total_cond, total_cond_taken, total_cond_ntaken,
            total_jal, total_jalr);

        fprintf(stderr,
            "%-24s %8s %8s %10s\n",
            "Predictor", "Total", "Correct", "Accuracy");
        fprintf(stderr,
            "------------------------ -------- -------- ----------\n");

        for (int i = 0; i < 6; i++) {
            double acc = stats[i].total ? (double)stats[i].correct / stats[i].total * 100.0 : 0.0;
            fprintf(stderr,
                "%-24s %8" PRIu64 " %8" PRIu64 " %9.2f%%\n",
                stats[i].name, stats[i].total, stats[i].correct, acc);
        }
        fprintf(stderr,
            "============================================\n");
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <itrace_file> [--json]\n"
            "Reads an itrace log, extracts branch instructions, and evaluates\n"
            "multiple branch prediction algorithms.\n", argv[0]);
        return 1;
    }

    const char *fname = argv[1];
    int json_output = (argc > 2 && strcmp(argv[2], "--json") == 0);

    FILE *fp = fopen(fname, "r");
    if (!fp) { perror(fname); return 1; }

    init_predictors();

    // Read itrace lines. We need pairs of consecutive lines to determine
    // whether a branch was taken (next PC != current PC + 4 → taken).
    char line_cur[512], line_next[512];
    int have_cur = 0;
    uint64_t line_no = 0;

    // Read first line
    if (fgets(line_cur, sizeof(line_cur), fp)) {
        have_cur = 1;
        line_no = 1;
    }

    while (have_cur && fgets(line_next, sizeof(line_next), fp)) {
        line_no++;

        // Skip comments/empty lines
        if (line_cur[0] == '#' || line_cur[0] == '\n' || line_cur[0] == '\r') {
            strcpy(line_cur, line_next);
            continue;
        }

        uint32_t pc_cur, inst_cur;
        uint32_t pc_next, inst_next;

        if (parse_itrace_line(line_cur, &pc_cur, &inst_cur) != 0) {
            strcpy(line_cur, line_next);
            continue;
        }

        BrType br_type = classify_branch(inst_cur);

        if (br_type != BR_NONE) {
            if (parse_itrace_line(line_next, &pc_next, &inst_next) == 0) {
                // Determine taken/not-taken from the PC sequence
                uint32_t expected_next = pc_cur + 4;
                int actually_taken = (pc_next != expected_next) ? 1 : 0;

                // Compute target
                uint32_t target = 0;
                if (br_type == BR_COND) {
                    target = pc_cur + (uint32_t)b_imm(inst_cur);
                } else if (br_type == BR_JAL) {
                    target = pc_cur + (uint32_t)j_imm(inst_cur);
                }
                // JALR target is register-based, unknown from static trace

                eval_branch(pc_cur, inst_cur, target, actually_taken, br_type);
            }
        }

        // Advance
        strcpy(line_cur, line_next);
    }
    fclose(fp);

    print_report(json_output);
    return 0;
}
