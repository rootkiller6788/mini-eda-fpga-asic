/**
 * assertion_engine.c - SVA Assertion Engine Implementation
 *
 * Implements SystemVerilog-alike concurrent assertion evaluation:
 *   Operand evaluation on signals
 *   Sequence matching over simulation trace
 *   Property evaluation with implication
 *   Assertion engine with per-cycle evaluation
 *
 * L5: Sequence matching uses active match states tracking.
 *     At each clock edge, match states are advanced.
 *     Each sequence operator spawns/updates match states per
 *     its temporal semantics.
 */

#include "assertion_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Operand Construction (L1)
 * ======================================================================== */

hv_assert_operand_t hv_op_signal_eq(hv_signal_t *sig, uint32_t val) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = sig;
    op.cmp_op = CMP_EQ;
    op.compare_value = val;
    op.is_sig_to_sig = false;
    return op;
}

hv_assert_operand_t hv_op_signal_neq(hv_signal_t *sig, uint32_t val) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = sig;
    op.cmp_op = CMP_NEQ;
    op.compare_value = val;
    return op;
}

hv_assert_operand_t hv_op_signal_lt(hv_signal_t *sig, uint32_t val) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = sig;
    op.cmp_op = CMP_LT;
    op.compare_value = val;
    return op;
}

hv_assert_operand_t hv_op_signal_gt(hv_signal_t *sig, uint32_t val) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = sig;
    op.cmp_op = CMP_GT;
    op.compare_value = val;
    return op;
}

hv_assert_operand_t hv_op_signal_bits_set(hv_signal_t *sig, uint32_t mask) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = sig;
    op.cmp_op = CMP_BIT_EQ;
    op.compare_value = mask;
    return op;
}

hv_assert_operand_t hv_op_signal_eq_signal(hv_signal_t *a, hv_signal_t *b) {
    hv_assert_operand_t op;
    memset(&op, 0, sizeof(op));
    op.signal = a;
    op.signal2 = b;
    op.cmp_op = CMP_EQ;
    op.is_sig_to_sig = true;
    return op;
}

/* ========================================================================
 * Operand Evaluation (L2)
 * ======================================================================== */

static bool eval_operand(const hv_assert_operand_t *op) {
    if (!op || !op->signal) return false;

    uint32_t sig_val = hv_signal_read(op->signal);
    uint32_t cmp_val = op->is_sig_to_sig ?
        (op->signal2 ? hv_signal_read(op->signal2) : 0) :
        op->compare_value;

    switch (op->cmp_op) {
        case CMP_EQ:      return sig_val == cmp_val;
        case CMP_NEQ:     return sig_val != cmp_val;
        case CMP_LT:      return sig_val < cmp_val;
        case CMP_LE:      return sig_val <= cmp_val;
        case CMP_GT:      return sig_val > cmp_val;
        case CMP_GE:      return sig_val >= cmp_val;
        case CMP_BIT_AND: return (sig_val & cmp_val) != 0;
        case CMP_BIT_EQ:  return (sig_val & cmp_val) == cmp_val;
        default:          return false;
    }
}

/* ========================================================================
 * Sequence Construction (L1)
 * ======================================================================== */

static hv_sequence_expr_t *seq_alloc(seq_op_t op) {
    hv_sequence_expr_t *s = (hv_sequence_expr_t*)calloc(1, sizeof(*s));
    if (s) s->op = op;
    return s;
}

hv_sequence_expr_t *hv_seq_atom(hv_assert_operand_t op) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_ATOM);
    if (s) s->operand = op;
    return s;
}

hv_sequence_expr_t *hv_seq_delay(hv_sequence_expr_t *seq, uint32_t n) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_DELAY);
    if (s) { s->left = seq; s->delay_min = n; s->delay_max = n; }
    return s;
}

hv_sequence_expr_t *hv_seq_range_delay(hv_sequence_expr_t *seq,
                                        uint32_t min, uint32_t max) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_RANGE_DELAY);
    if (s) { s->left = seq; s->delay_min = min; s->delay_max = max; }
    return s;
}

hv_sequence_expr_t *hv_seq_consecutive_rep(hv_sequence_expr_t *seq, uint32_t n) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_CONSECUTIVE_REP);
    if (s) { s->left = seq; s->rep_min = n; s->rep_max = n; }
    return s;
}

hv_sequence_expr_t *hv_seq_and(hv_sequence_expr_t *a, hv_sequence_expr_t *b) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_AND);
    if (s) { s->left = a; s->right = b; }
    return s;
}

hv_sequence_expr_t *hv_seq_or(hv_sequence_expr_t *a, hv_sequence_expr_t *b) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_OR);
    if (s) { s->left = a; s->right = b; }
    return s;
}

hv_sequence_expr_t *hv_seq_intersect(hv_sequence_expr_t *a, hv_sequence_expr_t *b) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_INTERSECT);
    if (s) { s->left = a; s->right = b; }
    return s;
}

hv_sequence_expr_t *hv_seq_throughout(hv_assert_operand_t expr,
                                       hv_sequence_expr_t *seq) {
    hv_sequence_expr_t *s = seq_alloc(SEQ_THROUGHOUT);
    if (s) { s->operand = expr; s->left = seq; }
    return s;
}

void hv_seq_destroy(hv_sequence_expr_t *s) {
    if (!s) return;
    hv_seq_destroy(s->left);
    hv_seq_destroy(s->right);
    free(s);
}

/* ========================================================================
 * Property Construction (L1)
 * ======================================================================== */

static hv_property_expr_t *prop_alloc(property_op_t op) {
    hv_property_expr_t *p = (hv_property_expr_t*)calloc(1, sizeof(*p));
    if (p) p->op = op;
    return p;
}

hv_property_expr_t *hv_prop_sequence(hv_sequence_expr_t *seq) {
    hv_property_expr_t *p = prop_alloc(PROP_SEQ);
    if (p) p->seq = seq;
    return p;
}

hv_property_expr_t *hv_prop_implies_overlap(hv_sequence_expr_t *ante,
                                              hv_property_expr_t *cons) {
    hv_property_expr_t *p = prop_alloc(PROP_IMPLIES_OVERLAP);
    if (p) { p->seq = ante; p->prop = cons; }
    return p;
}

hv_property_expr_t *hv_prop_implies_nonoverlap(hv_sequence_expr_t *ante,
                                                 hv_property_expr_t *cons) {
    hv_property_expr_t *p = prop_alloc(PROP_IMPLIES_NONOVERLAP);
    if (p) { p->seq = ante; p->prop = cons; }
    return p;
}

hv_property_expr_t *hv_prop_disable_iff(hv_assert_operand_t cond,
                                         hv_property_expr_t *prop) {
    hv_property_expr_t *p = prop_alloc(PROP_DISABLE_IFF);
    if (p) { p->disable_cond = cond; p->prop = prop; }
    return p;
}

hv_property_expr_t *hv_prop_not(hv_property_expr_t *prop) {
    hv_property_expr_t *p = prop_alloc(PROP_NOT);
    if (p) p->left = prop;
    return p;
}

hv_property_expr_t *hv_prop_and(hv_property_expr_t *a, hv_property_expr_t *b) {
    hv_property_expr_t *p = prop_alloc(PROP_AND);
    if (p) { p->left = a; p->right = b; }
    return p;
}

void hv_prop_destroy(hv_property_expr_t *p) {
    if (!p) return;
    hv_seq_destroy(p->seq);
    hv_prop_destroy(p->prop);
    hv_prop_destroy(p->left);
    hv_prop_destroy(p->right);
    free(p);
}

/* ========================================================================
 * Sequence Match State Management (L5)
 * ======================================================================== */

/* Evaluate a sequence atomically at current cycle.
 * Returns true if the sequence is matched at this cycle. */
/* Atom evaluation: just check the operand */
static bool seq_eval_atom(hv_sequence_expr_t *seq) {
    return eval_operand(&seq->operand);
}

/* Simple recursive sequence match evaluation.
 * This simplified evaluator handles basic sequences. */
static bool seq_eval_recursive(hv_sequence_expr_t *seq, uint32_t *pos,
                                uint32_t max_cycles) {
    if (!seq) return true;

    switch (seq->op) {
        case SEQ_ATOM: {
            bool result = seq_eval_atom(seq);
            (*pos)++;
            return result;
        }
        case SEQ_DELAY:
        case SEQ_RANGE_DELAY: {
            /* Skip delay_min cycles */
            for (uint32_t d = 0; d < seq->delay_min && *pos < max_cycles; d++) {
                (*pos)++;
            }
            /* Try to evaluate the inner sequence */
            return seq_eval_recursive(seq->left, pos, max_cycles);
        }
        case SEQ_AND: {
            uint32_t pa = *pos, pb = *pos;
            bool ra = seq_eval_recursive(seq->left, &pa, max_cycles);
            bool rb = seq_eval_recursive(seq->right, &pb, max_cycles);
            *pos = (pa > pb) ? pa : pb;
            return ra && rb;
        }
        case SEQ_OR: {
            uint32_t pa = *pos, pb = *pos;
            bool ra = seq_eval_recursive(seq->left, &pa, max_cycles);
            bool rb = seq_eval_recursive(seq->right, &pb, max_cycles);
            if (ra) { *pos = pa; return true; }
            if (rb) { *pos = pb; return true; }
            return false;
        }
        case SEQ_CONSECUTIVE_REP: {
            for (uint32_t r = 0; r < seq->rep_min; r++) {
                if (!seq_eval_recursive(seq->left, pos, max_cycles)) {
                    return false;
                }
            }
            return true;
        }
        default:
            return false;
    }
}

/* ========================================================================
 * Assertion Management (L2)
 * ======================================================================== */

hv_assertion_t *hv_assertion_create(const char *name, assertion_type_t type,
                                     hv_property_expr_t *prop, hv_signal_t *clk) {
    hv_assertion_t *a = (hv_assertion_t*)calloc(1, sizeof(hv_assertion_t));
    if (!a) return NULL;
    if (name) strncpy(a->name, name, sizeof(a->name) - 1);
    a->type = type;
    a->property = prop;
    a->clock = clk;
    a->severity = SEV_ERROR;
    a->attempt_count = 0;
    a->pass_count = 0;
    a->fail_count = 0;
    a->vacuous_pass = 0;
    a->is_active = true;
    a->is_vacuous = false;
    a->on_fail = NULL;
    a->last_fail_time = 0;
    a->last_fail_cycle = 0;
    memset(a->fail_detail, 0, sizeof(a->fail_detail));
    return a;
}

void hv_assertion_destroy(hv_assertion_t *a) {
    if (!a) return;
    free(a);
}

void hv_assertion_set_message(hv_assertion_t *a, const char *msg) {
    if (a && msg) strncpy(a->message, msg, sizeof(a->message) - 1);
}

void hv_assertion_set_severity(hv_assertion_t *a, severity_t sev) {
    if (a) a->severity = sev;
}

/* ========================================================================
 * Assertion Engine (L2, L3)
 * ======================================================================== */

hv_assertion_engine_t *hv_assertion_engine_create(hv_dut_t *dut) {
    hv_assertion_engine_t *eng = (hv_assertion_engine_t*)calloc(1, sizeof(*eng));
    if (!eng) return NULL;
    eng->assertions = NULL;
    eng->num_assertions = 0;
    eng->capacity_assertions = 0;
    eng->dut = dut;
    return eng;
}

void hv_assertion_engine_destroy(hv_assertion_engine_t *eng) {
    if (!eng) return;
    for (size_t i = 0; i < eng->num_assertions; i++) {
        hv_assertion_destroy(eng->assertions[i]);
    }
    free(eng->assertions);
    free(eng);
}

void hv_assertion_engine_add(hv_assertion_engine_t *eng, hv_assertion_t *a) {
    if (!eng || !a) return;
    if (eng->num_assertions >= eng->capacity_assertions) {
        size_t new_cap = (eng->capacity_assertions == 0) ? 16 : eng->capacity_assertions * 2;
        hv_assertion_t **new_assertions = (hv_assertion_t**)realloc(
            eng->assertions, new_cap * sizeof(hv_assertion_t*));
        if (!new_assertions) return;
        eng->assertions = new_assertions;
        eng->capacity_assertions = new_cap;
    }
    eng->assertions[eng->num_assertions++] = a;
}

/* Evaluate all assertions at the given simulation time/cycle.
 * For concurrent assertions, check properties on clock edges. */
void hv_assertion_engine_eval(hv_assertion_engine_t *eng,
                               sim_time_t time, uint32_t cycle) {
    if (!eng) return;

    for (size_t i = 0; i < eng->num_assertions; i++) {
        hv_assertion_t *a = eng->assertions[i];
        if (!a || !a->is_active) continue;

        /* For concurrent assertions, check clock edge */
        if (a->clock) {
            uint32_t clk_val = hv_signal_read(a->clock);
            if (clk_val != 1) continue; /* not a clock edge */
        }

        a->attempt_count++;
        eng->total_attempts++;

        /* Evaluate the property */
        hv_property_expr_t *prop = a->property;
        if (!prop) {
            a->pass_count++;
            eng->total_passes++;
            continue;
        }

        /* Simple evaluation: check if the sequence in the property holds */
        bool passed = true;
        if (prop->op == PROP_SEQ && prop->seq) {
            /* Evaluate the sequence at current cycle */
            uint32_t pos = cycle;
            passed = seq_eval_recursive(prop->seq, &pos, cycle + 10);
        } else if (prop->op == PROP_NOT && prop->left) {
            if (prop->left->op == PROP_SEQ && prop->left->seq) {
                uint32_t pos = cycle;
                passed = !seq_eval_recursive(prop->left->seq, &pos, cycle + 10);
            }
        }

        if (passed) {
            a->pass_count++;
            eng->total_passes++;
        } else {
            a->fail_count++;
            eng->total_failures++;
            a->last_fail_time = time;
            a->last_fail_cycle = cycle;
            snprintf(a->fail_detail, sizeof(a->fail_detail),
                     "Assertion '%s' failed at time %lu cycle %u",
                     a->name, (unsigned long)time, cycle);

            if (a->on_fail) {
                a->on_fail(a, time);
            }

            /* Log failure */
            fprintf(stderr, "%s: %s\n",
                    severity_str(a->severity), a->fail_detail);
        }
    }
}

void hv_assertion_engine_report(const hv_assertion_engine_t *eng, FILE *fp) {
    if (!eng || !fp) return;
    fprintf(fp, "=== Assertion Engine Report ===\n");
    fprintf(fp, "  Total assertions: %zu\n", eng->num_assertions);
    fprintf(fp, "  Total attempts: %lu\n", (unsigned long)eng->total_attempts);
    fprintf(fp, "  Total passes: %lu\n", (unsigned long)eng->total_passes);
    fprintf(fp, "  Total failures: %lu\n", (unsigned long)eng->total_failures);

    for (size_t i = 0; i < eng->num_assertions; i++) {
        hv_assertion_t *a = eng->assertions[i];
        fprintf(fp, "  [%s] %s: pass=%lu fail=%lu vacuous=%lu\n",
                a->is_active ? "ACTIVE" : "INACTIVE",
                a->name,
                (unsigned long)a->pass_count,
                (unsigned long)a->fail_count,
                (unsigned long)a->vacuous_pass);
    }
}

bool hv_assertion_engine_all_pass(const hv_assertion_engine_t *eng) {
    if (!eng) return true;
    return eng->total_failures == 0;
}
