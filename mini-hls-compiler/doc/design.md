# Design Document: mini-hls-compiler

## Overview

mini-hls-compiler is a lightweight High-Level Synthesis (HLS) compiler framework
implemented in C99. It models the key stages of the HLS compilation flow from
C function specification to synthesizable RTL.

## Architecture

The compiler is organized into 5 modules, each targeting a specific stage of
the HLS optimization and code generation pipeline:

### 1. Pipeline (`hls_pipeline.*`)

Handles the core HLS flow:

- **DFG Construction**: Parses C-like operations into a Data Flow Graph (DFG)
  of `HlsNode` objects. Each node represents an operation (add, mul, load,
  store, etc.) with input/output edges.
- **CFG Support**: Basic blocks (`HlsBasicBlock`) model control flow with
  predecessor/successor edges for branches and loops.
- **Scheduling**:
  - *ASAP* (As Soon As Possible): Topological forward pass assigning earliest
    cycle to each node based on input dependencies.
  - *ALAP* (As Late As Possible): Reverse pass computing latest cycle, enabling
    slack analysis and mobility-based optimization.
  - *List Scheduling*: Resource-constrained scheduling that respects operation-
    type resource limits (ALU, MUL, DIV, memory ports).
- **Resource Binding**: Maps scheduled operations to hardware functional units
  through greedy left-edge allocation.
- **Pipeline Generation**: Creates `HlsPipelineConfig` with configurable
  Initiation Interval (II), number of stages, and stall/flush capability.
  Pipeline verification checks feasibility against the schedule result.
- **RTL Emission**: Generates synthesizable Verilog from the pipeline
  configuration, including pipeline registers, stall logic, and flush control.

### 2. Loop Optimization (`loop_optimize.*`)

Targets loop-level transformations critical for FPGA performance:

- **Unroll**: Partial (factor-based reduction) and complete (full unroll into
  combinational logic). Legality checks ensure trip count divisibility.
- **Flatten**: Collapses nested loops into a single loop by multiplying trip
  counts and absorbing inner loop bodies.
- **Merge**: Combines consecutive loops at the same nesting level with
  identical trip counts and types.
- **Pipeline Rewind**: Enables continuous pipeline execution where the loop
  body is a pipeline that auto-restarts after completion, with configurable
  partial rewind depth.
- **Trip Count Analysis**: Analyzes loop bounds to determine if trip counts are
  constant, affine, or unknown, providing min/max/avg estimates.

### 3. Dataflow Optimization (`dataflow_opt.*`)

Implements task-level parallelism:

- **Task Model**: `HlsTask` represents an independent processing stage with
  configurable latency, interval, and pipelining.
- **Channel Types**:
  - *FIFO*: Point-to-point queue with depth/width configuration, supporting
    blocking and non-blocking reads/writes.
  - *Ping-Pong*: Double (or multi) buffer scheme with swap operation for
    overlapped computation and communication.
  - *Stream*: hls::stream-style interface with full/empty status flags.
  - *Shared Memory*: Direct memory access between tasks.
- **Stencil Patterns**: Defines 1D/2D/3D stencil computation windows with
  radii, window size calculations, line buffer depth configuration, and
  shift register optimizations for small windows.
- **Scheduling**: Links tasks into linear execution order.
- **Balancing**: Analyzes task latencies to identify bottlenecks and compute
  overall dataflow latency.
- **Verification**: Checks that every channel has at least one producer and
  consumer.

### 4. Array Partition (`array_partition.*`)

Manages on-chip memory organization:

- **Partition Types**:
  - *Block*: Divides array into contiguous blocks across banks.
  - *Cyclic*: Interleaves elements round-robin across banks for parallel
    access to consecutive elements.
  - *Complete*: Fully partitions into individual registers.
- **Reshape**: Combines elements to widen data width, reducing array
  dimensions while preserving total bit capacity.
- **Memory Banking**: Analyzes and assign banks, detects bank conflicts
  for given access patterns, and reports port utilization.
- **Memory Type Selection**: Tradeoff analysis between BRAM (block RAM),
  LUTRAM (distributed RAM), and URAM based on element count and width.
  BRAM is recommended for large arrays (>= 128 elements), LUTRAM for
  small arrays (<= 256 elements).
- **Port Management**: Configures single-port vs dual-port, validates
  whether a given number of parallel accesses is feasible across all
  banks.

### 5. Interface & Pragma (`interface_pragma.*`)

Configures hardware interfaces and HLS directives:

- **Interface Protocols**:
  - `ap_ctrl_none`: Minimal control (no handshake).
  - `ap_ctrl_hs`: Full handshake protocol.
  - `s_axilite`: AXI4-Lite slave for register-based control.
  - `m_axi`: AXI4 master for external memory access with burst
    configuration.
  - `axis`: AXI4-Stream with valid/ready handshake.
  - `ap_fifo`, `ap_memory`, `ap_bus`, `ap_none`, `ap_stable`, `ap_vld`.
- **AXI4-Stream Handshake**: Implements valid/ready protocol with
  `tlast`, `tkeep`, `tstrb`, and sideband signals (`tid`, `tdest`,
  `tuser`).
- **AXI4-Master Burst**: Configurable burst length, transfer size,
  cache/prot bits, QoS, and channel count.
- **Bundling**: Groups related ports under a named bundle sharing a
  single interface protocol.
- **HLS Directives**:
  - *Pipeline*: Sets II, enable_flush, rewind, and target function/loop.
  - *Unroll*: Factor (0 = complete), region targeting, exit check skip.
  - *Array Partitiion*: Type, dimension, factor, variable binding.
  - *Dataflow*: Enables task-level parallelism with max task limit and
    start propagation control.
  - *Resource*: Limits or allocates specific core types (DSP48E2,
    Fabric) per operation.
- **Tcl Generation**: Produces Vivado HLS compatible Tcl script with
  all directives, project setup, solution configuration, and design
  export commands.

## Data Flow

```
C Function
    |
    v
DFG Construction (hls_pipeline)
    |
    v
Loop Analysis (loop_optimize)
    |-- Unroll, Flatten, Merge
    v
Scheduling (hls_pipeline)
    |-- ASAP, ALAP, List
    v
Array Partition (array_partition)
    |-- Block/Cyclic/Complete
    v
Dataflow Decomposition (dataflow_opt)
    |-- Task splitting, Channel creation
    v
Resource Binding (hls_pipeline)
    |
    v
Interface Assignment (interface_pragma)
    |-- AXI, Bundle, Pragma directives
    v
RTL Generation (hls_pipeline)
    |
    v
Verilog Output + Tcl Scripts
```

## Key Design Decisions

1. **C99 compliance**: All code uses C99 features only (no C11/C17 extensions).
   Includes `<stdint.h>`, `<stdbool.h>`, and `<stdio.h>` for portability.

2. **Static limits**: Maximum nodes (1024), blocks (256), loops (256),
   channels (64), tasks (64), and pragma entries (16-32) are defined as
   compile-time constants. Production use would require dynamic sizing.

3. **Pipeline-centric**: The pipeline model is the core abstraction. All
   optimizations (loop unrolling, array partitioning, dataflow) feed into
   improving the pipeline schedule quality and achievable II.

4. **Resource modeling**: Functional units are categorized into 8 types
   (ALU, MUL, DIV, MEM_RD, MEM_WR, BRAM, DSP, LUT, FF). Resource limits
   drive list scheduling and binding decisions.

5. **Verilog emission**: The RTL printer generates basic synthesizable
   Verilog with pipeline registers. Advanced features (state machines,
   memory inference, IP instantiation) are left as extensions.
