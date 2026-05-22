#include "vivado_flow.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* =======================================================
 * XDC Constraint Implementation
 * ======================================================= */

void xdc_init(XdcConstraint *xc)
{
    memset(xc, 0, sizeof(XdcConstraint));
    xc->duty_cycle = 0.5;
    xc->multicycle_value = 2;
}

int xdc_create_clock(XdcConstraint *xc, const char *name,
                      double period_ns, const char *target,
                      double duty_cycle)
{
    xc->type = XDC_CLOCK;
    strncpy(xc->name, name, sizeof(xc->name) - 1);
    xc->period_ns = period_ns;
    strncpy(xc->target_pin, target, sizeof(xc->target_pin) - 1);
    xc->duty_cycle = duty_cycle;
    xc->rise_time_ns = 0.0;
    xc->fall_time_ns = period_ns * duty_cycle;
    return 0;
}

int xdc_create_generated_clock(XdcConstraint *xc, const char *name,
                                const char *source, double divide_by,
                                const char *target)
{
    xc->type = XDC_CLOCK;
    strncpy(xc->name, name, sizeof(xc->name) - 1);
    strncpy(xc->target_clk, source, sizeof(xc->target_clk) - 1);
    strncpy(xc->target_pin, target, sizeof(xc->target_pin) - 1);
    xc->period_ns = divide_by * 1.0; /* will be derived from master */
    return 0;
}

int xdc_set_false_path(XdcConstraint *xc, const char *from,
                        const char *to, const char *through)
{
    xc->type = XDC_FALSE_PATH;
    strncpy(xc->from_path, from, sizeof(xc->from_path) - 1);
    strncpy(xc->to_path, to, sizeof(xc->to_path) - 1);
    if (through) {
        strncpy(xc->through, through, sizeof(xc->through) - 1);
    }
    xc->is_datapath_only = 0;
    return 0;
}

int xdc_set_io_location(XdcConstraint *xc, const char *pin,
                         const char *iostandard, const char *package_pin)
{
    xc->type = XDC_PIN_LOC;
    strncpy(xc->name, pin, sizeof(xc->name) - 1);
    strncpy(xc->iostandard, iostandard, sizeof(xc->iostandard) - 1);
    strncpy(xc->target_pin, package_pin, sizeof(xc->target_pin) - 1);
    return 0;
}

int xdc_set_multicycle_path(XdcConstraint *xc, const char *from,
                             const char *to, int value, int is_setup)
{
    xc->type = XDC_MULTICYCLE;
    strncpy(xc->from_path, from, sizeof(xc->from_path) - 1);
    strncpy(xc->to_path, to, sizeof(xc->to_path) - 1);
    xc->multicycle_value = value;
    xc->multicycle_type = is_setup ? 0 : 1;
    return 0;
}

/* =======================================================
 * Synthesis Engine Implementation
 * ======================================================= */

static const char *strategy_names[] = {
    "Default", "AreaOptimized", "PerformanceOptimized",
    "PerformanceRetiming", "AlternateFlow", "AreaExplore"
};

void synthesis_init(SynthesisEngine *syn, const char *part)
{
    memset(syn, 0, sizeof(SynthesisEngine));
    strncpy(syn->part_name, part, sizeof(syn->part_name) - 1);
    syn->strategy = SYNTH_STRAT_DEFAULT;
    syn->flatten_hierarchy = 0;
    syn->fsm_extraction = 1;
    syn->resource_sharing = 1;
    syn->keep_hierarchy = 0;
    syn->estimated_fmax_mhz = 500.0;
}

void synthesis_add_source(SynthesisEngine *syn, const char *filepath)
{
    if (syn->num_src_files < 64) {
        strncpy(syn->src_files[syn->num_src_files], filepath,
                sizeof(syn->src_files[0]) - 1);
        syn->num_src_files++;
    }
}

static void synthesis_estimate_resources(SynthesisEngine *syn,
                                          int logic_ops, int regs,
                                          int mem_kb, int mults)
{
    /* Simple estimation model */
    syn->estimated_luts  = (int64_t)(logic_ops * 1.2 + regs * 0.5);
    syn->estimated_ffs   = (int64_t)regs;
    syn->estimated_dsps  = (int64_t)mults;
    syn->estimated_brams = (int64_t)((mem_kb + 17) / 18);
}

int synthesis_run(SynthesisEngine *syn)
{
    /* Model synthesis as deterministic transformation */
    int base_ops    = 1000 + (syn->flatten_hierarchy ? 200 : 0);
    int base_regs   = 500  + (syn->resource_sharing ? -50 : 100);
    int base_mem    = 64;
    int base_mults  = 4;

    if (syn->strategy == SYNTH_STRAT_PERF_OPT ||
        syn->strategy == SYNTH_STRAT_PERF_RETIME) {
        base_regs += 200; /* more pipelining */
        base_ops += 100;
    } else if (syn->strategy == SYNTH_STRAT_AREA_OPT ||
               syn->strategy == SYNTH_STRAT_AREA_EXPLORE) {
        base_ops -= 100;
        base_regs -= 50;
    }

    synthesis_estimate_resources(syn, base_ops, base_regs, base_mem, base_mults);
    syn->num_elapsed_sec = 5 + (syn->num_src_files * 3);
    return 0;
}

void synthesis_report(const SynthesisEngine *syn)
{
    printf("=== Synthesis Report ===\n");
    printf("  Part:            %s\n", syn->part_name);
    printf("  Strategy:        %s\n", strategy_names[syn->strategy]);
    printf("  Source files:    %d\n", syn->num_src_files);
    printf("  Elapsed:         %d sec\n", syn->num_elapsed_sec);
    printf("  Est. Fmax:       %.1f MHz\n", syn->estimated_fmax_mhz);
    printf("  Est. LUTs:       %lld\n", (long long)syn->estimated_luts);
    printf("  Est. FFs:        %lld\n", (long long)syn->estimated_ffs);
    printf("  Est. DSPs:       %lld\n", (long long)syn->estimated_dsps);
    printf("  Est. BRAMs:      %lld\n", (long long)syn->estimated_brams);
}

/* =======================================================
 * Implementation Engine Implementation
 * ======================================================= */

static const char *stage_names[IMPL_STAGE_NUM] = {
    "init_design", "opt_design", "power_opt_design",
    "place_design", "post_place_phys_opt", "phys_opt_design",
    "route_design", "post_route_phys_opt"
};

void impl_init(ImplementationEngine *impl)
{
    memset(impl, 0, sizeof(ImplementationEngine));
    impl->current_stage = 0;
    impl->opt_directive = 0;
    impl->place_directive = 0;
    impl->route_directive = 0;
    impl->phys_opt_directive = 0;
    impl->num_stages_completed = 0;
}

void impl_set_directive(ImplementationEngine *impl, int stage, int directive)
{
    switch (stage) {
    case IMPL_STAGE_OPT_DESIGN:      impl->opt_directive = directive; break;
    case IMPL_STAGE_PLACE_DESIGN:    impl->place_directive = directive; break;
    case IMPL_STAGE_ROUTE_DESIGN:    impl->route_directive = directive; break;
    case IMPL_STAGE_PHYS_OPT:        impl->phys_opt_directive = directive; break;
    default: break;
    }
}

int impl_run_stage(ImplementationEngine *impl, int stage)
{
    if (stage < 0 || stage >= IMPL_STAGE_NUM) return -1;
    impl->current_stage = stage;

    /* Model each stage */
    switch (stage) {
    case IMPL_STAGE_INIT_DESIGN:
        impl->place_estimated_wirelength = 10000.0;
        impl->total_routing_nodes = 5000;
        break;
    case IMPL_STAGE_OPT_DESIGN:
        impl->place_estimated_wirelength *= 0.95;
        break;
    case IMPL_STAGE_POWER_OPT:
        /* Power optimization reduces switching activity */
        impl->total_routing_nodes = (int)(impl->total_routing_nodes * 0.98);
        break;
    case IMPL_STAGE_PLACE_DESIGN:
        impl->place_estimated_wirelength *= (0.80 + impl->place_directive * 0.05);
        break;
    case IMPL_STAGE_POST_PLACE_PS:
        /* Post-place physical synthesis */
        impl->place_estimated_wirelength *= 0.92;
        break;
    case IMPL_STAGE_PHYS_OPT:
        impl->place_estimated_wirelength *= 0.88;
        impl->total_routing_nodes += 200;
        break;
    case IMPL_STAGE_ROUTE_DESIGN:
        impl->route_estimated_delay_ns = impl->place_estimated_wirelength /
                                         100.0 + 1.5;
        impl->num_critical_paths = 10;
        break;
    case IMPL_STAGE_POST_ROUTE_PS:
        impl->route_estimated_delay_ns *= 0.95;
        break;
    }

    impl->num_stages_completed++;
    impl->stage_progress[stage] = 1.0;
    return 0;
}

int impl_run_all(ImplementationEngine *impl)
{
    for (int i = 0; i < IMPL_STAGE_NUM; i++) {
        if (impl_run_stage(impl, i) != 0) return -1;
    }
    return 0;
}

void impl_generate_report(const ImplementationEngine *impl, int report_type)
{
    if (report_type == REPORT_TIMING_SUMMARY) {
        printf("=== Timing Summary ===\n");
        printf("  Worst Slack (WNS):  %.3f ns\n",
               impl->route_estimated_delay_ns > 10.0 ? -1.2 : 0.150);
        printf("  Total stages:       %d\n", impl->num_stages_completed);
        printf("  Critical paths:     %d\n", impl->num_critical_paths);
    } else if (report_type == REPORT_UTILIZATION) {
        printf("=== Utilization ===\n");
        printf("  Routing nodes:      %d\n", impl->total_routing_nodes);
        printf("  Wirelength est:     %.0f\n", impl->place_estimated_wirelength);
    }
}

/* =======================================================
 * Vivado Flow Manager Implementation
 * ======================================================= */

void vivado_flow_init(VivadoFlow *flow, const char *project, const char *part)
{
    memset(flow, 0, sizeof(VivadoFlow));
    strncpy(flow->project_name, project, sizeof(flow->project_name) - 1);
    strncpy(flow->output_dir, "./vivado_output", sizeof(flow->output_dir) - 1);
    synthesis_init(&flow->synth, part);
    impl_init(&flow->impl);
    flow->current_flow_step = 0;
    flow->flow_progress_pct = 0.0;
}

int vivado_flow_add_constraint(VivadoFlow *flow, const XdcConstraint *xc)
{
    if (flow->num_constraints >= MAX_XDC_CONSTRAINTS) return -1;
    flow->constraints[flow->num_constraints] = *xc;
    flow->num_constraints++;
    return 0;
}

int vivado_flow_run_synthesis(VivadoFlow *flow)
{
    synthesis_run(&flow->synth);
    flow->current_flow_step = FLOW_SYNTHESIS;
    flow->flow_progress_pct = 25.0;
    return 0;
}

int vivado_flow_run_implementation(VivadoFlow *flow)
{
    impl_run_all(&flow->impl);
    flow->current_flow_step = FLOW_ROUTE;
    flow->flow_progress_pct = 85.0;
    return 0;
}

int vivado_flow_generate_bitstream(VivadoFlow *flow, const char *outfile)
{
    (void)flow;
    /* Model bitstream generation: write configuration to file.
     * In a real flow, this would call BITSTREAM_GEN routines. */
    if (outfile) {
        printf("  Bitstream written to: %s\n", outfile);
    }
    flow->flow_progress_pct = 100.0;
    flow->current_flow_step = FLOW_BITGEN;
    return 0;
}

int vivado_flow_run_full(VivadoFlow *flow, const char *bitfile)
{
    if (vivado_flow_run_synthesis(flow) != 0) return -1;
    if (vivado_flow_run_implementation(flow) != 0) return -1;
    if (vivado_flow_generate_bitstream(flow, bitfile) != 0) return -1;
    return 0;
}

void vivado_flow_report_summary(const VivadoFlow *flow)
{
    printf("========================================\n");
    printf(" Vivado Flow Summary — %s\n", flow->project_name);
    printf("========================================\n");
    printf("  Progress: %.0f%%\n", flow->flow_progress_pct);
    printf("  Constraint files: %d\n", flow->num_constraints);
    synthesis_report(&flow->synth);
    impl_generate_report(&flow->impl, REPORT_TIMING_SUMMARY);
}

/* =======================================================
 * Timing Path Report
 * ======================================================= */

void timing_path_init(TimingPathReport *rpt)
{
    memset(rpt, 0, sizeof(TimingPathReport));
}

void timing_path_print(const TimingPathReport *rpt)
{
    printf("=== Timing Path: %s ===\n", rpt->path_name);
    printf("  Startpoint:   %s\n", rpt->startpoint);
    printf("  Endpoint:     %s\n", rpt->endpoint);
    printf("  Type:         %s\n", rpt->is_setup ? "Setup" : "Hold");
    printf("  Slack:        %.3f ns %s\n", rpt->slack_ns,
           rpt->slack_ns >= 0.0 ? "(MET)" : "(VIOLATED)");
    printf("  Total delay:  %.3f ns\n", rpt->total_delay_ns);
    printf("  Logic delay:  %.3f ns\n", rpt->logic_delay_ns);
    printf("  Route delay:  %.3f ns\n", rpt->route_delay_ns);
    printf("  Logic levels: %d\n", rpt->num_logic_levels);
    printf("  Arrival:      %.3f ns\n", rpt->arrival_time_ns);
    printf("  Required:     %.3f ns\n", rpt->required_time_ns);
}

/* =======================================================
 * Utilization Report
 * ======================================================= */

void utilization_init(UtilizationReport *rpt)
{
    memset(rpt, 0, sizeof(UtilizationReport));
}

void utilization_print(const UtilizationReport *rpt)
{
    printf("=== Utilization Report ===\n");
    printf("  Slice LUTs:  %lld (%.1f%%)\n",
           (long long)rpt->slice_lut_used, rpt->slice_lut_pct);
    printf("  Slice Regs:  %lld (%.1f%%)\n",
           (long long)rpt->slice_reg_used, rpt->slice_reg_pct);
    printf("  DSPs:        %lld (%.1f%%)\n",
           (long long)rpt->dsp_used, rpt->dsp_pct);
    printf("  BRAMs:       %lld (%.1f%%)\n",
           (long long)rpt->bram_used, rpt->bram_pct);
    printf("  I/Os:        %lld (%.1f%%)\n",
           (long long)rpt->io_used, rpt->io_pct);
    printf("  Power:       %.3f W (static: %.3f, dynamic: %.3f)\n",
           rpt->total_power_w, rpt->static_power_w, rpt->dynamic_power_w);
}
