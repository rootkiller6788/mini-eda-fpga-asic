#include "coverage_mdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
   Example: Coverage Model — Code + Functional Coverage
   example_coverage.c
   ================================================================ */

/* ----- Custom bin function ----- */
int power_of_two_bins(uint64_t value, void* ctx) {
    (void)ctx;
    if (value == 0) return 0;
    int bin = 0;
    uint64_t v = value;
    while (v > 1) { v >>= 1; bin++; }
    if (bin >= 16) bin = 15;
    return bin;
}

/* ----- Covergroup sample function ----- */
void my_cg_sample(cov_covergroup_t* cg, void* ctx) {
    (void)ctx;
    printf("  [%s] sample triggered\n", cg->name);
}

/* ----- Simulate random ALU operations ----- */
typedef struct {
    uint64_t a, b, result;
    int      opcode;
} alu_context_t;

int main(void) {
    printf("========================================\n");
    printf(" Coverage Model Example\n");
    printf("========================================\n\n");

    /* ====== Code Coverage ====== */
    printf("--- Code Coverage ---\n");
    cov_code_db_t* code_db = cov_code_db_create();

    /* Simulate lines in a hypothetical ALU module */
    const char* files[] = { "alu.v", "alu_ctrl.v", "alu_datapath.v" };
    for (int f = 0; f < 3; f++) {
        for (int line = 1; line <= 20; line++) {
            char name[128];
            snprintf(name, sizeof(name), "%s:line%d", files[f], line);
            cov_code_item_t* item = cov_code_item_create(name, COV_CODE_LINE, files[f], line);
            cov_code_db_add(code_db, item);

            /* Also add branch items */
            if (line % 5 == 0) {
                snprintf(name, sizeof(name), "%s:branch%d", files[f], line);
                cov_code_item_t* branch = cov_code_item_create(name, COV_CODE_BRANCH, files[f], line);
                branch->total_bins = 2;
                cov_code_db_add(code_db, branch);
            }
        }
    }

    /* Hit some items to simulate coverage */
    uint32_t seed = 42;
    for (int i = 0; i < 100; i++) {
        seed = seed * 1664525u + 1013904223u;
        int idx = (int)(seed % (uint32_t)code_db->count);
        cov_code_item_hit(code_db->items[idx]);
    }
    cov_code_db_update_metrics(code_db);
    cov_code_db_report(code_db, stdout);

    /* ====== Functional Coverage ====== */
    printf("\n--- Functional Coverage ---\n");

    /* Covergroup: ALU operations */
    cov_covergroup_t* cg_alu = cov_covergroup_create("cg_alu_operations");
    cg_alu->goal = 95.0;
    cov_covergroup_set_sample_fn(cg_alu, my_cg_sample, NULL);

    /* Coverpoint 1: opcode (8 bins for 8 ALU ops) */
    cov_coverpoint_t* cp_opcode = cov_coverpoint_create("cp_opcode", 8);
    cov_coverpoint_set_range(cp_opcode, 0.0, 7.0);
    cov_coverpoint_set_at_least(cp_opcode, 1);
    cov_coverpoint_set_goal(cp_opcode, 90.0);
    cov_covergroup_add_coverpoint(cg_alu, cp_opcode);

    /* Coverpoint 2: operand A value (auto-binned into 4 powers of 2) */
    cov_coverpoint_t* cp_a_val = cov_coverpoint_create("cp_operand_a", 16);
    cov_coverpoint_set_custom_bin_func(cp_a_val, power_of_two_bins, NULL);
    cov_coverpoint_set_goal(cp_a_val, 50.0);
    cov_covergroup_add_coverpoint(cg_alu, cp_a_val);

    /* Coverpoint 3: result zero check */
    cov_coverpoint_t* cp_zero = cov_coverpoint_create("cp_result_zero", 2);
    cov_coverpoint_set_range(cp_zero, 0.0, 1.0);
    cov_covergroup_add_coverpoint(cg_alu, cp_zero);

    /* Cross: opcode x zero result */
    cov_cross_t* cross_op_zero = cov_cross_create("cross_opcode_zero");
    cov_cross_add_coverpoint(cross_op_zero, cp_opcode);
    cov_cross_add_coverpoint(cross_op_zero, cp_zero);
    cov_cross_build(cross_op_zero);
    cov_covergroup_add_cross(cg_alu, cross_op_zero);

    /* ----- Simulate sampling ----- */
    alu_context_t alu;
    uint64_t test_vectors[][3] = {
        {0, 0, 0},           /* NOP: 0+0=0 */
        {5, 3, 8},           /* ADD */
        {15, 8, 7},          /* SUB */
        {3, 3, 9},           /* MUL */
        {16, 4, 4},          /* DIV */
        {0xFF, 0x00, 0xFF},  /* AND */
        {0xF0, 0x0F, 0xFF},  /* OR  */
        {0xAA, 0x55, 0xFF},  /* XOR */
        {1, 0, 1},           /* SHL */
        {8, 0, 4},           /* SHR */
        {42, 1, 42},         /* pass through */
        {100, 200, 0},       /* result zero */
        {7, 7, 49},          /* MUL again */
        {0, 15, 0},          /* zero result */
        {12, 3, 15},         /* ADD */
        {8, 2, 6},           /* SUB */
        {2, 8, 16},          /* MUL */
        {32, 8, 4},          /* DIV */
        {0, 0, 0},           /* zero again */
        {9, 9, 0},           /* zero */
    };
    int num_vectors = (int)(sizeof(test_vectors) / sizeof(test_vectors[0]));

    printf("Simulating %d ALU transactions:\n", num_vectors);
    for (int i = 0; i < num_vectors; i++) {
        alu.a      = test_vectors[i][0];
        alu.b      = test_vectors[i][1];
        alu.result = test_vectors[i][2];
        alu.opcode = (i % 8);

        /* Sample coverpoints */
        cov_coverpoint_sample(cp_opcode, (uint64_t)alu.opcode);
        cov_coverpoint_sample(cp_a_val, alu.a);
        cov_coverpoint_sample(cp_zero, (alu.result == 0) ? 1u : 0u);

        /* Covergroup sample */
        cov_covergroup_sample(cg_alu);
    }

    /* Report */
    cov_covergroup_report(cg_alu, stdout);

    /* ====== Coverage Closure ====== */
    printf("\n--- Coverage Closure ---\n");
    cov_closure_t* closure = cov_closure_create();
    cov_closure_add_group(closure, cg_alu);
    cov_closure_set_code_db(closure, code_db);
    cov_closure_set_goals(closure, 95.0, 90.0);
    cov_closure_report(closure, stdout);

    /* ====== Waivers ====== */
    printf("\n--- Waivers ---\n");
    cov_waiver_t* w = cov_waiver_create("cp_opcode", "Bin 7 is unused in current config", "verification_team");
    w->waived_bins[0] = 7;
    w->waived_count = 1;
    cov_waiver_apply_to_coverpoint(w, cp_opcode);
    printf("Applied waiver: %s\n", w->reason);

    /* ====== Coverage-Driven Verification Demo ====== */
    printf("\n--- Coverage-Driven Verification ---\n");
    cov_cdv_t* cdv = cov_cdv_create(closure, 12345);
    cov_cdv_iterate(cdv);
    cov_cdv_iterate(cdv);
    cov_cdv_report(cdv, stdout);
    printf("Converged: %s\n", cov_cdv_has_converged(cdv) ? "yes" : "no");

    /* ====== Export ====== */
    printf("\n--- Export ---\n");
    cov_export_xml(closure, "coverage_report.xml");
    cov_export_html(closure, "./cov_html/");
    cov_print_histogram(cg_alu, stdout);

    /* ====== Cleanup ====== */
    cov_cdv_destroy(cdv);
    cov_waiver_destroy(w);
    cov_closure_destroy(closure);
    /* cp/cg/cross are owned by cg_alu which is owned by closure */

    printf("\nDone.\n");
    return 0;
}
