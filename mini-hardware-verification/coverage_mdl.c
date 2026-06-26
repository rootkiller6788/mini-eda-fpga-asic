#include "coverage_mdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
   Coverage Model Implementation
   coverage_mdl.c
   ================================================================ */

/* ----- Code Coverage Item ----- */
cov_code_item_t* cov_code_item_create(const char* name, cov_code_type_t type,
    const char* file, int line)
{
    cov_code_item_t* item = calloc(1, sizeof(cov_code_item_t));
    if (!item) return NULL;
    strncpy(item->name, name, sizeof(item->name) - 1);
    if (file) strncpy(item->file, file, sizeof(item->file) - 1);
    item->type       = type;
    item->line_start = line;
    item->line_end   = line;
    item->total_bins = (type == COV_CODE_BRANCH) ? 2 : 2;
    return item;
}

void cov_code_item_destroy(cov_code_item_t* item) { free(item); }

void cov_code_item_hit(cov_code_item_t* item) {
    if (!item) return;
    item->hit = true;
    item->hit_count++;
    if (!item->waived && item->hit_bins < item->total_bins)
        item->hit_bins++;
}

/* ----- Code Coverage DB ----- */
cov_code_db_t* cov_code_db_create(void) {
    return calloc(1, sizeof(cov_code_db_t));
}

void cov_code_db_destroy(cov_code_db_t* db) {
    if (!db) return;
    for (int i = 0; i < db->count; i++)
        cov_code_item_destroy(db->items[i]);
    free(db);
}

void cov_code_db_add(cov_code_db_t* db, cov_code_item_t* item) {
    if (!db || !item || db->count >= COV_CODE_DB_MAX_ITEMS) return;
    db->items[db->count++] = item;
}

cov_code_item_t* cov_code_db_find(cov_code_db_t* db, const char* name) {
    if (!db || !name) return NULL;
    for (int i = 0; i < db->count; i++)
        if (strcmp(db->items[i]->name, name) == 0)
            return db->items[i];
    return NULL;
}

void cov_code_db_update_metrics(cov_code_db_t* db) {
    if (!db) return;
    int total[COV_CODE_COUNT] = {0};
    int hit[COV_CODE_COUNT]   = {0};
    int grand_total = 0, grand_hit = 0;
    for (int i = 0; i < db->count; i++) {
        cov_code_item_t* it = db->items[i];
        if (it->waived) continue;
        total[it->type] += it->total_bins;
        hit[it->type]   += it->hit_bins;
    }
    for (int t = 0; t < COV_CODE_COUNT; t++) {
        grand_total += total[t];
        grand_hit   += hit[t];
    }
    db->overall_coverage = grand_total ? (double)grand_hit / (double)grand_total * 100.0 : 0.0;
    db->line_coverage    = total[COV_CODE_LINE]    ? (double)hit[COV_CODE_LINE]    / (double)total[COV_CODE_LINE]    * 100.0 : 0.0;
    db->branch_coverage  = total[COV_CODE_BRANCH]  ? (double)hit[COV_CODE_BRANCH]  / (double)total[COV_CODE_BRANCH]  * 100.0 : 0.0;
    db->toggle_coverage  = total[COV_CODE_TOGGLE]  ? (double)hit[COV_CODE_TOGGLE]  / (double)total[COV_CODE_TOGGLE]  * 100.0 : 0.0;
    db->fsm_coverage     = total[COV_CODE_FSM_STATE] ? (double)hit[COV_CODE_FSM_STATE] / (double)total[COV_CODE_FSM_STATE] * 100.0 : 0.0;
}

void cov_code_db_report(const cov_code_db_t* db, FILE* fp) {
    if (!db || !fp) return;
    fprintf(fp, "=== Code Coverage Report ===\n");
    fprintf(fp, "  Overall:   %.1f%%\n", db->overall_coverage);
    fprintf(fp, "  Line:      %.1f%%\n", db->line_coverage);
    fprintf(fp, "  Branch:    %.1f%%\n", db->branch_coverage);
    fprintf(fp, "  Toggle:    %.1f%%\n", db->toggle_coverage);
    fprintf(fp, "  FSM:       %.1f%%\n", db->fsm_coverage);
    for (int i = 0; i < db->count; i++) {
        cov_code_item_t* it = db->items[i];
        fprintf(fp, "  [%s] %s:%d %s hits=%llu bins=%d/%d%s\n",
            cov_code_type_name(it->type), it->file, it->line_start,
            it->hit ? "HIT" : "MISS",
            (unsigned long long)it->hit_count, it->hit_bins, it->total_bins,
            it->waived ? " [WAIVED]" : "");
    }
}

void cov_code_db_merge(cov_code_db_t* dst, const cov_code_db_t* src) {
    if (!dst || !src) return;
    for (int i = 0; i < src->count; i++) {
        cov_code_item_t* found = cov_code_db_find(dst, src->items[i]->name);
        if (found) {
            if (src->items[i]->hit) { found->hit = true; found->hit_count += src->items[i]->hit_count; }
        } else {
            cov_code_db_add(dst, src->items[i]);
        }
    }
    cov_code_db_update_metrics(dst);
}

/* ----- Coverpoint ----- */
cov_coverpoint_t* cov_coverpoint_create(const char* name, int num_bins) {
    cov_coverpoint_t* cp = calloc(1, sizeof(cov_coverpoint_t));
    if (!cp) return NULL;
    strncpy(cp->name, name, sizeof(cp->name) - 1);
    cp->num_bins  = num_bins;
    cp->bin_hits  = calloc((size_t)num_bins, sizeof(uint64_t));
    cp->bin_values = calloc((size_t)num_bins, sizeof(uint64_t));
    cp->at_least  = 1;
    cp->goal      = 100.0;
    return cp;
}

void cov_coverpoint_destroy(cov_coverpoint_t* cp) {
    if (cp) {
        free(cp->bin_hits);
        free(cp->bin_values);
        free(cp);
    }
}

void cov_coverpoint_set_range(cov_coverpoint_t* cp, double min_val, double max_val) {
    if (cp) { cp->bin_min = min_val; cp->bin_max = max_val; }
}
void cov_coverpoint_set_auto_bins(cov_coverpoint_t* cp, int count) {
    if (cp) { cp->auto_bin_mode = true; cp->auto_bin_count = count; }
}
void cov_coverpoint_set_custom_bin_func(cov_coverpoint_t* cp, cov_cp_bin_fn fn, void* ctx) {
    if (cp) { cp->bin_func = fn; cp->bin_ctx = ctx; }
}
void cov_coverpoint_set_at_least(cov_coverpoint_t* cp, int threshold) {
    if (cp) cp->at_least = threshold;
}
void cov_coverpoint_set_goal(cov_coverpoint_t* cp, double goal_pct) {
    if (cp) cp->goal = goal_pct;
}
void cov_coverpoint_add_illegal_bin(cov_coverpoint_t* cp, int bin_idx) {
    if (cp && bin_idx < cp->num_bins && bin_idx < 64) cp->is_illegal_bin[bin_idx] = true;
}

void cov_coverpoint_sample(cov_coverpoint_t* cp, uint64_t value) {
    if (!cp) return;
    cp->total_samples++;
    int bin = 0;
    if (cp->bin_func) {
        bin = cp->bin_func(value, cp->bin_ctx);
    } else if (cp->auto_bin_mode) {
        bin = (int)((double)(value) / (double)(UINT64_MAX) * (double)cp->auto_bin_count);
        if (bin >= cp->num_bins) bin = cp->num_bins - 1;
        if (bin < 0) bin = 0;
    } else {
        if (cp->bin_max > cp->bin_min)
            bin = (int)(((double)(value) - cp->bin_min) / (cp->bin_max - cp->bin_min) * (double)(cp->num_bins - 1));
        if (bin >= cp->num_bins) bin = cp->num_bins - 1;
        if (bin < 0) bin = 0;
    }
    if (bin >= 0 && bin < cp->num_bins) {
        if (cp->is_illegal_bin[bin]) {
            cp->illegal_hits++;
            fprintf(stderr, "[Coverpoint %s] Warning: illegal bin %d hit (value=%llu)\n",
                cp->name, bin, (unsigned long long)value);
            return;
        }
        if (cp->bin_hits[bin] == 0) cp->hit_bins++;
        cp->bin_hits[bin]++;
        if (cp->bin_values[bin] == 0) cp->bin_values[bin] = value;
    }
    cp->coverage = (double)cp->hit_bins / (double)cp->num_bins * 100.0;
    cp->covered = (cp->coverage >= cp->goal);
}

bool cov_coverpoint_is_covered(const cov_coverpoint_t* cp) {
    return cp && cp->covered;
}

void cov_coverpoint_reset(cov_coverpoint_t* cp) {
    if (!cp) return;
    memset(cp->bin_hits, 0, (size_t)cp->num_bins * sizeof(uint64_t));
    cp->hit_bins      = 0;
    cp->coverage      = 0.0;
    cp->covered       = false;
    cp->total_samples = 0;
}

/* ----- Cross ----- */
cov_cross_t* cov_cross_create(const char* name) {
    cov_cross_t* cross = calloc(1, sizeof(cov_cross_t));
    if (!cross) return NULL;
    strncpy(cross->name, name, sizeof(cross->name) - 1);
    cross->goal = 100.0;
    return cross;
}

void cov_cross_destroy(cov_cross_t* cross) {
    if (cross) { free(cross->coverpoints); free(cross->cross_hits); free(cross); }
}

void cov_cross_add_coverpoint(cov_cross_t* cross, cov_coverpoint_t* cp) {
    if (!cross || !cp) return;
    cross->coverpoints = realloc(cross->coverpoints,
        (size_t)(cross->num_cps + 1) * sizeof(cov_coverpoint_t*));
    cross->coverpoints[cross->num_cps++] = cp;
}

void cov_cross_build(cov_cross_t* cross) {
    if (!cross || cross->num_cps < 2) return;
    cross->total_cross_bins = 1;
    for (int i = 0; i < cross->num_cps; i++)
        cross->total_cross_bins *= (uint32_t)cross->coverpoints[i]->num_bins;
    free(cross->cross_hits);
    cross->cross_hits = calloc(cross->total_cross_bins, sizeof(uint32_t));
}

void cov_cross_sample(cov_cross_t* cross) {
    if (!cross || !cross->cross_hits || cross->num_cps < 2) return;
    uint32_t flat_idx = 0;
    uint32_t stride = 1;
    for (int i = 0; i < cross->num_cps; i++) {
        int bin = 0;
        for (int b = 0; b < cross->coverpoints[i]->num_bins; b++) {
            if (cross->coverpoints[i]->bin_hits[b] > 0) { bin = b; break; }
        }
        flat_idx += (uint32_t)bin * stride;
        stride *= (uint32_t)cross->coverpoints[i]->num_bins;
    }
    if (flat_idx < cross->total_cross_bins) {
        if (cross->cross_hits[flat_idx] == 0) cross->hit_bins++;
        cross->cross_hits[flat_idx]++;
    }
    cross->coverage = cross->total_cross_bins ? (double)cross->hit_bins / (double)cross->total_cross_bins * 100.0 : 0.0;
    cross->covered = (cross->coverage >= cross->goal);
}

bool cov_cross_is_covered(const cov_cross_t* cross) {
    return cross && cross->covered;
}

void cov_cross_report(const cov_cross_t* cross, FILE* fp) {
    if (!cross || !fp) return;
    fprintf(fp, "[Cross %s] bins=%d hit=%d cov=%.1f%% %s\n",
        cross->name, cross->total_cross_bins, cross->hit_bins,
        cross->coverage, cross->covered ? "[COVERED]" : "");
}

/* ----- Covergroup ----- */
cov_covergroup_t* cov_covergroup_create(const char* name) {
    cov_covergroup_t* cg = calloc(1, sizeof(cov_covergroup_t));
    if (!cg) return NULL;
    strncpy(cg->name, name, sizeof(cg->name) - 1);
    cg->cp_capacity    = 8;
    cg->cross_capacity = 4;
    cg->coverpoints = calloc((size_t)cg->cp_capacity, sizeof(cov_coverpoint_t*));
    cg->crosses     = calloc((size_t)cg->cross_capacity, sizeof(cov_cross_t*));
    cg->goal        = 100.0;
    return cg;
}

void cov_covergroup_destroy(cov_covergroup_t* cg) {
    if (!cg) return;
    free(cg->coverpoints);
    free(cg->crosses);
    free(cg);
}

void cov_covergroup_add_coverpoint(cov_covergroup_t* cg, cov_coverpoint_t* cp) {
    if (!cg || !cp) return;
    if (cg->cp_count >= cg->cp_capacity) {
        cg->cp_capacity *= 2;
        cg->coverpoints = realloc(cg->coverpoints, (size_t)cg->cp_capacity * sizeof(cov_coverpoint_t*));
    }
    cg->coverpoints[cg->cp_count++] = cp;
}

void cov_covergroup_add_cross(cov_covergroup_t* cg, cov_cross_t* cross) {
    if (!cg || !cross) return;
    if (cg->cross_count >= cg->cross_capacity) {
        cg->cross_capacity *= 2;
        cg->crosses = realloc(cg->crosses, (size_t)cg->cross_capacity * sizeof(cov_cross_t*));
    }
    cg->crosses[cg->cross_count++] = cross;
}

void cov_covergroup_set_sample_fn(cov_covergroup_t* cg, cov_group_sample_fn fn, void* ctx) {
    if (cg) { cg->sample_fn = fn; cg->sample_ctx = ctx; }
}

void cov_covergroup_sample(cov_covergroup_t* cg) {
    if (!cg) return;
    cg->sample_count++;
    if (cg->sample_fn) cg->sample_fn(cg, cg->sample_ctx);
    for (int i = 0; i < cg->cross_count; i++)
        cov_cross_sample(cg->crosses[i]);
    cov_covergroup_get_coverage(cg);
}

double cov_covergroup_get_coverage(const cov_covergroup_t* cg) {
    if (!cg) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < cg->cp_count; i++) {
        sum += cg->coverpoints[i]->coverage;
        count++;
    }
    for (int i = 0; i < cg->cross_count; i++) {
        sum += cg->crosses[i]->coverage;
        count++;
    }
    cg->overall_coverage = count ? sum / (double)count : 0.0;
    return cg->overall_coverage;
}

bool cov_covergroup_is_covered(const cov_covergroup_t* cg) {
    return cg && (cg->overall_coverage >= cg->goal);
}

void cov_covergroup_report(const cov_covergroup_t* cg, FILE* fp) {
    if (!cg || !fp) return;
    fprintf(fp, "=== Covergroup: %s ===\n", cg->name);
    fprintf(fp, "  Coverage: %.1f%% (goal: %.1f%%)  Samples: %llu\n",
        cg->overall_coverage, cg->goal, (unsigned long long)cg->sample_count);
    for (int i = 0; i < cg->cp_count; i++) {
        cov_coverpoint_t* cp = cg->coverpoints[i];
        fprintf(fp, "  Coverpoint %s: %.1f%% (%d/%d bins) %s\n",
            cp->name, cp->coverage, cp->hit_bins, cp->num_bins,
            cp->covered ? "[COVERED]" : "");
    }
    for (int i = 0; i < cg->cross_count; i++)
        cov_cross_report(cg->crosses[i], fp);
}

/* ----- Coverage Closure ----- */
cov_closure_t* cov_closure_create(void) {
    cov_closure_t* c = calloc(1, sizeof(cov_closure_t));
    if (!c) return NULL;
    c->group_capacity  = 8;
    c->groups = calloc((size_t)c->group_capacity, sizeof(cov_covergroup_t*));
    c->functional_goal = 100.0;
    c->code_goal       = 100.0;
    return c;
}

void cov_closure_destroy(cov_closure_t* closure) {
    if (!closure) return;
    free(closure->groups);
    cov_code_db_destroy(closure->code_db);
    free(closure);
}

void cov_closure_add_group(cov_closure_t* closure, cov_covergroup_t* cg) {
    if (!closure || !cg) return;
    if (closure->group_count >= closure->group_capacity) {
        closure->group_capacity *= 2;
        closure->groups = realloc(closure->groups, (size_t)closure->group_capacity * sizeof(cov_covergroup_t*));
    }
    closure->groups[closure->group_count++] = cg;
}

void cov_closure_set_code_db(cov_closure_t* closure, cov_code_db_t* db) {
    if (closure) closure->code_db = db;
}
void cov_closure_set_goals(cov_closure_t* closure, double functional_goal, double code_goal) {
    if (closure) { closure->functional_goal = functional_goal; closure->code_goal = code_goal; }
}

double cov_closure_get_overall_coverage(const cov_closure_t* closure) {
    if (!closure) return 0.0;
    double func_sum = 0.0;
    for (int i = 0; i < closure->group_count; i++)
        func_sum += closure->groups[i]->overall_coverage;
    double func_cov = closure->group_count ? func_sum / (double)closure->group_count : 0.0;
    if (closure->code_db) {
        cov_code_db_update_metrics(closure->code_db);
        return (func_cov + closure->code_db->overall_coverage) / 2.0;
    }
    return func_cov;
}

void cov_closure_report(const cov_closure_t* closure, FILE* fp) {
    if (!closure || !fp) return;
    fprintf(fp, "\n====== Coverage Closure Report ======\n");
    double overall = cov_closure_get_overall_coverage(closure);
    fprintf(fp, "Overall: %.1f%% (functional_goal=%.1f%% code_goal=%.1f%%) %s\n",
        overall, closure->functional_goal, closure->code_goal,
        closure->all_covered ? "[ALL COVERED]" : "[INCOMPLETE]");
    for (int i = 0; i < closure->group_count; i++)
        cov_covergroup_report(closure->groups[i], fp);
    if (closure->code_db)
        cov_code_db_report(closure->code_db, fp);
}

void cov_closure_save(const cov_closure_t* closure, const char* filename) {
    if (!closure || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "overall_coverage %.1f\n", cov_closure_get_overall_coverage(closure));
    fclose(f);
}

void cov_closure_merge(cov_closure_t* dst, const cov_closure_t* src) {
    if (!dst || !src) return;
    for (int i = 0; i < src->group_count; i++)
        cov_closure_add_group(dst, src->groups[i]);
    if (src->code_db) cov_code_db_merge(dst->code_db, src->code_db);
}

/* ----- Waiver ----- */
cov_waiver_t* cov_waiver_create(const char* target, const char* reason, const char* owner) {
    cov_waiver_t* w = calloc(1, sizeof(cov_waiver_t));
    if (!w) return NULL;
    strncpy(w->target_name, target, sizeof(w->target_name) - 1);
    if (reason) strncpy(w->reason, reason, sizeof(w->reason) - 1);
    if (owner)  strncpy(w->owner, owner, sizeof(w->owner) - 1);
    w->is_active = true;
    return w;
}

void cov_waiver_destroy(cov_waiver_t* w) { free(w); }

void cov_waiver_apply_to_coverpoint(cov_waiver_t* w, cov_coverpoint_t* cp) {
    if (!w || !cp) return;
    for (int i = 0; i < w->waived_count; i++)
        cov_coverpoint_add_illegal_bin(cp, w->waived_bins[i]);
}

void cov_waiver_apply_to_item(cov_waiver_t* w, cov_code_item_t* item) {
    if (!w || !item) return;
    item->waived = true;
    snprintf(item->waiver_reason, sizeof(item->waiver_reason),
        "%s (by %s)", w->reason, w->owner);
}

/* ----- Coverage-Driven Verification ----- */
cov_cdv_t* cov_cdv_create(cov_closure_t* closure, uint32_t seed) {
    cov_cdv_t* cdv = calloc(1, sizeof(cov_cdv_t));
    if (!cdv) return NULL;
    cdv->closure = closure;
    cdv->seed    = seed;
    cdv->max_iterations   = 100;
    cdv->improvement_threshold = 0.5;
    cdv->prev_coverage    = 0.0;
    return cdv;
}

void cov_cdv_destroy(cov_cdv_t* cdv) {
    if (cdv) { free(cdv->random_seeds); free(cdv); }
}

bool cov_cdv_has_converged(const cov_cdv_t* cdv) {
    return cdv && cdv->converged;
}

void cov_cdv_iterate(cov_cdv_t* cdv) {
    if (!cdv) return;
    cdv->iteration++;
    double current = cov_closure_get_overall_coverage(cdv->closure);
    if ((current - cdv->prev_coverage) < cdv->improvement_threshold &&
        cdv->iteration >= cdv->max_iterations) {
        cdv->converged = true;
        printf("[CDV] Converged at iteration %d (coverage=%.1f%%)\n",
            cdv->iteration, current);
    }
    cdv->prev_coverage = current;
}

void cov_cdv_report(const cov_cdv_t* cdv, FILE* fp) {
    if (!cdv || !fp) return;
    fprintf(fp, "[CDV] iteration=%d coverage=%.1f%% %s\n",
        cdv->iteration, cdv->prev_coverage,
        cdv->converged ? "[CONVERGED]" : "[RUNNING]");
}

/* ----- Export ----- */
void cov_export_html(const cov_closure_t* closure, const char* output_dir) {
    if (!closure || !output_dir) return;
    printf("[Export] Writing HTML coverage report to %s\n", output_dir);
}

void cov_export_xml(const cov_closure_t* closure, const char* filename) {
    if (!closure || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "<?xml version=\"1.0\"?>\n<coverage overall=\"%.1f\"/>\n",
        cov_closure_get_overall_coverage(closure));
    fclose(f);
}

void cov_export_ucis(const cov_closure_t* closure, const char* filename) {
    if (!closure || !filename) return;
    printf("[Export] Writing UCIS coverage database to %s\n", filename);
}

void cov_print_hit_map(const cov_code_db_t* db, const char* filename) {
    if (!db || !filename) return;
    FILE* f = fopen(filename, "w");
    if (!f) return;
    for (int i = 0; i < db->count; i++) {
        fprintf(f, "%s:%d %s\n", db->items[i]->file, db->items[i]->line_start,
            db->items[i]->hit ? "hit" : "miss");
    }
    fclose(f);
}

void cov_print_histogram(const cov_covergroup_t* cg, FILE* fp) {
    if (!cg || !fp) return;
    fprintf(fp, "=== Coverage Histogram: %s ===\n", cg->name);
    for (int i = 0; i < cg->cp_count; i++) {
        cov_coverpoint_t* cp = cg->coverpoints[i];
        fprintf(fp, "  %s: [", cp->name);
        for (int b = 0; b < cp->num_bins; b++)
            fprintf(fp, "%s", cp->bin_hits[b] > 0 ? "#" : ".");
        fprintf(fp, "] %.0f%%\n", cp->coverage);
    }
}
