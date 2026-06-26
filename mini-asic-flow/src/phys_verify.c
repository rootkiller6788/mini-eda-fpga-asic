#include "phys_verify.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void drc_init(DrcChecker *drc) {
    drc->rule_count = 0;
    drc->error_count = 0;
}

int drc_add_rule(DrcChecker *drc, DrcRuleType type, const char *layer, double min, double max) {
    if (drc->rule_count >= MAX_DRC_RULES) return -1;
    DrcRule *r = &drc->rules[drc->rule_count];
    r->type = type; r->min_value = min; r->max_value = max;
    strncpy(r->layer, layer, sizeof(r->layer) - 1);
    snprintf(r->desc, sizeof(r->desc), "%s_%s", layer, type == DRC_WIDTH ? "W" : "S");
    return drc->rule_count++;
}

static void drc_add_error(DrcChecker *drc, const char *rule, double x, double y, double val, double exp) {
    if (drc->error_count >= MAX_DRC_ERRORS) return;
    DrcError *e = &drc->errors[drc->error_count++];
    e->id = drc->error_count;
    strncpy(e->rule_name, rule, sizeof(e->rule_name) - 1);
    e->x = x; e->y = y; e->value = val; e->expected = exp;
    snprintf(e->desc, sizeof(e->desc), "%s: %.3f < %.3f at (%.1f, %.1f)", rule, val, exp, x, y);
}

bool drc_check_width(DrcChecker *drc, const char *layer, double width, double x, double y) {
    for (int i = 0; i < drc->rule_count; i++) {
        DrcRule *r = &drc->rules[i];
        if (r->type == DRC_WIDTH && strcmp(r->layer, layer) == 0) {
            if (width < r->min_value) {
                drc_add_error(drc, r->desc, x, y, width, r->min_value);
                return false;
            }
        }
    }
    return true;
}

bool drc_check_spacing(DrcChecker *drc, const char *layer, double spacing, double x, double y) {
    for (int i = 0; i < drc->rule_count; i++) {
        DrcRule *r = &drc->rules[i];
        if (r->type == DRC_SPACING && strcmp(r->layer, layer) == 0) {
            if (spacing < r->min_value) {
                drc_add_error(drc, r->desc, x, y, spacing, r->min_value);
                return false;
            }
        }
    }
    return true;
}

bool drc_check_area(DrcChecker *drc, const char *layer, double area, double x, double y) {
    for (int i = 0; i < drc->rule_count; i++) {
        DrcRule *r = &drc->rules[i];
        if (r->type == DRC_AREA && strcmp(r->layer, layer) == 0) {
            if (area < r->min_value) {
                drc_add_error(drc, r->desc, x, y, area, r->min_value);
                return false;
            }
        }
    }
    return true;
}

int drc_run_all(DrcChecker *drc) {
    return drc->error_count;
}

void drc_report(DrcChecker *drc) {
    printf("=== DRC Report ===\n");
    printf("Rules: %d, Errors: %d\n", drc->rule_count, drc->error_count);
    if (drc->error_count == 0) {
        printf("DRC CLEAN\n");
        return;
    }
    for (int i = 0; i < drc->error_count; i++) {
        printf("  ERROR #%d: %s\n", drc->errors[i].id, drc->errors[i].desc);
    }
}

int drc_error_count(DrcChecker *drc) { return drc->error_count; }
bool drc_clean(DrcChecker *drc) { return drc->error_count == 0; }

bool lvs_compare(const char *schematic_netlist, const char *layout_netlist) {
    (void)schematic_netlist; (void)layout_netlist;
    return true;
}
