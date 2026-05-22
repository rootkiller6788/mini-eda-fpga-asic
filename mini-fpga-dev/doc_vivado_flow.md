# Vivado Design Flow Reference

## Overview

The Vivado flow module models Xilinx Vivado's core design flow from synthesis through bitstream generation, including XDC constraint management.

## Design Flow Pipeline

```
RTL Sources + XDC Constraints
        │
        ▼  (synthesis)
  Synthesized Netlist (DCP)
        │
        ▼  (implementation)
  Placed-and-Routed Design (DCP)
        │
        ▼  (bitgen)
  Configuration Bitstream (.bit)
```

## 1. XDC Constraints

XDC (Xilinx Design Constraints) use SDC-compatible syntax extended with Xilinx-specific commands.

### Constraint Types

| Type | Value | Description |
|------|-------|-------------|
| XDC_CLOCK | 0 | create_clock / create_generated_clock |
| XDC_INPUT_DELAY | 1 | set_input_delay |
| XDC_OUTPUT_DELAY | 2 | set_output_delay |
| XDC_CLOCK_GROUP | 3 | set_clock_groups |
| XDC_FALSE_PATH | 4 | set_false_path |
| XDC_MULTICYCLE | 5 | set_multicycle_path |
| XDC_PIN_LOC | 6 | set_property PACKAGE_PIN |
| XDC_IO_STANDARD | 7 | set_property IOSTANDARD |

### Clock Constraints
```
create_clock -name <name> -period <ns> [get_ports <port>]
create_generated_clock -name <name> -source <src> -divide_by <N> [get_ports <port>]
```

### Timing Exceptions
```
set_false_path -from <from> -to <to> [-through <through>]
set_multicycle_path <N> -from <from> -to <to> [-setup|-hold]
```

### I/O Constraints
```
set_property PACKAGE_PIN <pin>  [get_ports <signal>]
set_property IOSTANDARD <std>   [get_ports <signal>]
```

## 2. Synthesis

Converts RTL (VHDL/Verilog) into an optimized gate-level netlist.

### Strategies

| Strategy | Value | When to Use |
|----------|-------|-------------|
| Default | 0 | Balanced area/performance |
| AreaOptimized | 1 | Minimize LUT/FF usage |
| PerformanceOptimized | 2 | Maximize Fmax |
| PerformanceRetiming | 3 | Best Fmax with register retiming |
| AlternateFlow | 4 | Alternative algorithm flow |
| AreaExplore | 5 | Aggressive area reduction |

### Key Options
- `flatten_hierarchy` — Rebuild vs. preserve design hierarchy
- `fsm_extraction` — 0=off, 1=one-hot, 2=sequential encoding
- `resource_sharing` — Share arithmetic operators
- `keep_hierarchy` — Preserve module boundaries

**API:**
- `synthesis_init(syn, part)` — Initialize synthesis engine
- `synthesis_add_source(src)` — Add RTL source file
- `synthesis_run()` — Execute synthesis
- `synthesis_report()` — Print resource estimates

## 3. Implementation (Place & Route)

Transforms synthesized netlist into placed-and-routed physical design.

### Stages

| Stage | Index | Description |
|-------|-------|-------------|
| init_design | 0 | Load synthesized DCP, create design |
| opt_design | 1 | Logic optimization, constant propagation |
| power_opt_design | 2 | Power optimization (clock gating, etc.) |
| place_design | 3 | Place logic cells onto fabric |
| post_place_phys_opt | 4 | Physical optimization after placement |
| phys_opt_design | 5 | Full physical synthesis |
| route_design | 6 | Route nets through routing fabric |
| post_route_phys_opt | 7 | Final timing optimization |

### Directives
- **opt_design**: Explore, AggressiveExplore, RuntimeOptimized
- **place_design**: Explore, AggressiveExplore, WLDriven, TimingDriven
- **route_design**: Explore, AggressiveExplore, NoTimingRelaxation
- **phys_opt_design**: Explore, AggressiveExplore, AlternateReplication

**API:**
- `impl_init(impl)` — Initialize implementation engine
- `impl_set_directive(stage, directive)` — Set per-stage directive
- `impl_run_stage(stage)` — Run single stage
- `impl_run_all()` — Run all stages sequentially
- `impl_generate_report(type)` — Generate timing/utilization reports

## 4. Vivado Flow Manager

Orchestrates the complete flow from synthesis through bitstream generation.

**API:**
- `vivado_flow_init(project, part)` — Initialize flow
- `vivado_flow_add_constraint(xdc)` — Add XDC constraint
- `vivado_flow_run_synthesis()` — Run synthesis only
- `vivado_flow_run_implementation()` — Run implementation only
- `vivado_flow_generate_bitstream(outfile)` — Generate bitstream
- `vivado_flow_run_full(bitfile)` — Run complete flow
- `vivado_flow_report_summary()` — Print flow summary

## 5. Reports

### Timing Path Report
```
Path:     path_name
Start:    src_reg/C
End:      dst_reg/D
Slack:    +/- n.n ns (MET/VIOLATED)
Delay:    total (logic: n.nn, route: n.nn)
Levels:   N
```

### Utilization Report
```
Slice LUTs:  used / total (xx%)
Slice Regs:  used / total (xx%)
DSPs:        used / total (xx%)
BRAMs:       used / total (xx%)
I/Os:        used / total (xx%)
Power:       total W (static: W, dynamic: W)
```

## Typical Usage

```c
VivadoFlow flow;
vivado_flow_init(&flow, "my_project", "xc7k325tffg900-2");

// Add constraints
XdcConstraint clk;
xdc_init(&clk);
xdc_create_clock(&clk, "clk", 10.0, "clk_pin", 0.5);
vivado_flow_add_constraint(&flow, &clk);

XdcConstraint io;
xdc_init(&io);
xdc_set_io_location(&io, "led", "LVCMOS33", "Y11");
vivado_flow_add_constraint(&flow, &io);

// Add sources
synthesis_add_source(&flow.synth, "top.vhd");
synthesis_add_source(&flow.synth, "core.v");

// Run
vivado_flow_run_full(&flow, "output.bit");
vivado_flow_report_summary(&flow);
```
