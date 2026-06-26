/**
 * coverage_model.c - Coverage-Driven Verification (CDV) Implementation
 *
 * Implements coverage model with covergroups, coverpoints, bins,
 * cross-coverage, coverage database with merge and gap analysis.
 */

#include "coverage_model.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * Bin Operations (L1)
 * ======================================================================== */

hv_coverage_bin_t hv_bin_auto(const char *name, uint32_t low, uint32_t high) {
    hv_coverage_bin_t bin;
    memset(&bin, 0, sizeof(bin));
    if (name) strncpy(bin.name, name, sizeof(bin.name) - 1);
    bin.type = BIN_AUTO;
    bin.range.low = low;
    bin.range.high = high;
    bin.hit_count = 0;
    bin.goal_count = 1;
    bin.is_covered = false;
    return bin;
}

hv_coverage_bin_t hv_bin_manual(const char *name, uint32_t value) {
    hv_coverage_bin_t bin;
    memset(&bin, 0, sizeof(bin));
    if (name) strncpy(bin.name, name, sizeof(bin.name) - 1);
    bin.type = BIN_MANUAL;
    bin.range.low = value;
    bin.range.high = value;
    bin.hit_count = 0;
    bin.goal_count = 1;
    bin.is_covered = false;
    return bin;
}

hv_coverage_bin_t hv_bin_transition(const char *name,
                                     uint32_t from, uint32_t to) {
    hv_coverage_bin_t bin;
    memset(&bin, 0, sizeof(bin));
    if (name) strncpy(bin.name, name, sizeof(bin.name) - 1);
    bin.type = BIN_TRANSITION;
    bin.from_value = from;
    bin.to_value = to;
    bin.range.low = from;
    bin.range.high = to;
    bin.hit_count = 0;
    bin.goal_count = 1;
    bin.is_covered = false;
    return bin;
}

hv_coverage_bin_t hv_bin_illegal(const char *name, uint32_t value) {
    hv_coverage_bin_t bin;
    memset(&bin, 0, sizeof(bin));
    if (name) strncpy(bin.name, name, sizeof(bin.name) - 1);
    bin.type = BIN_ILLEGAL;
    bin.range.low = value;
    bin.range.high = value;
    bin.is_illegal = true;
    bin.hit_count = 0;
    bin.goal_count = 0;
    bin.is_covered = false;
    return bin;
}

void hv_bin_hit(hv_coverage_bin_t *bin) {
    if (!bin) return;
    bin->hit_count++;
    if (bin->hit_count >= bin->goal_count && !bin->is_illegal) {
        bin->is_covered = true;
    }
}

/* ========================================================================
 * Coverpoint Operations (L1, L2)
 * ======================================================================== */

hv_coverpoint_t *hv_coverpoint_create(const char *name, coverpoint_type_t type,
                                       uint32_t width) {
    hv_coverpoint_t *cp = (hv_coverpoint_t*)calloc(1, sizeof(hv_coverpoint_t));
    if (!cp) return NULL;
    if (name) strncpy(cp->name, name, sizeof(cp->name) - 1);
    cp->type = type;
    cp->width = (width > 0 && width <= 64) ? width : 32;
    cp->bins = NULL;
    cp->num_bins = 0;
    cp->capacity_bins = 0;
    cp->total_hits = 0;
    cp->coverage_pct = 0.0f;
    cp->sample_cb = NULL;
    cp->sample_ctx = NULL;
    return cp;
}

void hv_coverpoint_destroy(hv_coverpoint_t *cp) {
    if (!cp) return;
    free(cp->bins);
    free(cp);
}

void hv_coverpoint_add_bin(hv_coverpoint_t *cp, hv_coverage_bin_t bin) {
    if (!cp) return;
    if (cp->num_bins >= cp->capacity_bins) {
        size_t new_cap = (cp->capacity_bins == 0) ? 16 : cp->capacity_bins * 2;
        hv_coverage_bin_t *new_bins = (hv_coverage_bin_t*)realloc(
            cp->bins, new_cap * sizeof(hv_coverage_bin_t));
        if (!new_bins) return;
        cp->bins = new_bins;
        cp->capacity_bins = new_cap;
    }
    cp->bins[cp->num_bins++] = bin;
}

void hv_coverpoint_sample(hv_coverpoint_t *cp, uint32_t value) {
    if (!cp) return;
    cp->total_hits++;

    /* Check each bin to see if value falls in range */
    for (size_t i = 0; i < cp->num_bins; i++) {
        hv_coverage_bin_t *bin = &cp->bins[i];
        bool hit = false;

        switch (bin->type) {
            case BIN_AUTO:
            case BIN_MANUAL:
            case BIN_ILLEGAL:
            case BIN_IGNORE:
                hit = (value >= bin->range.low && value <= bin->range.high);
                break;
            case BIN_TRANSITION:
                /* Transition bins are matched by transition sampling,
                   not direct value sampling */
                break;
            case BIN_CROSS:
                break;
        }

        if (hit) {
            hv_bin_hit(bin);
            if (bin->is_illegal) {
                fprintf(stderr, "WARNING: illegal bin '%s' hit with value %u\n",
                        bin->name, value);
            }
        }
    }

    /* Update coverage percentage */
    cp->coverage_pct = hv_coverpoint_get_coverage(cp);
}

float hv_coverpoint_get_coverage(const hv_coverpoint_t *cp) {
    if (!cp || cp->num_bins == 0) return 0.0f;
    size_t covered = 0;
    size_t total_legal = 0;
    for (size_t i = 0; i < cp->num_bins; i++) {
        if (cp->bins[i].is_illegal) continue;
        total_legal++;
        if (cp->bins[i].is_covered) covered++;
    }
    if (total_legal == 0) return 100.0f;
    return ((float)covered / (float)total_legal) * 100.0f;
}

void hv_coverpoint_set_sample_cb(hv_coverpoint_t *cp,
                                  uint32_t (*cb)(void*), void *ctx) {
    if (!cp) return;
    cp->sample_cb = cb;
    cp->sample_ctx = ctx;
}

void hv_coverpoint_report(const hv_coverpoint_t *cp, FILE *fp) {
    if (!cp || !fp) return;
    fprintf(fp, "  Coverpoint: %s (%.1f%%, %zu bins)\n",
            cp->name, cp->coverage_pct, cp->num_bins);
    for (size_t i = 0; i < cp->num_bins; i++) {
        const hv_coverage_bin_t *b = &cp->bins[i];
        fprintf(fp, "    bin[%s]: hits=%lu goal=%lu %s %s\n",
                b->name, (unsigned long)b->hit_count,
                (unsigned long)b->goal_count,
                b->is_covered ? "[COVERED]" : "[UNCOVERED]",
                b->is_illegal ? "[ILLEGAL]" : "");
    }
}

/* ========================================================================
 * Covergroup Operations (L2)
 * ======================================================================== */

hv_covergroup_t *hv_covergroup_create(const char *name) {
    hv_covergroup_t *cg = (hv_covergroup_t*)calloc(1, sizeof(hv_covergroup_t));
    if (!cg) return NULL;
    if (name) strncpy(cg->name, name, sizeof(cg->name) - 1);
    cg->coverpoints = NULL;
    cg->num_coverpoints = 0;
    cg->capacity_coverpoints = 0;
    cg->crosses = NULL;
    cg->num_crosses = 0;
    cg->capacity_crosses = 0;
    cg->aggregate_coverage = 0.0f;
    cg->total_samples = 0;
    return cg;
}

void hv_covergroup_destroy(hv_covergroup_t *cg) {
    if (!cg) return;
    for (size_t i = 0; i < cg->num_coverpoints; i++) {
        hv_coverpoint_destroy(cg->coverpoints[i]);
    }
    free(cg->coverpoints);
    for (size_t i = 0; i < cg->num_crosses; i++) {
        if (cg->crosses[i]) {
            free(cg->crosses[i]->bins);
            free(cg->crosses[i]);
        }
    }
    free(cg->crosses);
    free(cg);
}

void hv_covergroup_add_coverpoint(hv_covergroup_t *cg, hv_coverpoint_t *cp) {
    if (!cg || !cp) return;
    if (cg->num_coverpoints >= cg->capacity_coverpoints) {
        size_t new_cap = (cg->capacity_coverpoints == 0) ? 8 : cg->capacity_coverpoints * 2;
        hv_coverpoint_t **new_cps = (hv_coverpoint_t**)realloc(
            cg->coverpoints, new_cap * sizeof(hv_coverpoint_t*));
        if (!new_cps) return;
        cg->coverpoints = new_cps;
        cg->capacity_coverpoints = new_cap;
    }
    cg->coverpoints[cg->num_coverpoints++] = cp;
}

hv_cross_t *hv_covergroup_add_cross(hv_covergroup_t *cg,
                                     const char *name, hv_coverpoint_t *a,
                                     hv_coverpoint_t *b) {
    if (!cg || !a || !b) return NULL;
    if (cg->num_crosses >= cg->capacity_crosses) {
        size_t new_cap = (cg->capacity_crosses == 0) ? 4 : cg->capacity_crosses * 2;
        hv_cross_t **new_crosses = (hv_cross_t**)realloc(
            cg->crosses, new_cap * sizeof(hv_cross_t*));
        if (!new_crosses) return NULL;
        cg->crosses = new_crosses;
        cg->capacity_crosses = new_cap;
    }
    hv_cross_t *cross = (hv_cross_t*)calloc(1, sizeof(hv_cross_t));
    if (!cross) return NULL;
    if (name) strncpy(cross->name, name, sizeof(cross->name) - 1);
    cross->cp_a = a;
    cross->cp_b = b;
    /* Create cross-product bins */
    size_t total = a->num_bins * b->num_bins;
    cross->bins = (hv_coverage_bin_t*)calloc(total, sizeof(hv_coverage_bin_t));
    if (!cross->bins) {
        free(cross);
        return NULL;
    }
    cross->num_bins = total;
    for (size_t ia = 0; ia < a->num_bins; ia++) {
        for (size_t ib = 0; ib < b->num_bins; ib++) {
            size_t idx = ia * b->num_bins + ib;
            /* Truncated concatenation: max 127 chars + null */
            size_t max_name = sizeof(cross->bins[idx].name);
            snprintf(cross->bins[idx].name, max_name,
                     "%.60s_x_%.60s", a->bins[ia].name, b->bins[ib].name);
            cross->bins[idx].type = BIN_CROSS;
            cross->bins[idx].cross_idx_a = (uint32_t)ia;
            cross->bins[idx].cross_idx_b = (uint32_t)ib;
            cross->bins[idx].goal_count = 1;
        }
    }
    cg->crosses[cg->num_crosses++] = cross;
    return cross;
}

float hv_covergroup_get_coverage(const hv_covergroup_t *cg) {
    if (!cg || cg->num_coverpoints == 0) return 0.0f;

    /* Weighted average: 70% coverpoints, 30% crosses */
    float cp_sum = 0.0f;
    for (size_t i = 0; i < cg->num_coverpoints; i++) {
        cp_sum += hv_coverpoint_get_coverage(cg->coverpoints[i]);
    }
    float cp_avg = cp_sum / (float)cg->num_coverpoints;

    float cross_sum = 0.0f;
    if (cg->num_crosses > 0) {
        for (size_t i = 0; i < cg->num_crosses; i++) {
            size_t covered = 0;
            for (size_t j = 0; j < cg->crosses[i]->num_bins; j++) {
                if (cg->crosses[i]->bins[j].is_covered) covered++;
            }
            cross_sum += ((float)covered / (float)cg->crosses[i]->num_bins) * 100.0f;
        }
        cross_sum /= (float)cg->num_crosses;
        return cp_avg * 0.7f + cross_sum * 0.3f;
    }

    return cp_avg;
}

void hv_covergroup_report(const hv_covergroup_t *cg, FILE *fp) {
    if (!cg || !fp) return;
    fprintf(fp, "Covergroup: %s (agg: %.1f%%)\n", cg->name,
            hv_covergroup_get_coverage(cg));
    for (size_t i = 0; i < cg->num_coverpoints; i++) {
        hv_coverpoint_report(cg->coverpoints[i], fp);
    }
    for (size_t i = 0; i < cg->num_crosses; i++) {
        fprintf(fp, "  Cross: %s\n", cg->crosses[i]->name);
    }
}

/* ========================================================================
 * Coverage Database (L2, L5)
 * ======================================================================== */

hv_coverage_db_t *hv_coverage_db_create(const char *name) {
    hv_coverage_db_t *db = (hv_coverage_db_t*)calloc(1, sizeof(hv_coverage_db_t));
    if (!db) return NULL;
    if (name) strncpy(db->name, name, sizeof(db->name) - 1);
    db->groups = NULL;
    db->num_groups = 0;
    db->capacity_groups = 0;
    return db;
}

void hv_coverage_db_destroy(hv_coverage_db_t *db) {
    if (!db) return;
    for (size_t i = 0; i < db->num_groups; i++) {
        hv_covergroup_destroy(db->groups[i]);
    }
    free(db->groups);
    free(db);
}

void hv_coverage_db_add_group(hv_coverage_db_t *db, hv_covergroup_t *cg) {
    if (!db || !cg) return;
    if (db->num_groups >= db->capacity_groups) {
        size_t new_cap = (db->capacity_groups == 0) ? 8 : db->capacity_groups * 2;
        hv_covergroup_t **new_groups = (hv_covergroup_t**)realloc(
            db->groups, new_cap * sizeof(hv_covergroup_t*));
        if (!new_groups) return;
        db->groups = new_groups;
        db->capacity_groups = new_cap;
    }
    db->groups[db->num_groups++] = cg;
}

float hv_coverage_db_get_total(const hv_coverage_db_t *db) {
    if (!db || db->num_groups == 0) return 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < db->num_groups; i++) {
        sum += hv_covergroup_get_coverage(db->groups[i]);
    }
    return sum / (float)db->num_groups;
}

void hv_coverage_db_report(const hv_coverage_db_t *db, FILE *fp) {
    if (!db || !fp) return;
    fprintf(fp, "=== Coverage Database: %s ===\n", db->name);
    fprintf(fp, "Total coverage: %.1f%%\n", hv_coverage_db_get_total(db));
    for (size_t i = 0; i < db->num_groups; i++) {
        hv_covergroup_report(db->groups[i], fp);
    }
}

void hv_coverage_db_merge(hv_coverage_db_t *dst, const hv_coverage_db_t *src) {
    if (!dst || !src) return;
    for (size_t si = 0; si < src->num_groups; si++) {
        hv_covergroup_t *src_cg = src->groups[si];
        /* Look for matching group in dst */
        hv_covergroup_t *dst_cg = NULL;
        for (size_t di = 0; di < dst->num_groups; di++) {
            if (strcmp(dst->groups[di]->name, src_cg->name) == 0) {
                dst_cg = dst->groups[di];
                break;
            }
        }
        if (!dst_cg) {
            /* Add new group */
            hv_coverage_db_add_group(dst, src_cg);
        } else {
            /* Merge: max of hit counts */
            for (size_t ci = 0; ci < src_cg->num_coverpoints && ci < dst_cg->num_coverpoints; ci++) {
                for (size_t bi = 0; bi < src_cg->coverpoints[ci]->num_bins && bi < dst_cg->coverpoints[ci]->num_bins; bi++) {
                    if (src_cg->coverpoints[ci]->bins[bi].hit_count >
                        dst_cg->coverpoints[ci]->bins[bi].hit_count) {
                        dst_cg->coverpoints[ci]->bins[bi].hit_count =
                            src_cg->coverpoints[ci]->bins[bi].hit_count;
                        dst_cg->coverpoints[ci]->bins[bi].is_covered =
                            src_cg->coverpoints[ci]->bins[bi].is_covered;
                    }
                }
            }
        }
    }
}

/* ========================================================================
 * Coverage Gap Analysis (L5)
 *
 * Algorithm: Iterate all covergroups, coverpoints, and bins.
 * For each uncovered legal bin, create a gap report entry.
 * Sort by priority: bins with higher expected coverage goal first.
 * ======================================================================== */

hv_coverage_gap_list_t *hv_coverage_find_gaps(const hv_coverage_db_t *db) {
    if (!db) return NULL;

    hv_coverage_gap_list_t *list = (hv_coverage_gap_list_t*)calloc(1, sizeof(*list));
    if (!list) return NULL;

    /* First pass: count gaps */
    size_t count = 0;
    for (size_t gi = 0; gi < db->num_groups; gi++) {
        hv_covergroup_t *cg = db->groups[gi];
        for (size_t ci = 0; ci < cg->num_coverpoints; ci++) {
            hv_coverpoint_t *cp = cg->coverpoints[ci];
            for (size_t bi = 0; bi < cp->num_bins; bi++) {
                if (!cp->bins[bi].is_covered && !cp->bins[bi].is_illegal) {
                    count++;
                }
            }
        }
    }

    list->gaps = (hv_coverage_gap_t*)calloc(count, sizeof(hv_coverage_gap_t));
    if (!list->gaps) {
        free(list);
        return NULL;
    }
    list->num_gaps = count;

    /* Second pass: fill gaps */
    count = 0;
    for (size_t gi = 0; gi < db->num_groups; gi++) {
        hv_covergroup_t *cg = db->groups[gi];
        for (size_t ci = 0; ci < cg->num_coverpoints; ci++) {
            hv_coverpoint_t *cp = cg->coverpoints[ci];
            for (size_t bi = 0; bi < cp->num_bins; bi++) {
                hv_coverage_bin_t *bin = &cp->bins[bi];
                if (!bin->is_covered && !bin->is_illegal) {
                    hv_coverage_gap_t *gap = &list->gaps[count++];
                    strncpy(gap->group_name, cg->name, sizeof(gap->group_name) - 1);
                    strncpy(gap->coverpoint_name, cp->name, sizeof(gap->coverpoint_name) - 1);
                    strncpy(gap->bin_name, bin->name, sizeof(gap->bin_name) - 1);
                    gap->expected_min = bin->range.low;
                    gap->expected_max = bin->range.high;
                    gap->priority = (uint32_t)(bin->goal_count - bin->hit_count);
                }
            }
        }
    }

    return list;
}

void hv_coverage_gap_list_destroy(hv_coverage_gap_list_t *list) {
    if (!list) return;
    free(list->gaps);
    free(list);
}

void hv_coverage_gap_list_report(const hv_coverage_gap_list_t *list, FILE *fp) {
    if (!list || !fp) return;
    fprintf(fp, "=== Coverage Gaps (%zu total) ===\n", list->num_gaps);
    fprintf(fp, "%-30s %-30s %-24s %s\n",
            "Covergroup", "Coverpoint", "Bin", "Priority");
    for (size_t i = 0; i < list->num_gaps; i++) {
        hv_coverage_gap_t *g = &list->gaps[i];
        fprintf(fp, "%-30.30s %-30.30s %-24.24s %u\n",
                g->group_name, g->coverpoint_name, g->bin_name, g->priority);
    }
}
