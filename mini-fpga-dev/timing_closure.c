#include "timing_closure.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* =======================================================
 * Timing Arc Implementation
 * ======================================================= */

void timing_arc_init(TimingArc *arc, const char *name)
{
    memset(arc, 0, sizeof(TimingArc));
    strncpy(arc->arc_name, name, sizeof(arc->arc_name) - 1);
}

void timing_arc_set_comb(TimingArc *arc, double rise_delay, double fall_delay)
{
    arc->is_combinational = 1;
    arc->is_setup = 0;
    arc->is_hold = 0;
    arc->delay_rise_ns = rise_delay;
    arc->delay_fall_ns = fall_delay;
    arc->slew_rise_ns = rise_delay * 0.2;
    arc->slew_fall_ns = fall_delay * 0.2;
}

void timing_arc_set_seq(TimingArc *arc, double setup, double hold)
{
    arc->is_combinational = 0;
    arc->is_setup = 1;
    arc->is_hold = 1;
    arc->setup_limit_ns = setup;
    arc->hold_limit_ns = hold;
    arc->delay_rise_ns = 0.0;
    arc->delay_fall_ns = 0.0;
}

/* =======================================================
 * Timing Node Implementation
 * ======================================================= */

void timing_node_init(TimingNode *node, const char *name)
{
    memset(node, 0, sizeof(TimingNode));
    strncpy(node->node_name, name, sizeof(node->node_name) - 1);
    node->slack_ns = 999.9;  /* unconstrained */
}

/* =======================================================
 * Timing Path Implementation
 * ======================================================= */

void timing_path_init(TimingPath *tp, double clk_period)
{
    memset(tp, 0, sizeof(TimingPath));
    tp->clock_period_ns = clk_period;
    tp->corner = CORNER_SLOW;
}

void timing_path_add_arc(TimingPath *tp, const TimingArc *arc)
{
    if (tp->num_arcs < 128) {
        tp->arcs[tp->num_arcs] = *arc;
        tp->num_arcs++;

        if (arc->is_combinational) {
            tp->logic_delay_ns += arc->delay_rise_ns; /* pessimistic */
            tp->route_delay_ns += arc->delay_rise_ns * 0.3;
            tp->num_stages++;
        }
    }
    tp->total_delay_ns = tp->logic_delay_ns + tp->route_delay_ns;
}

int timing_path_check_setup(const TimingPath *tp)
{
    double clk_arrival = tp->clock_period_ns;
    double data_arrival = tp->total_delay_ns;
    double slack = clk_arrival - data_arrival;
    ((TimingPath *)tp)->slack_ns = slack;
    return (slack >= SETUP_SLACK_MIN_NS) ? 0 : -1;
}

int timing_path_check_hold(const TimingPath *tp)
{
    double data_arrival = tp->total_delay_ns;
    double slack = data_arrival - tp->endpoint.clock_uncertainty_ns;
    ((TimingPath *)tp)->slack_ns = slack;
    return (slack >= HOLD_SLACK_MIN_NS) ? 0 : -1;
}

void timing_path_report(const TimingPath *tp)
{
    printf("=== Timing Path Report ===\n");
    printf("  Startpoint:      %s\n", tp->startpoint.node_name);
    printf("  Endpoint:        %s\n", tp->endpoint.node_name);
    printf("  Clock period:    %.3f ns\n", tp->clock_period_ns);
    printf("  Total delay:     %.3f ns\n", tp->total_delay_ns);
    printf("  Logic delay:     %.3f ns\n", tp->logic_delay_ns);
    printf("  Route delay:     %.3f ns\n", tp->route_delay_ns);
    printf("  Logic stages:    %d\n", tp->num_stages);
    printf("  Slack:           %.3f ns %s\n", tp->slack_ns,
           tp->slack_ns >= 0.0 ? "(MET)" : "(VIOLATED)");
    printf("  Corner:          %s\n",
           tp->corner == CORNER_SLOW ? "Slow" :
           tp->corner == CORNER_FAST ? "Fast" : "Typical");
    if (tp->is_multicycle) {
        printf("  Multicycle:      %d\n", tp->multicycle_value);
    }
}

/* =======================================================
 * Setup/Hold Check Implementation
 * ======================================================= */

void sh_check_init(SetupHoldCheck *sh, double setup_req, double hold_req)
{
    memset(sh, 0, sizeof(SetupHoldCheck));
    sh->setup_required_ns = setup_req;
    sh->hold_required_ns = hold_req;
}

int sh_check_setup(const SetupHoldCheck *sh, double *slack_out)
{
    double arrival = sh->data_path_delay_ns + sh->clock_skew_ns +
                     sh->clock_jitter_ns;
    double required = sh->clock_path_delay_ns + sh->setup_required_ns;
    double slack = required - arrival;

    ((SetupHoldCheck *)sh)->setup_slack_ns = slack;
    if (slack_out) *slack_out = slack;
    return (slack >= 0.0) ? 0 : -1;
}

int sh_check_hold(const SetupHoldCheck *sh, double *slack_out)
{
    double arrival = sh->data_path_delay_ns + sh->clock_path_delay_ns;
    double required = sh->hold_required_ns + sh->clock_skew_ns;
    double slack = arrival - required;

    ((SetupHoldCheck *)sh)->hold_slack_ns = slack;
    if (slack_out) *slack_out = slack;
    return (slack >= 0.0) ? 0 : -1;
}

/* =======================================================
 * Pipeline Inserter Implementation
 * ======================================================= */

void pipeline_init(PipelineInserter *pi, double target_delay)
{
    memset(pi, 0, sizeof(PipelineInserter));
    pi->target_delay_ns = target_delay;
}

int pipeline_compute_stages(PipelineInserter *pi, double total_delay,
                             int logic_levels)
{
    if (total_delay <= 0.0 || logic_levels <= 0) return -1;

    pi->original_delay_ns = total_delay;
    int stages = (int)ceil(total_delay / pi->target_delay_ns);
    if (stages < 1) stages = 1;
    if (stages > MAX_PIPELINE_STAGES) stages = MAX_PIPELINE_STAGES;

    pi->stage_count = (uint8_t)stages;
    pi->num_stages_inserted = stages - 1;

    /* Distribute logic levels evenly across stages */
    int levels_per_stage = logic_levels / stages;
    int remainder = logic_levels % stages;
    int pos = 0;
    for (int i = 0; i < stages - 1; i++) {
        pi->stage_position[i] = (uint8_t)(pos + levels_per_stage +
                                           (i < remainder ? 1 : 0));
        pos += levels_per_stage + (i < remainder ? 1 : 0);
    }

    pi->new_delay_ns = total_delay / stages + (stages - 1) * 0.05;
    pi->pipelined_netlist_size = logic_levels + pi->num_stages_inserted;
    return pi->num_stages_inserted;
}

int pipeline_insert_reg(PipelineInserter *pi, int at_logic_level)
{
    (void)pi;
    (void)at_logic_level;
    /* Model register insertion: adds setup time to path, reduces per-stage delay */
    pi->num_stages_inserted++;
    pi->new_delay_ns += 0.05; /* small register overhead */
    return 0;
}

int pipeline_get_throughput(const PipelineInserter *pi, double *tp_mhz)
{
    if (!tp_mhz) return -1;
    if (pi->target_delay_ns <= 0.0) return -1;
    *tp_mhz = 1000.0 / pi->target_delay_ns;
    return 0;
}

/* =======================================================
 * Retiming Engine Implementation
 * ======================================================= */

void retiming_init(RetimingEngine *re)
{
    memset(re, 0, sizeof(RetimingEngine));
    re->max_reg_moves = MAX_REG_MOVE_DIST;
    re->weight_forward = 1.0;
    re->weight_backward = 0.8;
}

int retiming_move_forward(RetimingEngine *re, double logic_delay,
                           double reg_setup)
{
    /* Move register forward: split logic, add register at halfway point */
    if (re->num_regs_forward >= re->max_reg_moves) return -1;

    double half_delay = logic_delay / 2.0;
    double slack = re->delay_before_move_ns - (half_delay + reg_setup);

    re->num_regs_forward++;
    re->delay_before_move_ns = logic_delay;
    re->delay_after_move_ns = half_delay + reg_setup;
    return (slack >= 0.0) ? 0 : -1;
}

int retiming_move_backward(RetimingEngine *re, double logic_delay,
                            double reg_delay)
{
    if (re->num_regs_backward >= re->max_reg_moves) return -1;

    re->num_regs_backward++;
    re->delay_before_move_ns = logic_delay;
    re->delay_after_move_ns = logic_delay - reg_delay * 0.3;
    return 0;
}

int retiming_balance(RetimingEngine *re, double *delays, int num_stages,
                      double clk_period)
{
    if (!delays || num_stages <= 1) return -1;

    /* Simple balancing: redistribute registers to equalize stage delays */
    double total = 0.0;
    for (int i = 0; i < num_stages; i++) total += delays[i];

    double target = total / (double)num_stages;
    if (target > clk_period) {
        /* Need more pipelining - move registers to reduce worst stage */
        double worst = delays[0];
        for (int i = 1; i < num_stages; i++) {
            if (delays[i] > worst) worst = delays[i];
        }
        int splits = (int)ceil(worst / clk_period) - 1;
        re->num_regs_forward += splits;
        re->delay_before_move_ns = worst;
        re->delay_after_move_ns = worst / (double)(splits + 1);
        return splits;
    }
    return 0;
}

/* =======================================================
 * Logic Duplication Implementation
 * ======================================================= */

void logic_dup_init(LogicDuplicator *ld, int fanout_threshold)
{
    memset(ld, 0, sizeof(LogicDuplicator));
    ld->fanout_threshold = fanout_threshold;
    ld->max_duplication_factor = 8;
}

int logic_dup_analyze(LogicDuplicator *ld, int *fanout_counts, int num_nets)
{
    if (!fanout_counts) return -1;

    ld->num_original_cells = num_nets;
    int needs_dup = 0;
    ld->num_clusters = 0;

    for (int i = 0; i < num_nets; i++) {
        if (fanout_counts[i] > ld->fanout_threshold) {
            needs_dup++;
            if (ld->num_clusters < 64) {
                ld->duplicate_clusters[ld->num_clusters] = i;
                ld->num_clusters++;
            }
        }
    }

    /* Estimate: each duplication reduces fanout delay by ~15% */
    ld->delay_before_ns = num_nets * 0.25 * (double)ld->fanout_threshold;
    ld->delay_after_ns = ld->delay_before_ns *
                         (1.0 - 0.15 * (double)needs_dup / (double)num_nets);
    ld->num_duplicated_cells = needs_dup;
    return needs_dup;
}

int logic_dup_apply(LogicDuplicator *ld, int net_id)
{
    if (net_id < 0 || net_id >= ld->num_original_cells) return -1;
    /* Check if already duplicated */
    for (int i = 0; i < ld->num_clusters; i++) {
        if (ld->duplicate_clusters[i] == net_id) return 0;
    }
    /* Mark for duplication */
    if (ld->num_clusters < 64) {
        ld->duplicate_clusters[ld->num_clusters] = net_id;
        ld->num_clusters++;
        ld->num_duplicated_cells++;
    }
    return 0;
}

int logic_dup_estimate_improvement(const LogicDuplicator *ld,
                                    double *improvement_pct)
{
    if (!improvement_pct) return -1;
    if (ld->delay_before_ns <= 0.0) { *improvement_pct = 0.0; return 0; }
    *improvement_pct = (ld->delay_before_ns - ld->delay_after_ns) /
                       ld->delay_before_ns * 100.0;
    return 0;
}

/* =======================================================
 * Clock Domain Crossing (CDC) Implementation
 * ======================================================= */

void cdc_init(ClockCrossing *cdc, double src_period, double dst_period)
{
    memset(cdc, 0, sizeof(ClockCrossing));
    cdc->src_clk_period_ns = src_period;
    cdc->dst_clk_period_ns = dst_period;
    cdc->need_synchronizer = (src_period != dst_period);
    cdc->sync_stages = 2; /* standard 2-stage synchronizer */
}

int cdc_check_safe(const ClockCrossing *cdc)
{
    /* Safe if source rate <= destination rate, or async FIFO present */
    if (cdc->src_clk_period_ns >= cdc->dst_clk_period_ns) {
        return 0; /* safe: source slower */
    }
    /* Source faster than destination: need synchronization */
    if (cdc->need_synchronizer && cdc->sync_stages >= 2) {
        return 0; /* OK with synchronizer */
    }
    return -1; /* potential data loss */
}

void cdc_recommend_fifo_depth(ClockCrossing *cdc, double data_rate_mbps)
{
    cdc->data_rate_mbps = data_rate_mbps;
    /* Depth = max outstanding writes during slow clock cycle */
    double ratio = cdc->src_clk_period_ns / cdc->dst_clk_period_ns;
    if (ratio > 0.0) {
        int depth = (int)ceil(ratio * 4.0); /* 4x safety margin */
        if (depth < 8) depth = 8;
        cdc->async_fifo_depth = depth;
    } else {
        cdc->async_fifo_depth = 16;
    }
}
