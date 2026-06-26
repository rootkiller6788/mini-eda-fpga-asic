# mini-hdl-lang — HDL语言 (C 语言实现)

A lightweight, educational hardware description language simulation framework written in C99.

## Overview

`mini-hdl-lang` provides simulation kernels for the three major hardware description languages — **Verilog**, **VHDL**, and **SystemVerilog** — along with a complete testbench generation framework and a waveform viewer that parses standard VCD files. All engines are implemented in pure C99 with no external dependencies beyond the standard library.

## Features

- **Verilog Simulator** — Event-driven simulation with module instantiation, continuous assignments (`assign`), procedural blocks (`always@`), wire/reg net types, blocking (`=`) and non-blocking (`<=`) assignments, and VCD waveform output.
- **VHDL Simulator** — Delta-cycle simulation with entity/architecture pairs, signal vs variable semantics, process sensitivity lists, concurrent statements, and IEEE 1164 `std_logic` resolution functions.
- **SystemVerilog Simulator** — Extends Verilog with `always_ff`/`always_comb` blocks, `logic` 4-state type, interface constructs, enums, structs, packages, and immediate assertions.
- **Testbench Generator** — Clock/reset generation, programmable stimulus injection, assertion-based monitors, self-checking pass/fail reporting, functional coverage collection, and VCD waveform generation.
- **Waveform Viewer** — VCD file parser supporting IEEE 1364-2001 format, signal value history tracking, transition detection (rise/fall/change/X/Z), and an ASCII waveform renderer for terminal-based waveform inspection.

## Project Structure

```
mini-hdl-lang/
├── include/               # Header files
│   ├── verilog_sim.h
│   ├── vhdl_sim.h
│   ├── systemverilog_sim.h
│   ├── testbench_gen.h
│   └── waveform_view.h
├── src/                   # Source implementations
│   ├── verilog_sim.c
│   ├── vhdl_sim.c
│   ├── systemverilog_sim.c
│   ├── testbench_gen.c
│   └── waveform_view.c
├── examples/              # Example programs
│   ├── verilog_counter.c
│   ├── vhdl_adder.c
│   └── sv_fifo.c
├── demos/                 # Demo documentation
│   ├── mini-verilog-sim/
│   └── mini-vhdl-testbench/
├── docs/                  # Documentation
│   ├── architecture.md
│   └── api_reference.md
├── bin/                   # Build output
├── Makefile
└── README.md
```

## Building

Requirements: GCC (or any C99 compiler), GNU Make.

```sh
make          # Build all examples
make clean    # Remove build artifacts
```

After building, the compiled binaries will appear in `bin/`:
- `bin/verilog_counter` — Verilog 4-bit counter simulation with VCD output
- `bin/vhdl_adder` — VHDL 4-bit adder with test vectors
- `bin/sv_fifo` — SystemVerilog FIFO with enums, interfaces, and assertions

## Quick Start

### Verilog Counter Example

```sh
make
bin/verilog_counter
gtkwave counter_wave.vcd    # View waveform output
```

### VHDL Adder Example

```sh
bin/vhdl_adder
```

### SystemVerilog FIFO Example

```sh
bin/sv_fifo
```

## API Overview

Each engine follows a consistent initialization-build-run-cleanup pattern. See `docs/api_reference.md` for the complete function reference.

### Verilog Simulator

```c
VerilogSimulator sim;
vs_init(&sim);
int mod = vs_add_module(&sim, "my_module");
vs_add_port(sim.modules[mod], "clk", VS_PORT_INPUT, 1);
// ... build design ...
vs_run(&sim, 1000);
vs_display_signals(sim.modules[mod], sim.current_time);
vs_free(&sim);
```

### VHDL Simulator

```c
VhdlSimulator sim;
vhdl_init(&sim);
int ent = vhdl_add_entity(&sim, "my_entity");
int arch = vhdl_add_architecture(&sim, ent, "behavioral");
// ... build design ...
vhdl_run(&sim, 1000);
vhdl_display_signals(&sim.architectures[arch]);
vhdl_free(&sim);
```

### SystemVerilog Simulator

```c
SvSimulator sim;
sv_init(&sim);
int mod = sv_add_module(&sim, "my_module");
sv_add_always_ff(&sim.modules[mod], "clk", true);
sv_add_always_comb(&sim.modules[mod]);
// ... build design ...
sv_run(&sim, 1000);
sv_display_module(&sim.modules[mod]);
sv_free(&sim);
```

### Testbench Generator

```c
TbTestbench tb;
tb_init(&tb, "my_testbench", 1000);
tb_add_clock(&tb, "clk", 10, 50);
tb_add_reset(&tb, "rst_n", 0, TB_RST_ACTIVE_LOW);
tb_run(&tb);
tb_report(&tb);
tb_free(&tb);
```

### Waveform Viewer

```c
WvVcdData vcd;
wv_init(&vcd);
wv_parse_vcd(&vcd, "dump.vcd");
wv_detect_transitions(&vcd);
wv_print_signal_list(&vcd, stdout);

WvViewer viewer;
wv_init_viewer(&viewer);
wv_set_view_range(&viewer, vcd.start_time, vcd.end_time);
wv_render_ascii_waveform(&vcd, &viewer, stdout);

wv_free_viewer(&viewer);
wv_free(&vcd);
```

## Simulation Concepts

| Concept | Verilog | VHDL | SystemVerilog |
|---|---|---|---|
| Module/Entity | `module` | `entity`/`architecture` | `module` |
| Net/Signal | wire, reg | signal, variable | logic |
| Combinational | `assign`, `always@(*)` | concurrent, process | `always_comb` |
| Sequential | `always@(posedge clk)` | process(clk) | `always_ff` |
| Blocking assign | `=` (immediate) | `:=` (variable) | `=` |
| Non-blocking | `<=` (NBA region) | `<=` (projected) | `<=` |
| Multi-driver | wand, wor, tri | resolution function | resolution |
| Time model | Event queue | Delta cycles | Event queue |
| Waveform | VCD | VCD | VCD |

## License

MIT License

## Contributing

This is an educational project. Contributions, bug reports, and feature requests are welcome.
