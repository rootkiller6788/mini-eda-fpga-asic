#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "timing_closure.h"

static void print_section(const char *title)
{
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n");
}

/* -------------------------------------------------------
 * Demo 1: Basic Timing Arc & Path Analysis
 * ------------------------------------------------------- */
static void demo_timing_arcs(void)
{
    print_section("Demo 1: Timing Arc & Path Analysis");

    /* Create timing arcs for a simple path */
    TimingArc lut_arc, route_arc1, route_arc2, ff_setup_arc;

    timing_arc_init(&lut_arc, "LUT6: A1->O6");
    timing_arc_set_comb(&lut_arc, 0.35, 0.32);

    timing_arc_init(&route_arc1, "ROUTE: single");
    timing_arc_set_comb(&route_arc1, 0.15, 0.13);

    timing_arc_init(&route_arc2, "ROUTE: double");
    timing_arc_set_comb(&route_arc2, 0.28, 0.25);

    timing_arc_init(&ff_setup_arc, "FF: D->setup");
    timing_arc_set_seq(&ff_setup_arc, 0.12, 0.05);

    /* Build a timing path */
    TimingPath path;
    timing_path_init(&path, 10.0);  /* 100 MHz clock */
    strcpy(path.startpoint.node_name, "src_ff/Q");
    strcpy(path.endpoint.node_name, "dst_ff/D");
    path.corner = CORNER_SLOW;

    timing_path_add_arc(&path, &route_arc1);   /* Q -> LUT input */
    timing_path_add_arc(&path, &lut_arc);      /* LUT delay */
    timing_path_add_arc(&path, &route_arc2);   /* LUT -> FF */
    timing_path_add_arc(&path, &ff_setup_arc); /* FF setup */

    printf("  Path: %s -> %s\n", path.startpoint.node_name,
           path.endpoint.node_name);
    printf("  Logic delay:  %.3f ns\n", path.logic_delay_ns);
    printf("  Route delay:  %.3f ns\n", path.route_delay_ns);
    printf("  Total delay:  %.3f ns\n", path.total_delay_ns);
    printf("  Logic levels: %d\n", path.num_stages);

    int met = timing_path_check_setup(&path);
    printf("  Setup check:  %s (slack=%.3f ns)\n",
           met == 0 ? "MET" : "VIOLATED", path.slack_ns);

    timing_path_report(&path);
}

/* -------------------------------------------------------
 * Demo 2: Setup/Hold Analysis
 * ------------------------------------------------------- */
static void demo_setup_hold(void)
{
    print_section("Demo 2: Setup/Hold Analysis");

    SetupHoldCheck sh;
    sh_check_init(&sh, 0.15, 0.03);  /* FF setup=0.15ns, hold=0.03ns */
    sh.data_path_delay_ns = 8.5;
    sh.clock_path_delay_ns = 10.0;
    sh.clock_skew_ns = 0.15;
    sh.clock_jitter_ns = 0.08;

    printf("  FF Setup req: %.3f ns\n", sh.setup_required_ns);
    printf("  FF Hold  req: %.3f ns\n", sh.hold_required_ns);
    printf("  Data delay:   %.3f ns\n", sh.data_path_delay_ns);
    printf("  Clock delay:  %.3f ns\n", sh.clock_path_delay_ns);
    printf("  Clock skew:   %.3f ns\n", sh.clock_skew_ns);
    printf("  Clock jitter: %.3f ns\n", sh.clock_jitter_ns);

    double setup_slack, hold_slack;
    int setup_ok = sh_check_setup(&sh, &setup_slack);
    int hold_ok = sh_check_hold(&sh, &hold_slack);

    printf("  Setup slack:  %.3f ns [%s]\n", setup_slack,
           setup_ok == 0 ? "MET" : "VIOLATED");
    printf("  Hold slack:   %.3f ns [%s]\n", hold_slack,
           hold_ok == 0 ? "MET" : "VIOLATED");

    /* Tight scenario */
    SetupHoldCheck sh2;
    sh_check_init(&sh2, 0.15, 0.05);
    sh2.data_path_delay_ns = 9.6;  /* very tight */
    sh2.clock_path_delay_ns = 10.0;
    sh2.clock_skew_ns = 0.30;
    double tight_slack;
    int tight_ok = sh_check_setup(&sh2, &tight_slack);
    printf("\n  Tight scenario:\n");
    printf("  Setup slack:  %.3f ns [%s]\n", tight_slack,
           tight_ok == 0 ? "MET" : "VIOLATED");
}

/* -------------------------------------------------------
 * Demo 3: Pipeline Insertion
 * ------------------------------------------------------- */
static void demo_pipeline_insertion(void)
{
    print_section("Demo 3: Pipeline Insertion");

    /* Multiple scenarios with different target delays */
    double scenarios[][3] = {
        { 15.0, 20, 5.0 },   /* 15ns path, 20 levels, target 5ns */
        { 25.0, 30, 4.0 },   /* 25ns path, 30 levels, target 4ns */
        { 8.0,  12, 3.0 },   /* 8ns path,  12 levels, target 3ns */
        { 32.0, 40, 8.0 },   /* 32ns path, 40 levels, target 8ns */
    };

    for (int s = 0; s < 4; s++) {
        double delay = scenarios[s][0];
        int levels   = (int)scenarios[s][1];
        double target = scenarios[s][2];

        PipelineInserter pi;
        pipeline_init(&pi, target);

        int stages = pipeline_compute_stages(&pi, delay, levels);

        printf("\n  Scenario %d: %.1fns path, %d logic levels, target %.1fns/stage\n",
               s + 1, delay, levels, target);
        printf("    Pipeline stages:  %d (inserted %d registers)\n",
               pi.stage_count, stages);
        printf("    Original delay:   %.3f ns\n", pi.original_delay_ns);
        printf("    New per-stage:    %.3f ns\n", pi.new_delay_ns);
        printf("    Netlist growth:   %d -> %d cells\n",
               levels, pi.pipelined_netlist_size);

        double tp;
        pipeline_get_throughput(&pi, &tp);
        printf("    Throughput:       %.1f MHz\n", tp);

        printf("    Stage positions:  [");
        for (int i = 0; i < pi.stage_count - 1 && i < MAX_PIPELINE_STAGES; i++) {
            printf("%d%s", pi.stage_position[i],
                   (i < pi.stage_count - 2) ? ", " : "");
        }
        printf("]\n");
    }
}

/* -------------------------------------------------------
 * Demo 4: Retiming
 * ------------------------------------------------------- */
static void demo_retiming(void)
{
    print_section("Demo 4: Retiming");

    RetimingEngine re;
    retiming_init(&re);

    printf("  Max reg moves: %d\n", re.max_reg_moves);
    printf("  Forward weight: %.1f, Backward weight: %.1f\n",
           re.weight_forward, re.weight_backward);

    /* Forward retiming: long combinational chain */
    printf("\n  Forward retiming (10ns chain):\n");
    for (int i = 0; i < 5; i++) {
        double delay = 2.0 + (double)i * 2.0;
        int ret = retiming_move_forward(&re, delay, 0.15);
        printf("    Move %d: delay %.1fns -> %.3fns  %s\n",
               i + 1, delay, re.delay_after_move_ns,
               ret == 0 ? "OK" : "NO_MORE_MOVES");
    }

    printf("  Forward moves: %d\n", re.num_regs_forward);

    /* Backward retiming */
    RetimingEngine re2;
    retiming_init(&re2);
    printf("\n  Backward retiming:\n");
    for (int i = 0; i < 3; i++) {
        double delay = 1.5 + (double)i * 1.0;
        int ret = retiming_move_backward(&re2, delay, 0.10);
        printf("    Move %d: delay %.1fns -> %.3fns  %s\n",
               i + 1, delay, re2.delay_after_move_ns,
               ret == 0 ? "OK" : "NO_MORE_MOVES");
    }

    /* Balance a multi-stage path */
    printf("\n  Stage balancing:\n");
    double delays[] = { 3.2, 1.5, 4.8, 2.1, 3.9, 1.2, 4.5, 2.8 };
    int n_stages = 8;
    double clk_period = 4.0;
    printf("  Input: [");
    for (int i = 0; i < n_stages; i++) {
        printf("%.1f%s", delays[i], (i < n_stages - 1) ? ", " : "");
    }
    printf("]  (clk period = %.1fns)\n", clk_period);

    int splits = retiming_balance(&re, delays, n_stages, clk_period);
    printf("  Splits needed: %d\n", splits);
}

/* -------------------------------------------------------
 * Demo 5: Logic Duplication for High Fanout
 * ------------------------------------------------------- */
static void demo_logic_duplication(void)
{
    print_section("Demo 5: Logic Duplication for High Fanout");

    /* Test scenarios with varying fanout distributions */
    int fanout_distributions[][10] = {
        { 2, 3, 5, 8, 12, 4, 3, 2, 1, 1 },     /* moderate */
        { 5, 8, 15, 22, 30, 12, 6, 4, 3, 2 },    /* high */
        { 1, 1, 2, 2, 3, 3, 4, 4, 5, 1 },        /* low */
        { 16, 32, 8, 64, 4, 128, 2, 1, 1, 1 },   /* extreme */
    };
    int thresholds[] = { 10, 10, 6, 8 };
    const char *labels[] = { "Moderate", "High", "Low", "Extreme" };

    for (int s = 0; s < 4; s++) {
        LogicDuplicator ld;
        logic_dup_init(&ld, thresholds[s]);

        int to_dup = logic_dup_analyze(&ld, fanout_distributions[s], 10);

        double improvement;
        logic_dup_estimate_improvement(&ld, &improvement);

        printf("\n  %s fanout (threshold=%d):\n", labels[s], thresholds[s]);
        printf("    Nets needing duplication: %d\n", to_dup);
        printf("    Est. delay before:  %.3f ns\n", ld.delay_before_ns);
        printf("    Est. delay after:   %.3f ns\n", ld.delay_after_ns);
        printf("    Improvement:        %.1f%%\n", improvement);
        printf("    Duplicated cells:   %d\n", ld.num_duplicated_cells);
    }
}

/* -------------------------------------------------------
 * Demo 6: Clock Domain Crossing
 * ------------------------------------------------------- */
static void demo_cdc(void)
{
    print_section("Demo 6: Clock Domain Crossing (CDC)");

    struct {
        double src_mhz;
        double dst_mhz;
        double data_rate;
    } test_cases[] = {
        { 100.0, 50.0,  200.0 },
        { 50.0,  100.0, 400.0 },
        { 125.0, 125.0, 500.0 },
        { 200.0, 25.0,  800.0 },
    };

    for (int i = 0; i < 4; i++) {
        double src_period = 1000.0 / test_cases[i].src_mhz;
        double dst_period = 1000.0 / test_cases[i].dst_mhz;

        ClockCrossing cdc;
        cdc_init(&cdc, src_period, dst_period);

        printf("\n  Case %d: %.0f MHz -> %.0f MHz @ %.0f Mbps\n",
               i + 1, test_cases[i].src_mhz, test_cases[i].dst_mhz,
               test_cases[i].data_rate);

        printf("    Src period:  %.3f ns\n", cdc.src_clk_period_ns);
        printf("    Dst period:  %.3f ns\n", cdc.dst_clk_period_ns);
        printf("    Sync needed: %s\n",
               cdc.need_synchronizer ? "YES" : "NO");
        printf("    Sync stages: %d\n", cdc.sync_stages);

        int safe = cdc_check_safe(&cdc);
        printf("    CDC safe:    %s\n", safe == 0 ? "YES" : "POTENTIAL ISSUE");

        cdc_recommend_fifo_depth(&cdc, test_cases[i].data_rate);
        printf("    FIFO depth:  %d\n", cdc.async_fifo_depth);
    }
}

/* -------------------------------------------------------
 * Demo 7: Full Timing Closure Flow
 * ------------------------------------------------------- */
static void demo_timing_closure_flow(void)
{
    print_section("Demo 7: Full Timing Closure Flow");

    printf("  Simulating a timing closure iteration...\n\n");

    /* Step 1: Analyze timing */
    printf("  Step 1: Timing Analysis\n");
    TimingPath longest_path;
    timing_path_init(&longest_path, 10.0);  /* 100 MHz target */
    strcpy(longest_path.startpoint.node_name, "ff_a/Q");
    strcpy(longest_path.endpoint.node_name, "ff_z/D");

    /* Add 20 levels of logic */
    for (int i = 0; i < 20; i++) {
        TimingArc la;
        timing_arc_init(&la, "LUT");
        timing_arc_set_comb(&la, 0.30, 0.28);
        timing_path_add_arc(&longest_path, &la);
    }
    timing_path_check_setup(&longest_path);
    printf("    Path delay: %.3f ns, Slack: %.3f ns [%s]\n",
           longest_path.total_delay_ns, longest_path.slack_ns,
           longest_path.slack_ns >= 0.0 ? "MET" : "VIOLATED");

    /* Step 2: Pipeline insertion */
    printf("\n  Step 2: Pipeline Insertion\n");
    PipelineInserter pi;
    pipeline_init(&pi, 3.0);  /* target 3ns per stage */
    int inserted = pipeline_compute_stages(&pi, longest_path.total_delay_ns,
                                            longest_path.num_stages);
    printf("    Stages after pipelining: %d (inserted %d regs)\n",
           pi.stage_count, inserted);
    printf("    New per-stage delay: %.3f ns\n", pi.new_delay_ns);

    /* Step 3: Logic duplication for high fanout nets */
    printf("\n  Step 3: Logic Duplication\n");
    LogicDuplicator ld;
    logic_dup_init(&ld, 16);
    int fanout[] = { 4, 8, 12, 24, 6, 18, 32, 2, 5, 14 };
    int needs_dup = logic_dup_analyze(&ld, fanout, 10);
    printf("    Nets to duplicate: %d\n", needs_dup);

    /* Step 4: Retiming for balance */
    printf("\n  Step 4: Retiming for Balance\n");
    RetimingEngine re;
    retiming_init(&re);
    double stage_delays[] = { 2.8, 3.1, 2.5, 2.9, 3.0, 2.7, 3.2 };
    int retiming_result = retiming_balance(&re, stage_delays, 7, 3.0);
    printf("    Worst stage adjusted: %s\n",
           retiming_result > 0 ? "YES (split)" : "NO (balanced)");

    /* Summary */
    printf("\n  === Timing Closure Summary ===\n");
    printf("  Original slack:  %.3f ns (VIOLATED)\n", longest_path.slack_ns);
    printf("  Pipeline stages: %d\n", pi.stage_count);
    printf("  Duplications:    %d\n", needs_dup);
    printf("  Retiming splits: %d\n", retiming_result);
    printf("  Final per-stage: %.3f ns\n", pi.new_delay_ns);
    printf("  Expected Fmax:   %.1f MHz\n",
           1000.0 / (pi.new_delay_ns + 0.10));

    double expected_slack = 10.0 - (pi.new_delay_ns + 0.10);
    printf("  Expected slack:  %.3f ns [%s]\n",
           expected_slack, expected_slack >= 0 ? "MET" : "VIOLATED");
}

int main(void)
{
    printf("==================================================\n");
    printf("  mini-fpga-dev: Timing Closure Demo\n");
    printf("  Version: %s\n", TIMING_VERSION);
    printf("==================================================\n");

    demo_timing_arcs();
    demo_setup_hold();
    demo_pipeline_insertion();
    demo_retiming();
    demo_logic_duplication();
    demo_cdc();
    demo_timing_closure_flow();

    printf("\n==================================================\n");
    printf("  All timing closure demos completed.\n");
    printf("==================================================\n");
    return 0;
}
