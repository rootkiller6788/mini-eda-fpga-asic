# Architecture Overview

## Project Structure

```
mini-hdl-lang/
├── include/           Header files (5)
│   ├── verilog_sim.h         Verilog simulation engine
│   ├── vhdl_sim.h            VHDL simulation engine
│   ├── systemverilog_sim.h   SystemVerilog simulation engine
│   ├── testbench_gen.h       Testbench generation framework
│   └── waveform_view.h       Waveform viewer (VCD/ASCII)
├── src/               Source implementations (5)
│   ├── verilog_sim.c
│   ├── vhdl_sim.c
│   ├── systemverilog_sim.c
│   ├── testbench_gen.c
│   └── waveform_view.c
├── examples/          Example programs (3)
│   ├── verilog_counter.c     Counter module in Verilog
│   ├── vhdl_adder.c          Adder entity in VHDL
│   └── sv_fifo.c             FIFO module in SystemVerilog
├── demos/             Demo documentation (2)
│   ├── mini-verilog-sim/     Verilog simulator demo
│   └── mini-vhdl-testbench/  VHDL testbench demo
├── docs/              Documentation
│   ├── architecture.md       This file
│   └── api_reference.md      API reference
├── bin/               Build output directory
├── Makefile           Build system
└── README.md          Project README
```

## Design Philosophy

`mini-hdl-lang` is a lightweight, pure C99 implementation of core HDL simulation concepts. The focus is on educational clarity and modularity rather than production-grade performance. Each simulation engine follows the same pattern:
1. Initialize the simulator struct with defaults.
2. Build the design by adding modules/entities, ports, signals, and behavioral blocks.
3. Run the simulation for a specified duration.
4. Inspect results and generate outputs (console display, VCD waveform).
5. Clean up allocated resources.

## Simulation Engines

### Verilog Simulator (`verilog_sim.h`)
Event-driven simulation kernel that processes time-stamped events from a priority queue. Supports the classical Verilog scheduling model with continuous assignments evaluated first, followed by procedural always blocks triggered by sensitivity list matches. Implements both blocking (`=`) and non-blocking (`<=`) assignments following IEEE 1364 semantics.

### VHDL Simulator (`vhdl_sim.h`)
Delta-cycle-based simulation following IEEE 1076 semantics. Maintains projected signal waveforms, distinguishes between signal and variable assignments, and resolves multi-driver conflicts using resolution functions for `std_logic`. Each time step may execute multiple delta cycles until signal stabilization.

### SystemVerilog Simulator (`systemverilog_sim.h`)
Extends Verilog concepts with SystemVerilog-specific features: `always_ff` and `always_comb` blocks, the `logic` 4-state data type, interface constructs with modports, enumerated types, struct definitions, packages for shared declarations, and immediate assertion checking.

## Testbench Framework (`testbench_gen.h`)
Provides a complete testbench infrastructure: clock generation (free-running, gated, divided), reset sequencing, programmable stimulus injection (set, pulse, random, increment, pattern), monitor checking with assertion-based self-verification, functional coverage collection with bin tracking, and VCD waveform output for visualization.

## Waveform Viewer (`waveform_view.h`)
Parses standard VCD (Value Change Dump) files conforming to IEEE 1364-2001 format. Tracks signal value history over time, detects transitions (rise, fall, change, unknown, high-impedance), and renders an ASCII waveform display for quick terminal-based inspection of digital signals.

## Memory Management

All engines provide `_free()` functions that deallocate heap-allocated resources (signal value arrays, event lists, etc.). The user is responsible for calling these cleanup functions when simulation is complete.
