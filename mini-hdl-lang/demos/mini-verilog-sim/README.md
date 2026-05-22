# Mini Verilog Sim Demo

## Overview

This demo showcases the Verilog simulation capabilities of `mini-hdl-lang`. The mini-verilog-sim engine provides a lightweight, event-driven simulation kernel that supports the core Verilog hardware description language constructs including module instantiation, continuous assignments (`assign`), procedural blocks (`always@(posedge clk)`), wire and reg net types, and both blocking (`=`) and non-blocking (`<=`) assignments. The simulation follows the standard Verilog scheduling semantics, where blocking assignments execute immediately in the active region while non-blocking assignments are scheduled for the NBA (Non-Blocking Assignment) region, updating their left-hand side targets after all active events have been processed.

## Event-Driven Simulation

The event-driven engine processes events from a time-sorted event queue. Each event carries a timestamp, a trigger type (posedge, negedge, anyedge, or level), and references to the affected signal. When the simulator runs, it repeatedly dequeues the next event, advances the global simulation time, and evaluates all sensitive always blocks and continuous assignments in the correct order. This accurately models the reactive behavior of real digital hardware.

## Features Demonstrated

- **Module Definition**: Create named modules with input/output/inout ports of any width.
- **Net Types**: Wire, reg, wand, wor, tri, tri0, tri1, supply0, supply1.
- **Continuous Assignment**: `assign` statements that drive wire nets combinatorially.
- **Always Blocks**: `always@(posedge clk)` / `always@(negedge clk)` / `always@(*)` with sensitivity lists.
- **Blocking Assignment (`=`)**: Immediate value update within a procedural block.
- **Non-Blocking Assignment (`<=`)**: Scheduled update after the current time step completes.
- **Value Change Dump (VCD)**: Generate standard VCD files for waveform viewing in tools like GTKWave. VCD output includes module scope definitions, signal variable declarations, and time-stamped value changes.

## Running the Demo

The `examples/verilog_counter.c` program demonstrates a simple 4-bit counter module. It creates a counter with clock, reset, and count ports, schedules clock edges using the event queue, and produces a VCD file (`counter_wave.vcd`) that can be opened in GTKWave for visual waveform inspection. Build with `make` and run the generated binary from the `bin/` directory.

## Simulation Flow

1. Initialize the simulator via `vs_init()`.
2. Add a module with `vs_add_module()`.
3. Define ports and nets with appropriate types and widths.
4. Create always blocks with sensitivity lists using `vs_add_always()` and `vs_add_sensitivity()`.
5. Add procedural statements (blocking/non-blocking assigns, if, case, for loops).
6. Optionally open a VCD file with `vs_vcd_open()`.
7. Run the simulation for a specified time duration with `vs_run()`.
8. Display final signal states with `vs_display_signals()`.
9. Clean up with `vs_free()`.

## Output

The simulation produces console output showing signal values at each time step plus a standard-compliant VCD waveform file. The VCD file captures full signal history including transitions between 0, 1, X, and Z logic values with nanosecond-resolution timestamps.
