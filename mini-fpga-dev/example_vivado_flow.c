#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vivado_flow.h"

static void demo_synthesis(void)
{
    printf("\n--- Synthesis Demo ---\n");

    SynthesisEngine syn;
    synthesis_init(&syn, "xc7k325tffg900-2");

    synthesis_add_source(&syn, "top.vhd");
    synthesis_add_source(&syn, "alu.vhd");
    synthesis_add_source(&syn, "control_unit.vhd");
    synthesis_add_source(&syn, "datapath.vhd");
    synthesis_add_source(&syn, "memory_interface.vhd");

    syn.strategy = SYNTH_STRAT_PERF_OPT;
    syn.resource_sharing = 1;
    syn.fsm_extraction = 1;  /* one-hot */

    synthesis_run(&syn);
    synthesis_report(&syn);
}

static void demo_constraints(void)
{
    printf("\n--- XDC Constraints Demo ---\n");

    XdcConstraint clk_constraint, io_constraint, fp_constraint;

    /* Create clock constraint */
    xdc_init(&clk_constraint);
    xdc_create_clock(&clk_constraint, "sys_clk", 10.0,
                     "clk_pin", 0.5);
    printf("  Clock: '%s', period=%.1fns, duty=%.0f%%\n",
           clk_constraint.name, clk_constraint.period_ns,
           clk_constraint.duty_cycle * 100.0);

    /* Create I/O location constraint */
    xdc_init(&io_constraint);
    xdc_set_io_location(&io_constraint, "led[0]", "LVCMOS33", "Y11");
    printf("  I/O:   pin='%s', std='%s', loc='%s'\n",
           io_constraint.name, io_constraint.iostandard,
           io_constraint.target_pin);

    /* Create false path */
    xdc_init(&fp_constraint);
    xdc_set_false_path(&fp_constraint,
                       "async_reg_group_a",
                       "async_reg_group_b",
                       NULL);
    printf("  False path: from='%s' to='%s'\n",
           fp_constraint.from_path, fp_constraint.to_path);

    /* Multicycle path */
    XdcConstraint mc;
    xdc_init(&mc);
    xdc_set_multicycle_path(&mc, "src_ff", "dst_ff", 3, 1);
    printf("  Multicycle: %d-cycle setup from='%s' to='%s'\n",
           mc.multicycle_value, mc.from_path, mc.to_path);
}

static void demo_implementation(void)
{
    printf("\n--- Implementation Demo ---\n");

    ImplementationEngine impl;
    impl_init(&impl);
    impl_set_directive(&impl, IMPL_STAGE_PLACE_DESIGN, 1);
    impl_set_directive(&impl, IMPL_STAGE_ROUTE_DESIGN, 1);

    int ret = impl_run_all(&impl);
    printf("  Run status: %s\n", ret == 0 ? "SUCCESS" : "FAILED");
    printf("  Stages completed: %d\n", impl.num_stages_completed);
    printf("  Est. wirelength:  %.0f\n", impl.place_estimated_wirelength);
    printf("  Est. route delay: %.3f ns\n", impl.route_estimated_delay_ns);

    impl_generate_report(&impl, REPORT_TIMING_SUMMARY);
    impl_generate_report(&impl, REPORT_UTILIZATION);
}

static void demo_full_flow(void)
{
    printf("\n--- Full Vivado Flow Demo ---\n");

    VivadoFlow flow;
    vivado_flow_init(&flow, "my_fpga_project", "xc7k325tffg900-2");

    /* Add constraints */
    XdcConstraint clk;
    xdc_init(&clk);
    xdc_create_clock(&clk, "clk_100MHz", 10.0, "clk_p", 0.5);
    vivado_flow_add_constraint(&flow, &clk);

    XdcConstraint io;
    xdc_init(&io);
    xdc_set_io_location(&io, "uart_tx", "LVCMOS33", "AB22");
    vivado_flow_add_constraint(&flow, &io);

    /* Run full flow */
    flow.synth.num_src_files = 12;
    int ret = vivado_flow_run_full(&flow, "output.bit");
    printf("  Full flow result: %s\n", ret == 0 ? "SUCCESS" : "FAILED");
    printf("  Progress: %.0f%%\n", flow.flow_progress_pct);
    printf("  Constraints: %d\n", flow.num_constraints);
}

static void demo_timing_report(void)
{
    printf("\n--- Timing Path Report Demo ---\n");

    TimingPathReport rpt;
    timing_path_init(&rpt);
    strcpy(rpt.path_name, "clk_to_dout");
    strcpy(rpt.startpoint, "src_reg/C");
    strcpy(rpt.endpoint, "dout_reg/D");
    rpt.is_setup = 1;
    rpt.slack_ns = 0.234;
    rpt.total_delay_ns = 8.766;
    rpt.logic_delay_ns = 5.123;
    rpt.route_delay_ns = 3.643;
    rpt.num_logic_levels = 12;
    rpt.arrival_time_ns = 9.234;
    rpt.required_time_ns = 10.000;
    timing_path_print(&rpt);

    printf("\n--- Utilization Report Demo ---\n");
    UtilizationReport util;
    utilization_init(&util);
    util.slice_lut_used = 45678;
    util.slice_lut_pct = 23.4;
    util.slice_reg_used = 22100;
    util.slice_reg_pct = 10.8;
    util.dsp_used = 48;
    util.dsp_pct = 5.7;
    util.bram_used = 32;
    util.bram_pct = 7.2;
    util.io_used = 156;
    util.io_pct = 31.2;
    util.total_power_w = 2.345;
    util.static_power_w = 0.456;
    util.dynamic_power_w = 1.889;
    utilization_print(&util);
}

int main(void)
{
    printf("=== Vivado Flow Example ===\n");
    printf("Version: %s\n\n", VIVADO_VERSION);

    printf("Available strategies:\n");
    printf("  SYNTH: Default, AreaOpt, PerfOpt, PerfRetime, AltFlow, AreaExplore\n");
    printf("  IMPL:  Default, Explore, AggressiveExplore, RuntimeOptimized\n\n");

    printf("Implementation stages:\n");
    printf("  0:init_design 1:opt_design 2:power_opt 3:place_design\n");
    printf("  4:post_place_ps 5:phys_opt 6:route_design 7:post_route_ps\n");

    demo_synthesis();
    demo_constraints();
    demo_implementation();
    demo_full_flow();
    demo_timing_report();

    printf("\n=== All Vivado flow tests passed ===\n");
    return 0;
}
