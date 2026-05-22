# Mini VHDL Testbench Demo

## Overview

This demo showcases the VHDL simulation and testbench generation capabilities of `mini-hdl-lang`. The mini-vhdl-testbench engine implements a cycle-accurate VHDL simulator supporting entity/architecture pairs, signal and variable semantics, process statements with sensitivity lists, concurrent signal assignment statements, and the full delta cycle mechanism that VHDL requires for correct simulation. It also includes a resolution function for `std_logic` type that properly handles multiple drivers following the IEEE 1164 standard.

## Entity and Architecture Model

The VHDL simulation kernel models the fundamental VHDL design unit: an entity defines the interface (ports and generics), while an architecture defines the implementation body (signals, processes, concurrent statements, and component instantiations). Ports are declared with modes (IN, OUT, INOUT, BUFFER) and can be of types including `std_logic`, `std_logic_vector`, `bit`, `bit_vector`, `integer`, and `boolean`. Generic parameters allow compile-time configuration of entity behavior.

## Signal vs Variable Semantics

A critical distinction in VHDL simulation is the difference between signals and variables. Signals have a projected output waveform, meaning that signal assignments within a process take effect only after the process suspends — they do not update immediately. Variables, on the other hand, update instantly within the process. This implementation correctly models both behaviors. Variables are scoped within individual processes, while signals are accessible across the entire architecture, including from concurrent statements.

## Delta Cycles

Delta cycles are the mechanism by which VHDL achieves zero-time simulation steps. When a signal is updated, any processes sensitive to that signal must be re-evaluated before simulation time advances. Multiple delta cycles may execute at the same simulation time, ensuring that all signal propagation completes before the clock advances. The demo tracks and reports the number of delta cycles executed, providing visibility into the simulation's internal scheduling behavior.

## Resolution Functions

For `std_logic` and `std_logic_vector` types, the resolution function determines the final value when multiple drivers connect to the same signal. The implementation handles the full 9-value logic system (U, X, 0, 1, Z, W, L, H, -) and resolves conflicts according to standard VHDL rules:
- 0 and 1 driving together produce X (unknown/conflict).
- A single driver (0 or 1) with Z produces the driven value.
- Multiple Z drivers produce Z.
- Uninitialized signals default to U.

## Testbench Features

- **Clock Generation**: Free-running, gated, and divided clock modes with configurable period and duty cycle.
- **Reset Generation**: Active-high or active-low reset with programmable assertion duration and deassertion timing.
- **Stimulus Injection**: Set, pulse, random, increment, and pattern-based stimulus types.
- **Monitor Checking**: Assertion-based monitors (compare actual vs expected), display monitors, and timing monitors.
- **Self-Checking**: Automatic pass/fail reporting with per-monitor error tracking.
- **Coverage Collection**: Functional coverage with bin-based collection and hit tracking.
- **Waveform Generation**: VCD waveform output compatible with GTKWave.

## Running the Demo

The `examples/vhdl_adder.c` program demonstrates a 4-bit adder entity with behavioral architecture. It runs multiple test vectors and reports results. Build with `make` and run the binary from `bin/`. The simulation outputs entity/architecture details, delta cycle counts, and signal state dumps.

## Simulation Flow

1. Initialize the VHDL simulator with `vhdl_init()`.
2. Create an entity with ports using `vhdl_add_entity()` and `vhdl_add_port()`.
3. Create an architecture with `vhdl_add_architecture()`.
4. Add signals (optionally resolved) with `vhdl_add_signal()`.
5. Create processes with sensitivity lists using `vhdl_add_process()`.
6. Add procedural statements to processes.
7. Run the simulation with `vhdl_run()` or individual delta cycles with `vhdl_run_delta_cycle()`.
8. Display results and clean up.
