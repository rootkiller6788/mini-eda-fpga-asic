# mini-hls-compiler — HLS Compiler (C99)

A lightweight High-Level Synthesis (HLS) compiler implemented in pure C99. Converts
C functions into synthesizable RTL (Verilog) through dataflow graph analysis,
scheduling, resource binding, and FSMD code generation.

## Features

- **DFG Construction**: Build dataflow graph from C source or manual API
- **Scheduling**: ASAP, ALAP, list scheduling, force-directed scheduling
- **Allocation**: Greedy resource allocation, clique partitioning, lifetime analysis
- **Binding**: Left-edge register binding, functional unit binding
- **FSMD Generation**: Finite State Machine with Datapath, Verilog emission

## Structure

```
mini-hls-compiler/
├── include/
│   ├── hls_dfg.h         — Dataflow graph types and API
│   ├── hls_schedule.h    — Scheduling algorithms
│   ├── hls_allocate.h    — Resource allocation
│   ├── hls_binding.h     — Register and FU binding
│   └── hls_fsmd.h        — FSMD controller and Verilog emission
├── src/                  — Corresponding .c implementations
├── examples/
│   └── example_hls.c     — Full HLS flow demo
├── tests/
│   └── test_hls.c        — Unit tests
├── docs/
│   └── course-alignment.md
├── Makefile
└── README.md
```

## Build

```sh
make          # build static library and examples
make test     # build and run tests
make clean    # remove build artifacts
```

## Quick Start

```c
#include "hls_dfg.h"
#include "hls_schedule.h"
#include "hls_fsmd.h"

int main(void) {
    DataFlowGraph dfg;
    dfg_init(&dfg);

    // Build: a + b
    int a = dfg_add_node(&dfg, HLS_LOAD, "a", 0.5);
    int b = dfg_add_node(&dfg, HLS_LOAD, "b", 0.5);
    int add = dfg_add_node(&dfg, HLS_ADD, "sum", 1.0);
    dfg_add_edge(&dfg, a, add, 0);
    dfg_add_edge(&dfg, b, add, 0);

    // Schedule, generate Verilog
    sched_asap(&dfg);
    FsmdController c;
    c.dfg = &dfg;
    fsmd_generate(&c);
    fsmd_emit_verilog(&c);
    return 0;
}
```

## Course Alignment

- CMU 18-643: Reconfigurable Logic and HLS
- Stanford EE271: ASIC Design
- MIT 6.375: Complex Digital Systems

## License

MIT
