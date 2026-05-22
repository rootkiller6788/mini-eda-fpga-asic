#include "drc_lvs.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void drc_lvs_init(drc_lvs_engine_t *eng)
{
    memset(eng, 0, sizeof(*eng));
}

int drc_add_rule(drc_lvs_engine_t *eng, drc_rule_type_t type, int layer,
                 double min_width, double min_spacing, drc_severity_t severity)
{
    drc_rule_t *r;
    if (eng->rule_count >= MAX_DRC_RULES) return -1;
    r = &eng->rules[eng->rule_count++];
    memset(r, 0, sizeof(*r));
    r->id            = eng->rule_count;
    r->type          = type;
    r->layer         = layer;
    r->min_width_nm  = min_width;
    r->min_spacing_nm = min_spacing;
    r->severity      = severity;
    snprintf(r->name, sizeof(r->name), "%s_L%d",
             type == DRC_SPACING ? "SPACING" :
             type == DRC_WIDTH ? "WIDTH" :
             type == DRC_ENCLOSURE ? "ENC" :
             type == DRC_AREA ? "AREA" : "OTHER", layer);
    return 0;
}

static int drc_report_violation(drc_lvs_engine_t *eng, int rule_id, int layer,
                                double measured, double required,
                                double x1, double y1, double x2, double y2)
{
    drc_violation_t *v;
    const drc_rule_t *r = NULL;
    int i;
    if (eng->violation_count >= MAX_DRC_VIOLATIONS) return -1;
    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].id == rule_id) { r = &eng->rules[i]; break; }
    }
    v = &eng->violations[eng->violation_count++];
    v->id             = eng->violation_count;
    v->rule_id        = rule_id;
    v->layer          = layer;
    v->x1_nm          = x1; v->y1_nm = y1;
    v->x2_nm          = x2; v->y2_nm = y2;
    v->measured_value = measured;
    v->required_value = required;
    v->severity       = r ? r->severity : DRC_SEVERITY_ERROR;
    if (r) {
        v->type = r->type;
        snprintf(v->description, sizeof(v->description),
                 "%s violation: %.1f vs %.1f required", r->name, measured, required);
    }
    v->is_fixed = 0;
    eng->total_drc_errors++;
    return 0;
}

int drc_check_spacing(drc_lvs_engine_t *eng, int layer,
                      double x1, double y1, double x2, double y2,
                      double other_x1, double other_y1,
                      double other_x2, double other_y2)
{
    int i;
    double dist = drc_rect_distance(x1, y1, x2, y2, other_x1, other_y1,
                                     other_x2, other_y2);
    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].layer == layer && eng->rules[i].type == DRC_SPACING) {
            if (dist < eng->rules[i].min_spacing_nm) {
                return drc_report_violation(eng, eng->rules[i].id, layer,
                                            dist, eng->rules[i].min_spacing_nm,
                                            x1, y1, x2, y2);
            }
        }
    }
    return 0;
}

int drc_check_width(drc_lvs_engine_t *eng, int layer,
                    double x1, double y1, double x2, double y2)
{
    int i;
    double w = fabs(x2 - x1);
    double h = fabs(y2 - y1);
    double min_dim = (w < h) ? w : h;
    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].layer == layer && eng->rules[i].type == DRC_WIDTH) {
            if (min_dim < eng->rules[i].min_width_nm) {
                return drc_report_violation(eng, eng->rules[i].id, layer,
                                            min_dim, eng->rules[i].min_width_nm,
                                            x1, y1, x2, y2);
            }
        }
    }
    return 0;
}

int drc_check_enclosure(drc_lvs_engine_t *eng, int inner_layer,
                         int outer_layer, double ix, double iy,
                         double iw, double ih, double ox, double oy,
                         double ow, double oh)
{
    int i;
    double margin_left   = ix - ox;
    double margin_right  = (ox + ow) - (ix + iw);
    double margin_bottom = iy - oy;
    double margin_top    = (oy + oh) - (iy + ih);
    double min_margin = margin_left;
    if (margin_right < min_margin) min_margin = margin_right;
    if (margin_bottom < min_margin) min_margin = margin_bottom;
    if (margin_top < min_margin) min_margin = margin_top;

    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].type == DRC_ENCLOSURE) {
            if (min_margin < eng->rules[i].min_enclosure_nm) {
                return drc_report_violation(eng, eng->rules[i].id, outer_layer,
                                            min_margin,
                                            eng->rules[i].min_enclosure_nm,
                                            ix, iy, ix + iw, iy + ih);
            }
        }
    }
    (void)inner_layer;
    return 0;
}

int drc_check_area(drc_lvs_engine_t *eng, int layer,
                    double width, double height)
{
    int i;
    double area = width * height;
    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].layer == layer && eng->rules[i].type == DRC_AREA) {
            if (area < eng->rules[i].min_area_nm2) {
                return drc_report_violation(eng, eng->rules[i].id, layer,
                                            area, eng->rules[i].min_area_nm2,
                                            0.0, 0.0, width, height);
            }
        }
    }
    return 0;
}

int drc_check_density(drc_lvs_engine_t *eng, int layer,
                       double window_w, double window_h,
                       double filled_area)
{
    int i;
    double density = filled_area / (window_w * window_h) * 100.0;
    for (i = 0; i < eng->rule_count; i++) {
        if (eng->rules[i].layer == layer && eng->rules[i].type == DRC_DENSITY) {
            if (density > eng->rules[i].max_density_pct) {
                return drc_report_violation(eng, eng->rules[i].id, layer,
                                            density,
                                            eng->rules[i].max_density_pct,
                                            0.0, 0.0, window_w, window_h);
            }
        }
    }
    return 0;
}

int drc_fix_push_cell(drc_lvs_engine_t *eng, int violation_id,
                       double dx_nm, double dy_nm)
{
    if (violation_id <= 0 || violation_id > eng->violation_count) return -1;
    eng->violations[violation_id - 1].x1_nm += dx_nm;
    eng->violations[violation_id - 1].y1_nm += dy_nm;
    eng->violations[violation_id - 1].x2_nm += dx_nm;
    eng->violations[violation_id - 1].y2_nm += dy_nm;
    eng->violations[violation_id - 1].is_fixed = 1;
    return 0;
}

int drc_fix_widen_wire(drc_lvs_engine_t *eng, int violation_id,
                        double width_nm)
{
    if (violation_id <= 0 || violation_id > eng->violation_count) return -1;
    eng->violations[violation_id - 1].x2_nm =
        eng->violations[violation_id - 1].x1_nm + width_nm;
    eng->violations[violation_id - 1].is_fixed = 1;
    return 0;
}

int drc_fix_reduce_spacing(drc_lvs_engine_t *eng, int violation_id,
                            double factor)
{
    drc_violation_t *v;
    double cx, cy, half_w, half_h;
    if (violation_id <= 0 || violation_id > eng->violation_count) return -1;
    v = &eng->violations[violation_id - 1];
    cx = (v->x1_nm + v->x2_nm) / 2.0;
    cy = (v->y1_nm + v->y2_nm) / 2.0;
    half_w = (v->x2_nm - v->x1_nm) * factor / 2.0;
    half_h = (v->y2_nm - v->y1_nm) * factor / 2.0;
    v->x1_nm = cx - half_w;
    v->y1_nm = cy - half_h;
    v->x2_nm = cx + half_w;
    v->y2_nm = cy + half_h;
    v->is_fixed = 1;
    return 0;
}

int lvs_add_schematic_device(drc_lvs_engine_t *eng, const char *name,
                              const char *type, double w, double l, int fingers)
{
    lvs_device_t *d;
    if (eng->schematic_device_count >= MAX_LVS_DEVICES) return -1;
    d = &eng->schematic_devices[eng->schematic_device_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, sizeof(d->name) - 1);
    strncpy(d->type, type, sizeof(d->type) - 1);
    d->w_nm     = w;
    d->l_nm     = l;
    d->fingers  = fingers;
    d->multiplier = 1;
    d->match_status = LVS_MISMATCH;
    return 0;
}

int lvs_add_schematic_net(drc_lvs_engine_t *eng, const char *name,
                           int device_count)
{
    lvs_net_t *n;
    if (eng->schematic_net_count >= MAX_LVS_NETS) return -1;
    n = &eng->schematic_nets[eng->schematic_net_count++];
    memset(n, 0, sizeof(*n));
    n->net_id = eng->schematic_net_count;
    strncpy(n->net_name, name, sizeof(n->net_name) - 1);
    n->device_count = device_count;
    return 0;
}

int lvs_add_layout_device(drc_lvs_engine_t *eng, const char *name,
                           const char *type, double w, double l, int fingers)
{
    lvs_device_t *d;
    if (eng->layout_device_count >= MAX_LVS_DEVICES) return -1;
    d = &eng->layout_devices[eng->layout_device_count++];
    memset(d, 0, sizeof(*d));
    strncpy(d->name, name, sizeof(d->name) - 1);
    strncpy(d->type, type, sizeof(d->type) - 1);
    d->w_nm     = w;
    d->l_nm     = l;
    d->fingers  = fingers;
    d->multiplier = 1;
    d->match_status = LVS_MISMATCH;
    return 0;
}

int lvs_add_layout_net(drc_lvs_engine_t *eng, const char *name,
                        int device_count)
{
    lvs_net_t *n;
    if (eng->layout_net_count >= MAX_LVS_NETS) return -1;
    n = &eng->layout_nets[eng->layout_net_count++];
    memset(n, 0, sizeof(*n));
    n->net_id = eng->layout_net_count;
    strncpy(n->net_name, name, sizeof(n->net_name) - 1);
    n->device_count = device_count;
    return 0;
}

int lvs_compare(drc_lvs_engine_t *eng)
{
    int i, matched = 0, errors = 0;
    for (i = 0; i < eng->schematic_device_count; i++) {
        int j;
        eng->schematic_devices[i].match_status = LVS_MISSING;
        for (j = 0; j < eng->layout_device_count; j++) {
            if (eng->layout_devices[j].match_status == LVS_MISMATCH &&
                strcmp(eng->schematic_devices[i].name,
                       eng->layout_devices[j].name) == 0) {
                if (fabs(eng->schematic_devices[i].w_nm -
                         eng->layout_devices[j].w_nm) < 1.0 &&
                    fabs(eng->schematic_devices[i].l_nm -
                         eng->layout_devices[j].l_nm) < 1.0) {
                    eng->schematic_devices[i].match_status = LVS_MATCH;
                    eng->layout_devices[j].match_status    = LVS_MATCH;
                    matched++;
                    break;
                }
            }
        }
    }
    for (i = 0; i < eng->layout_device_count; i++) {
        if (eng->layout_devices[i].match_status == LVS_MISMATCH) {
            eng->layout_devices[i].match_status = LVS_EXTRA;
            errors++;
        }
    }
    for (i = 0; i < eng->schematic_device_count; i++) {
        if (eng->schematic_devices[i].match_status == LVS_MISSING) {
            snprintf(eng->schematic_devices[i].mismatch_reason,
                     sizeof(eng->schematic_devices[i].mismatch_reason),
                     "Missing in layout");
            errors++;
        }
    }
    eng->total_lvs_errors = errors;
    return errors;
}

int lvs_match_devices(drc_lvs_engine_t *eng, int s_idx, int l_idx)
{
    if (s_idx < 0 || s_idx >= eng->schematic_device_count) return -1;
    if (l_idx < 0 || l_idx >= eng->layout_device_count) return -1;
    eng->schematic_devices[s_idx].match_status = LVS_MATCH;
    eng->layout_devices[l_idx].match_status    = LVS_MATCH;
    return 0;
}

int lvs_report_mismatches(const drc_lvs_engine_t *eng)
{
    int i, count = 0;
    printf("--- LVS Mismatches ---\n");
    for (i = 0; i < eng->schematic_device_count; i++) {
        if (eng->schematic_devices[i].match_status != LVS_MATCH) {
            printf("  SCH: %s [%s] - %s\n",
                   eng->schematic_devices[i].name,
                   eng->schematic_devices[i].type,
                   eng->schematic_devices[i].mismatch_reason);
            count++;
        }
    }
    for (i = 0; i < eng->layout_device_count; i++) {
        if (eng->layout_devices[i].match_status == LVS_EXTRA) {
            printf("  LAY: %s [%s] - Extra in layout\n",
                   eng->layout_devices[i].name,
                   eng->layout_devices[i].type);
            count++;
        }
    }
    return count;
}

int antenna_check(drc_lvs_engine_t *eng, int net_id,
                   double gate_area_nm2, double max_ratio)
{
    antenna_check_t *ac;
    if (eng->antenna_check_count >= MAX_ANTENNA_NODES) return -1;
    ac = &eng->antenna_checks[eng->antenna_check_count++];
    ac->id           = eng->antenna_check_count;
    ac->net_id       = net_id;
    ac->gate_area_nm2 = gate_area_nm2;
    ac->antenna_ratio = gate_area_nm2 > 0.0 ? (gate_area_nm2 / 1000.0) : 0.0;
    ac->max_antenna_ratio = max_ratio;
    ac->is_violation = ac->antenna_ratio > max_ratio;
    if (ac->is_violation) {
        strncpy(ac->fix_method, "Add protection diode", sizeof(ac->fix_method) - 1);
        eng->total_antenna_errors++;
    }
    return 0;
}

int antenna_fix_add_diode(drc_lvs_engine_t *eng, int check_id)
{
    if (check_id <= 0 || check_id > eng->antenna_check_count) return -1;
    eng->antenna_checks[check_id - 1].antenna_ratio *= 0.1;
    eng->antenna_checks[check_id - 1].is_violation = 0;
    return 0;
}

int antenna_fix_metal_jump(drc_lvs_engine_t *eng, int check_id,
                            int upper_layer)
{
    if (check_id <= 0 || check_id > eng->antenna_check_count) return -1;
    eng->antenna_checks[check_id - 1].antenna_ratio *= 0.3;
    eng->antenna_checks[check_id - 1].is_violation = 0;
    snprintf(eng->antenna_checks[check_id - 1].fix_method,
             sizeof(eng->antenna_checks[check_id - 1].fix_method),
             "Metal jump to layer %d", upper_layer);
    return 0;
}

int erc_check_well_ties(drc_lvs_engine_t *eng, int device_id, int well_net)
{
    erc_violation_t *e;
    if (well_net < 0) {
        if (eng->erc_violation_count >= MAX_ERC_RULES) return -1;
        e = &eng->erc_violations[eng->erc_violation_count++];
        e->type        = ERC_WELL_TIE;
        e->severity    = 2;
        e->device_id   = device_id;
        e->net_id      = well_net;
        snprintf(e->description, sizeof(e->description),
                 "Well tie missing for device %d", device_id);
        eng->total_erc_errors++;
    }
    return 0;
}

int erc_check_floating(drc_lvs_engine_t *eng, int net_id)
{
    erc_violation_t *e;
    int i;
    for (i = 0; i < eng->erc_violation_count; i++) {
        if (eng->erc_violations[i].type == ERC_FLOATING &&
            eng->erc_violations[i].net_id == net_id) return 0;
    }
    if (eng->erc_violation_count >= MAX_ERC_RULES) return -1;
    e = &eng->erc_violations[eng->erc_violation_count++];
    e->type      = ERC_FLOATING;
    e->severity  = 1;
    e->device_id = -1;
    e->net_id    = net_id;
    snprintf(e->description, sizeof(e->description),
             "Floating net detected: net %d", net_id);
    eng->total_erc_errors++;
    return 0;
}

int erc_check_hot_well(drc_lvs_engine_t *eng, int device_id)
{
    erc_violation_t *e;
    if (eng->erc_violation_count >= MAX_ERC_RULES) return -1;
    e = &eng->erc_violations[eng->erc_violation_count++];
    e->type      = ERC_HOT_WELL;
    e->severity  = 2;
    e->device_id = device_id;
    e->net_id    = -1;
    snprintf(e->description, sizeof(e->description),
             "Hot well condition on device %d", device_id);
    eng->total_erc_errors++;
    return 0;
}

int drc_lvs_clean(drc_lvs_engine_t *eng)
{
    int initial = eng->total_drc_errors + eng->total_lvs_errors +
                  eng->total_antenna_errors + eng->total_erc_errors;
    eng->violation_count      = 0;
    eng->total_drc_errors     = 0;
    eng->total_lvs_errors     = 0;
    eng->total_antenna_errors = 0;
    eng->total_erc_errors     = 0;
    return initial;
}

int drc_lvs_signoff(const drc_lvs_engine_t *eng)
{
    return (eng->total_drc_errors == 0 && eng->total_lvs_errors == 0 &&
            eng->total_antenna_errors == 0 && eng->total_erc_errors == 0);
}

void drc_lvs_report(const drc_lvs_engine_t *eng)
{
    int i;
    printf("=== DRC/LVS Report ===\n");
    printf("Rules: %d  Violations: %d\n", eng->rule_count, eng->violation_count);
    printf("DRC errors: %d  LVS errors: %d  Antenna: %d  ERC: %d\n",
           eng->total_drc_errors, eng->total_lvs_errors,
           eng->total_antenna_errors, eng->total_erc_errors);
    printf("Sign-off: %s\n", drc_lvs_signoff(eng) ? "PASS" : "FAIL");
    if (eng->violation_count > 0) {
        printf("Top violations:\n");
        for (i = 0; i < eng->violation_count && i < 10; i++) {
            printf("  [%d] %s (layer %d) %.1f < %.1f @ %.0f,%.0f-%.0f,%.0f %s\n",
                   eng->violations[i].id,
                   eng->violations[i].description,
                   eng->violations[i].layer,
                   eng->violations[i].measured_value,
                   eng->violations[i].required_value,
                   eng->violations[i].x1_nm, eng->violations[i].y1_nm,
                   eng->violations[i].x2_nm, eng->violations[i].y2_nm,
                   eng->violations[i].is_fixed ? "[FIXED]" : "");
        }
    }
    printf("Schematic devices: %d/%d  Layout devices: %d/%d\n",
           eng->schematic_device_count, MAX_LVS_DEVICES,
           eng->layout_device_count, MAX_LVS_DEVICES);
}

void drc_violation_report(const drc_violation_t *v)
{
    printf("DRC Violation #%d: %s\n  Layer: %d  Measured: %.1f  Required: %.1f\n"
           "  BBox: (%.0f,%.0f) - (%.0f,%.0f)  Fixed: %d\n",
           v->id, v->description, v->layer,
           v->measured_value, v->required_value,
           v->x1_nm, v->y1_nm, v->x2_nm, v->y2_nm, v->is_fixed);
}

void lvs_device_report(const lvs_device_t *d)
{
    printf("LVS Device: %s [%s]  W=%.1f L=%.1f F=%d  Status: %d\n",
           d->name, d->type, d->w_nm, d->l_nm, d->fingers, d->match_status);
}

void antenna_report(const antenna_check_t *a)
{
    printf("Antenna #%d: net %d ratio %.2f/%.2f %s  Fix: %s\n",
           a->id, a->net_id, a->antenna_ratio,
           a->max_antenna_ratio,
           a->is_violation ? "VIOLATION" : "OK", a->fix_method);
}

double drc_rect_distance(double x1a, double y1a, double x2a, double y2a,
                          double x1b, double y1b, double x2b, double y2b)
{
    double dx = 0.0, dy = 0.0;
    if (x1a > x2b) dx = x1a - x2b;
    else if (x1b > x2a) dx = x1b - x2a;
    if (y1a > y2b) dy = y1a - y2b;
    else if (y1b > y2a) dy = y1b - y2a;
    return sqrt(dx * dx + dy * dy);
}

int drc_rect_overlap(double x1a, double y1a, double x2a, double y2a,
                      double x1b, double y1b, double x2b, double y2b)
{
    if (x1a >= x2b || x1b >= x2a) return 0;
    if (y1a >= y2b || y1b >= y2a) return 0;
    return 1;
}
