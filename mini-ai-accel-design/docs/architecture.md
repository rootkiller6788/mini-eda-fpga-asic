# AI Accelerator Architecture

## Overview

mini-ai-accel-design implements a behavior-level model of a systolic-array-based deep learning
accelerator. The architecture is designed for INT8 inference with Weight-Stationary dataflow,
multi-level on-chip buffering, and a custom ISA for tiled matrix operations.

## Systolic Array

### Dataflow

The systolic array uses **Weight-Stationary (WS)** dataflow, where:

1. **Weights** are pre-loaded into each PE and remain stationary during computation.
2. **Activations** are broadcast across rows (horizontal flow).
3. **Partial sums** accumulate along columns (vertical reduction).

```
        act_in[0] --> [PE00] --> [PE01] --> [PE02] --> partial[2]
        act_in[1] --> [PE10] --> [PE11] --> [PE12] --> partial[1]
        act_in[2] --> [PE20] --> [PE21] --> [PE22] --> partial[0]
                        |          |          |
                     weight     weight     weight
```

### Pipelining

Pipeline registers are inserted between PEs to improve timing closure:

```
PE -> [REG] -> PE -> [REG] -> PE
```

Pipeline depth is configurable (1-8 stages). Each register adds 1 cycle of latency
but allows higher clock frequency by reducing the critical path through each PE.

### Configuration

| Parameter          | Default  | Description                          |
|--------------------|----------|--------------------------------------|
| `SA_MAX_ROWS`      | 256      | Max systolic array rows              |
| `SA_MAX_COLS`      | 256      | Max systolic array columns           |
| `pipeline_depth`   | 1        | Pipeline registers between PEs        |
| `dataflow`         | WS       | Weight-stationary, output-stationary, or row-stationary |

## PE Microarchitecture

Each Processing Element (PE) contains:

```
          +-----------+       +-----------+
 act_in ->| Activation |------>|           |
          |   FIFO     |       | Multiply- |    +------------+
          +-----------+       | Accumulate|--->| Accumulator|
                              |  (MAC)    |    +------------+
          +-----------+       |           |
          |  Weight   |------>| INT8*INT8 |
          | Register  |       | -> INT32  |
          | (DBuf)    |       +-----------+
          +-----------+
```

### Key Features

| Feature             | Description                                        |
|---------------------|----------------------------------------------------|
| **Double-buffered weights** | Ping-pong weight registers for overlapped load/compute |
| **Activation FIFO** | 32-entry FIFO for decoupling input arrival from MAC |
| **INT8 × INT8 → INT32** | MAC with 32-bit accumulation to prevent overflow |
| **Zero-skipping**   | Skip MAC when activation is zero (configurable threshold) |
| **Clock gating**    | Per-PE clock gating for unused array elements       |

### Sparsity Handling

When zero-skipping is enabled:

1. Check activation value against zero (or threshold).
2. If zero, skip multiply and accumulation for that cycle.
3. Count zeros skipped for utilization tracking.
4. Skip rate tracked: `skips / (MACs + skips)`.

## Buffer Hierarchy

Three-level on-chip memory:

```
  DRAM (off-chip)
       |
       v (DMA)
  +-----------+
  | L3 Buffer |  2 MB chip-global, double-buffered
  +-----------+
       |
       v (multicast / scatter-gather)
  +-----------+
  | L2 Buffer |  64 KB array-global, double-buffered
  +-----------+
       |
       v (distribution)
  +-----------+
  | L1 Buffer |  512 B PE-local scratchpad
  +-----------+
```

### Double Buffering

Each buffer level supports double buffering for overlapped data transfer and computation:

| Phase     | Buffer 0        | Buffer 1        |
|-----------|-----------------|-----------------|
| Step N    | DMA load        | Compute         |
| Step N+1  | Compute         | DMA load        |
| Step N+2  | DMA load        | Compute         |

### Multicast Distribution

For activation broadcasting, L2 supports hardware multicast: one source data written
to multiple destinations in a single cycle via a destination bitmask.

### Scatter/Gather

Sparse matrix operations use scatter/gather:
- **Scatter**: Read from non-contiguous indices.
- **Gather**: Write to non-contiguous indices.

## DNN ISA

Custom 16-byte instruction format:

```
Byte:  0       1       2-3    4-5     6-7     8-15
     +-------+-------+-------+-------+-------+---------------+
     |opcode |flags  |reserved| operand fields (varies)      |
     +-------+-------+-------+-------+-------+---------------+
```

### Instruction Set

| Opcode        | Mnemonic     | Operands           | Description                    |
|---------------|-------------|---------------------|--------------------------------|
| 0x00          | NOP         | —                   | No operation                   |
| 0x01          | LOAD_W      | addr, rows, cols    | Load weights to SA             |
| 0x02          | LOAD_A      | addr, len           | Load activations               |
| 0x03          | MATMUL      | M, N, K             | Matrix multiply                |
| 0x04          | ACTIVATION  | fn, len             | Apply activation function       |
| 0x05          | STORE       | addr, len           | Store results to memory        |
| 0x06          | SYNC        | —                   | Synchronization barrier        |
| 0x07          | LOOP_BEG    | count, stride       | Begin hardware loop            |
| 0x08          | LOOP_END    | —                   | End hardware loop              |
| 0x09          | BARRIER     | id                  | Named barrier                  |
| 0x0A          | DMA_LD      | src, dst, len       | DMA load                       |
| 0x0B          | DMA_ST      | src, dst, len       | DMA store                      |
| 0x0C          | SET_TILE    | tm, tn, tk          | Configure tiling               |
| 0x0D          | WAIT        | id                  | Wait for barrier               |
| 0x0E          | HALT        | —                   | Stop execution                 |

### Tiling: M × N × K

Matrix multiplication is decomposed into tiles:

```
for (k_tile = 0; k_tile < K; k_tile += Tk)
  for (m_tile = 0; m_tile < M; m_tile += Tm)
    for (n_tile = 0; n_tile < N; n_tile += Tn)
      LOAD_W(Tm × Tk)
      LOAD_A(Tk × Tn)
      MATMUL(Tm, Tn, Tk)
```

### Hardware Loop Counters

Up to 6 nested hardware loops with auto-increment and stride. Programs use `LOOP_BEG`/`LOOP_END`
pairs — the sequencer manages PC tracking without software overhead.

## AXI4 Interfaces

### Control Plane (AXI4-Lite Slave)

- Register-mapped control and status.
- Program ISA instructions, set matrix dimensions, start/stop.

### Data Plane (AXI4-Full Master)

- Direct memory access for weight/activation load and result store.
- Burst transfers up to 256 beats.

## Performance Model

### TOPS Calculation

```
Peak TOPS = 2 × ROWS × COLS × frequency_GHz
```

The factor of 2 accounts for one multiply and one add per MAC operation.

### Power Estimation

```
P_dynamic = α × C × V² × f
P_total   = P_dynamic + P_static  (P_static ≈ 0.1 × P_dynamic)
Energy    = P_total / (2 × ROWS × COLS × f)  [Joules per operation]
```

| Parameter | Symbol | Typical Value     |
|-----------|--------|-------------------|
| Activity factor | α | 0.15 |
| Capacitance | C | 50 pF + 0.002 pF/PE |
| Supply voltage | V | 0.75 V |

## MLPerf Benchmarks

Supported MLPerf inference benchmarks:
- BERT-Large (SQuAD)
- ResNet-50 v1.5
- SSD-ResNet34
- Transformer-XL
- DLRM

## References

1. Jouppi et al., "In-Datacenter Performance Analysis of a Tensor Processing Unit", ISCA 2017
2. Chen et al., "Eyeriss: An Energy-Efficient Reconfigurable Accelerator for Deep CNNs", JSSC 2017
3. Samajdar et al., "SCALE-Sim: Systolic CNN Accelerator Simulator", ISPASS 2018
