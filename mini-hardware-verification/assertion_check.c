#include "assertion_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ================================================================
   Assertion Check Implementation
   assertion_check.c
   ================================================================ */

/* ----- SVA Expression ----- */
sva_expr_t* sva_expr_create(const char* name, sva_eval_fn eval, void* ctx) {
    sva_expr_t* e = calloc(1, sizeof(sva_expr_t));
    if (!e) return NULL;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->eval = eval;
    e->ctx  = ctx;
    return e;
}

void sva_expr_destroy(sva_expr_t* expr) { free(expr); }
bool sva_expr_eval(sva_expr_t* expr, void* ctx) {
    if (!expr || !expr->eval) return false;
    expr->evaluation_count++;
    expr->last_value = expr->eval(expr, ctx);
    return expr->last_value;
}

/* ----- SVA Sequence Step ----- */
sva_sequence_step_t* sva_seq_step_create(sva_expr_t* expr, sequence_op_t op, int delay) {
    sva_sequence_step_t* s = calloc(1, sizeof(sva_sequence_step_t));
    if (!s) return NULL;
    s->match_expr  = expr;
    s->op          = op;
    s->cycle_delay = delay;
    s->cycle_min   = delay;
    s->cycle_max   = delay;
    return s;
}

void sva_seq_step_destroy(sva_sequence_step_t* step) { free(step); }

sva_sequence_step_t* sva_seq_chain(sva_sequence_step_t** steps, int count) {
    if (!steps || count < 1) return NULL;
    for (int i = 0; i < count - 1; i++) {
        steps[i]->next = steps[i + 1];
    }
    steps[count - 1]->next = NULL;
    return steps[0];
}

/* ----- Assertion Clock ----- */
assertion_clock_t* assertion_clock_create(const char* name, assert_clock_edge_fn posedge_fn) {
    assertion_clock_t* clk = calloc(1, sizeof(assertion_clock_t));
    if (!clk) return NULL;
    strncpy(clk->name, name, sizeof(clk->name) - 1);
    clk->posedge_detect = posedge_fn;
    return clk;
}

void assertion_clock_destroy(assertion_clock_t* clk) { free(clk); }

bool assertion_clock_tick(assertion_clock_t* clk) {
    if (!clk) return false;
    clk->tick_count++;
    if (clk->posedge_detect) {
        clk->is_posedge = clk->posedge_detect(clk);
    } else {
        clk->is_posedge = (clk->tick_count % 2) == 0;
    }
    return clk->is_posedge;
}

/* ----- Assertion Definition ----- */
assertion_def_t* assertion_create(const char* name, assertion_type_t type) {
    assertion_def_t* a = calloc(1, sizeof(assertion_def_t));
    if (!a) return NULL;
    strncpy(a->name, name, sizeof(a->name) - 1);
    a->type     = type;
    a->severity = ASSERT_SEVERITY_ERROR;
    a->state    = ASSERT_STATE_ACTIVE;
    return a;
}

void assertion_destroy(assertion_def_t* a) { free(a); }

void assertion_set_clock(assertion_def_t* a, assertion_clock_t* clk) {
    if (a) a->clock = clk;
}
void assertion_set_severity(assertion_def_t* a, assertion_severity_t sev) {
    if (a) a->severity = sev;
}
void assertion_set_message(assertion_def_t* a, const char* msg) {
    if (a && msg) strncpy(a->message, msg, sizeof(a->message) - 1);
}
void assertion_set_action(assertion_def_t* a, const char* action) {
    if (a && action) strncpy(a->action_block, action, sizeof(a->action_block) - 1);
}

/* ----- Immediate Assertion ----- */
bool assert_immediate(assertion_def_t* a, bool condition) {
    if (!a || a->is_disabled) return condition;
    a->attempt_count++;
    if (!condition) {
        a->fail_count++;
        a->state = ASSERT_STATE_FAIL;
        return false;
    }
    a->pass_count++;
    a->state = ASSERT_STATE_PASS;
    return true;
}

/* ----- Sequence Step Evaluation for Concurrent ----- */
bool assert_sequence_step_eval(sva_sequence_step_t* step, void* ctx, int* cycle) {
    if (!step) return true;
    int delay = step->cycle_delay;
    *cycle += delay;
    if (step->match_expr) {
        return sva_expr_eval(step->match_expr, ctx);
    }
    return true;
}

/* ----- Concurrent Assertion Evaluation ----- */
bool assert_concurrent_eval(assertion_def_t* a, void* ctx) {
    if (!a || a->is_disabled) return true;
    a->attempt_count++;
    int cycle = 0;
    bool antecedent_pass = assert_sequence_step_eval(a->antecedent, ctx, &cycle);
    if (!antecedent_pass) {
        a->vacuous_pass_count++;
        a->state = ASSERT_STATE_VACUOUS;
        return true; /* vacuous pass */
    }
    bool consequent_pass = assert_sequence_step_eval(a->consequent, ctx, &cycle);
    if (consequent_pass) {
        a->pass_count++;
        a->state = ASSERT_STATE_PASS;
        return true;
    }
    a->fail_count++;
    a->state = ASSERT_STATE_FAIL;
    return false;
}

/* ----- Tick-based concurrent assertion ----- */
bool assert_concurrent_tick(assertion_def_t* a, void* ctx) {
    if (!a || a->is_disabled || !a->clock) return true;
    if (!assertion_clock_tick(a->clock)) return true;
    return assert_concurrent_eval(a, ctx);
}

/* ----- PSL ----- */
psl_property_t* psl_property_create(const char* name, psl_op_t op) {
    psl_property_t* p = calloc(1, sizeof(psl_property_t));
    if (!p) return NULL;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->op = op;
    return p;
}

void psl_property_destroy(psl_property_t* prop) { free(prop); }

bool psl_property_check(psl_property_t* prop, void* ctx) {
    if (!prop) return true;
    prop->check_count++;
    bool left_val  = prop->left  ? sva_expr_eval(prop->left, ctx)  : true;
    bool right_val = prop->right ? sva_expr_eval(prop->right, ctx) : true;
    bool result = false;
    switch (prop->op) {
        case PSL_ALWAYS:       result = left_val; break;
        case PSL_NEVER:        result = !left_val; break;
        case PSL_NEXT:         result = right_val; break;
        case PSL_EVENTUALLY:   result = left_val || right_val; break;
        case PSL_UNTIL:        result = left_val ? true : right_val; break;
        case PSL_UNTIL2:       result = (left_val && right_val); break;
        default:               result = left_val; break;
    }
    if (result) prop->hold_count++;
    return result;
}

/* ----- Assertion Coverage ----- */
assertion_coverage_t* assertion_coverage_create(const char* name) {
    assertion_coverage_t* cov = calloc(1, sizeof(assertion_coverage_t));
    if (!cov) return NULL;
    strncpy(cov->name, name, sizeof(cov->name) - 1);
    return cov;
}

void assertion_coverage_destroy(assertion_coverage_t* cov) { free(cov); }

void assertion_coverage_record(assertion_coverage_t* cov, assertion_def_t* a) {
    if (!cov || !a) return;
    cov->attempt_count = a->attempt_count;
    cov->success_count = a->pass_count;
    cov->failure_count = a->fail_count;
    cov->vacuous_count = a->vacuous_pass_count;
    cov->active_count  = a->pass_count + a->fail_count;
    if (cov->attempt_count > 0) {
        cov->pass_rate = (double)cov->success_count / (double)cov->attempt_count * 100.0;
        cov->activation_rate = (double)cov->active_count / (double)cov->attempt_count * 100.0;
    }
    cov->covered = (cov->success_count > 0);
}

void assertion_coverage_report(const assertion_coverage_t* cov) {
    if (!cov) return;
    printf("[AssertCov %s] attempts=%llu pass=%llu fail=%llu vacuous=%llu rate=%.1f%% act=%.1f%% %s\n",
        cov->name,
        (unsigned long long)cov->attempt_count,
        (unsigned long long)cov->success_count,
        (unsigned long long)cov->failure_count,
        (unsigned long long)cov->vacuous_count,
        cov->pass_rate, cov->activation_rate,
        cov->covered ? "[COVERED]" : "[NOT COVERED]");
}

/* ----- Assertion Bank ----- */
assertion_bank_t* assertion_bank_create(void) {
    return calloc(1, sizeof(assertion_bank_t));
}

void assertion_bank_destroy(assertion_bank_t* bank) {
    if (!bank) return;
    for (int i = 0; i < bank->count; i++)
        assertion_destroy(bank->assertions[i]);
    free(bank);
}

void assertion_bank_add(assertion_bank_t* bank, assertion_def_t* a) {
    if (!bank || !a || bank->count >= ASSERTION_BANK_MAX) return;
    bank->assertions[bank->count++] = a;
}

void assertion_bank_add_coverage(assertion_bank_t* bank, assertion_coverage_t* cov) {
    if (!bank || !cov || bank->cov_count >= ASSERTION_BANK_MAX) return;
    bank->coverages[bank->cov_count++] = cov;
}

void assertion_bank_set_default_clock(assertion_bank_t* bank, assertion_clock_t* clk) {
    if (bank) bank->default_clock = clk;
}

bool assertion_bank_check_all(assertion_bank_t* bank, void* ctx) {
    if (!bank) return false;
    bool all_pass = true;
    for (int i = 0; i < bank->count; i++) {
        assertion_def_t* a = bank->assertions[i];
        a->attempt_count++;
        bool pass = false;
        if (a->type == ASSERT_IMMEDIATE && a->check) {
            pass = a->check(a, ctx);
        } else if (a->type == ASSERT_CONCURRENT) {
            pass = assert_concurrent_tick(a, ctx);
        }
        if (pass) { a->pass_count++; bank->total_pass++; }
        else      { a->fail_count++; bank->total_fail++; all_pass = false; }
        bank->total_attempt++;
    }
    for (int i = 0; i < bank->cov_count; i++) {
        assertion_coverage_record(bank->coverages[i], bank->assertions[i]);
    }
    return all_pass;
}

void assertion_bank_report(const assertion_bank_t* bank) {
    if (!bank) return;
    printf("\n=== Assertion Bank Report ===\n");
    printf("Total: attempts=%llu pass=%llu fail=%llu\n",
        (unsigned long long)bank->total_attempt,
        (unsigned long long)bank->total_pass,
        (unsigned long long)bank->total_fail);
    for (int i = 0; i < bank->count; i++) {
        assertion_def_t* a = bank->assertions[i];
        printf("  [%s] %s: attempts=%llu pass=%llu fail=%llu vac=%llu\n",
            a->state == ASSERT_STATE_PASS ? "PASS" :
            a->state == ASSERT_STATE_FAIL ? "FAIL" : "ACTIVE",
            a->name,
            (unsigned long long)a->attempt_count,
            (unsigned long long)a->pass_count,
            (unsigned long long)a->fail_count,
            (unsigned long long)a->vacuous_pass_count);
    }
    for (int i = 0; i < bank->cov_count; i++)
        assertion_coverage_report(bank->coverages[i]);
}

/* ----- Utility ----- */
void assertion_report_failure(const assertion_def_t* a, const char* file, int line, const char* msg) {
    fprintf(stderr, "[%s] Assertion '%s' FAILED at %s:%d — %s\n",
        a->severity == ASSERT_SEVERITY_FATAL ? "FATAL" :
        a->severity == ASSERT_SEVERITY_ERROR ? "ERROR" : "WARNING",
        a->name, file, line, msg);
}

void assertion_print_state(const assertion_def_t* a, FILE* fp) {
    if (!a || !fp) return;
    fprintf(fp, "[Assertion %s] state=%d pass=%llu fail=%llu\n",
        a->name, a->state,
        (unsigned long long)a->pass_count,
        (unsigned long long)a->fail_count);
}

void assertion_log_to_file(const assertion_def_t* a, const char* filename) {
    FILE* f = fopen(filename, "a");
    if (!f) return;
    assertion_print_state(a, f);
    fclose(f);
}

/* ----- Formal with Assertions ----- */
bool assertion_to_formal(assertion_def_t* a, char* output_file, const char* lang) {
    if (!a || !output_file || !lang) return false;
    FILE* f = fopen(output_file, "w");
    if (!f) return false;
    if (strcmp(lang, "smt2") == 0) {
        fprintf(f, "(declare-const %s Bool)\n", a->name);
        fprintf(f, "(assert %s)\n", a->name);
        fprintf(f, "(check-sat)\n");
    } else if (strcmp(lang, "sva") == 0) {
        fprintf(f, "// %s\n", a->message);
        fprintf(f, "assert property (@(posedge clk) %s);\n", a->name);
    }
    fclose(f);
    return true;
}

bool assertion_prove(assertion_def_t* a, const char* solver_path, int max_cycles) {
    (void)solver_path; (void)max_cycles;
    if (!a) return false;
    printf("[Prove] Attempting to prove '%s' (max_cycles=%d)\n", a->name, max_cycles);
    /* Stub: would invoke external solver */
    a->is_formal = true;
    return true;
}

bool assertion_get_counterexample(assertion_def_t* a, void** trace, int* trace_len) {
    if (!a || !trace || !trace_len) return false;
    /* Stub: would return trace from formal engine */
    *trace = NULL;
    *trace_len = 3;
    return true;
}
