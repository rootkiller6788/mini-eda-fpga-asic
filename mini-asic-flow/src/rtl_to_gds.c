#include "rtl_to_gds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void asic_flow_init(asic_flow_t *flow, int tech_node_nm, int metal_layers)
{
    int i;
    memset(flow, 0, sizeof(*flow));
    flow->tech_node_nm    = tech_node_nm;
    flow->metal_layers    = metal_layers;
    flow->current_stage   = 0;
    flow->total_stages    = ASIC_STAGE_COUNT;

    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        flow->stages[i].stage_index  = i;
        flow->stages[i].stage_id     = (asic_stage_t)i;
        flow->stages[i].completed    = 0;
        flow->stages[i].checkpoint_count = 0;
    }
}

void asic_flow_set_targets(asic_flow_t *flow, double freq_mhz,
                           double area_um2, double power_mw)
{
    flow->target_freq_mhz  = freq_mhz;
    flow->target_area_um2  = area_um2;
    flow->target_power_mw  = power_mw;
}

int asic_flow_advance_stage(asic_flow_t *flow, asic_stage_t stage,
                            const design_milestone_t *milestone)
{
    if (stage < 0 || stage >= ASIC_STAGE_COUNT) return -1;
    if (milestone) {
        flow->stages[stage].milestone = *milestone;
    }
    flow->stages[stage].completed = 1;
    flow->current_stage = stage + 1;
    return 0;
}

int asic_flow_add_checkpoint(asic_flow_t *flow, asic_stage_t stage,
                             const design_checkpoint_t *cp)
{
    asic_flow_stage_t *s;
    if (stage < 0 || stage >= ASIC_STAGE_COUNT) return -1;
    s = &flow->stages[stage];
    if (s->checkpoint_count >= MAX_CHECKPOINTS) return -1;
    s->checkpoints[s->checkpoint_count++] = *cp;
    return 0;
}

int asic_flow_verify_stage(const asic_flow_t *flow, asic_stage_t stage)
{
    const asic_flow_stage_t *s;
    int i, fails = 0;
    if (stage < 0 || stage >= ASIC_STAGE_COUNT) return -1;
    s = &flow->stages[stage];
    if (!s->completed) return -1;
    for (i = 0; i < s->checkpoint_count; i++) {
        if (s->checkpoints[i].status == CHECKPOINT_FAIL) fails++;
    }
    return fails;
}

int asic_flow_is_signed_off(const asic_flow_t *flow)
{
    return flow->stages[ASIC_STAGE_SIGNOFF].completed != 0;
}

const char *asic_stage_name(asic_stage_t stage)
{
    static const char *names[] = {
        "RTL Design", "Synthesis", "DFT",
        "Floorplan", "Placement", "CTS",
        "Routing", "RC Extraction", "STA",
        "SignOff"
    };
    if (stage < 0 || stage >= ASIC_STAGE_COUNT) return "Unknown";
    return names[stage];
}

const design_milestone_t *asic_flow_get_milestone(const asic_flow_t *flow,
                                                   asic_stage_t stage)
{
    if (stage < 0 || stage >= ASIC_STAGE_COUNT) return NULL;
    return &flow->stages[stage].milestone;
}

void asic_flow_report(const asic_flow_t *flow)
{
    int i;
    printf("=== ASIC Flow Report ===\n");
    printf("Technology: %d nm, %d metal layers\n",
           flow->tech_node_nm, flow->metal_layers);
    printf("Targets: %.1f MHz, %.1f um2, %.2f mW\n",
           flow->target_freq_mhz, flow->target_area_um2,
           flow->target_power_mw);
    printf("Current stage: %d / %d\n\n", flow->current_stage,
           flow->total_stages);

    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        const asic_flow_stage_t *s = &flow->stages[i];
        printf("  [%c] Stage %d: %s\n",
               s->completed ? 'X' : ' ', i, asic_stage_name(s->stage_id));
        if (s->completed) {
            printf("       Cells: %d  Nets: %d  Area: %.1f um2\n",
                   s->milestone.total_cells, s->milestone.total_nets,
                   s->milestone.area_um2);
            printf("       Power: %.2f mW  Slack: %.3f/%.3f ns\n",
                   s->milestone.power_mw,
                   s->milestone.max_slack_ns, s->milestone.min_slack_ns);
            printf("       DRC: %d  LVS: %d  Setup: %d  Hold: %d\n",
                   s->milestone.drc_errors, s->milestone.lvs_errors,
                   s->milestone.setup_violations,
                   s->milestone.hold_violations);
        }
    }
}

int asic_flow_write_checkpoints(const asic_flow_t *flow, const char *filename)
{
    FILE *f = fopen(filename, "w");
    int i, j;
    if (!f) return -1;
    fprintf(f, "CHECKPOINTS %d %d\n", flow->tech_node_nm, flow->metal_layers);
    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        const asic_flow_stage_t *s = &flow->stages[i];
        fprintf(f, "STAGE %d %d\n", i, s->completed);
        for (j = 0; j < s->checkpoint_count; j++) {
            const design_checkpoint_t *cp = &s->checkpoints[j];
            fprintf(f, "CP %d %s %d %g %g\n",
                    cp->checkpoint_id, cp->name, cp->status,
                    cp->metric_value, cp->metric_target);
        }
    }
    fclose(f);
    return 0;
}

int asic_flow_read_checkpoints(asic_flow_t *flow, const char *filename)
{
    FILE *f = fopen(filename, "r");
    char header[32];
    int i, tech, metals, stage_idx, completed;
    if (!f) return -1;
    fscanf(f, "%s %d %d", header, &tech, &metals);
    flow->tech_node_nm = tech;
    flow->metal_layers = metals;
    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        fscanf(f, "%s %d %d", header, &stage_idx, &completed);
        flow->stages[stage_idx].completed = completed;
        flow->stages[stage_idx].checkpoint_count = 0;
    }
    fclose(f);
    return 0;
}

double asic_flow_total_timing_slack(const asic_flow_t *flow)
{
    double total = 0.0;
    int i;
    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        if (flow->stages[i].completed) {
            total += flow->stages[i].milestone.max_slack_ns;
        }
    }
    return total;
}

int asic_flow_total_drc_errors(const asic_flow_t *flow)
{
    int total = 0, i;
    for (i = 0; i < ASIC_STAGE_COUNT; i++) {
        if (flow->stages[i].completed) {
            total += flow->stages[i].milestone.drc_errors;
        }
    }
    return total;
}
