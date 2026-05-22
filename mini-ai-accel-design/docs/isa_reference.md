# DNN Accelerator ISA Reference

## Instruction Encoding

Each instruction is 16 bytes (128 bits) wide.

```
Byte   [0]     [1]     [2-3]   [4-15]
       +-------+-------+-------+---------------------------+
       | Opcode| Flags | Rsvd  | Operand Payload (12 bytes)|
       +-------+-------+-------+---------------------------+
```

- **Opcode** (byte 0): Operation code, see table below.
- **Flags** (byte 1): Condition flags and modifiers.
- **Reserved** (bytes 2-3): Reserved for future use.
- **Payload** (bytes 4-15): Operand fields, format depends on opcode.

## Opcode Table

| Value | Mnemonic    | Description                                    | Latency |
|-------|------------|------------------------------------------------|---------|
| 0x00  | NOP        | No operation, advance PC                       | 1       |
| 0x01  | LOAD_W     | Load weight matrix to systolic array           | DMA     |
| 0x02  | LOAD_A     | Load activation vector/matrix                  | DMA     |
| 0x03  | MATMUL     | Execute matrix multiply on loaded operands     | M+N+K   |
| 0x04  | ACT        | Apply activation function element-wise         | N       |
| 0x05  | STORE      | Store result matrix to memory                  | DMA     |
| 0x06  | SYNC       | Full pipeline synchronization barrier           | 1       |
| 0x07  | LOOP_BEG   | Begin hardware loop (decrement-and-branch)     | 1       |
| 0x08  | LOOP_END   | End hardware loop body                         | 1       |
| 0x09  | BARRIER    | Named cross-thread barrier                     | var     |
| 0x0A  | DMA_LD     | Direct memory access load                      | DMA     |
| 0x0B  | DMA_ST     | Direct memory access store                     | DMA     |
| 0x0C  | SET_TILE   | Configure tiling parameters                    | 1       |
| 0x0D  | WAIT       | Wait for named barrier                         | var     |
| 0x0E  | HALT       | Halt execution / end of program                | 1       |

## Instruction Formats

### NOP (0x00)

```
Opcode: 0x00
Bytes 1-15: ignored
```

No operation. Advances PC.

---

### LOAD_W (0x01) — Load Weights

```
[0]      [1]     [2-3]   [4-7]     [8-9]    [10-11]  [12-15]
+-------+-------+-------+---------+--------+--------+----------+
| 0x01  | flags | rsvd  | addr    | rows   | cols   | reserved |
+-------+-------+-------+---------+--------+--------+----------+
```

| Field  | Bits  | Description                          |
|--------|-------|--------------------------------------|
| addr   | 4-7   | Source address (byte offset in DRAM) |
| rows   | 8-9   | Number of rows (M)                   |
| cols   | 10-11 | Number of columns (K)                |

Loads `rows × cols` weight bytes from `addr`.

---

### LOAD_A (0x02) — Load Activations

```
[0]      [1]     [2-3]   [4-7]     [8-9]    [10-11]  [12-15]
+-------+-------+-------+---------+--------+--------+----------+
| 0x02  | flags | rsvd  | addr    | len    | rsvd   | reserved |
+-------+-------+-------+---------+--------+--------+----------+
```

| Field  | Bits  | Description                          |
|--------|-------|--------------------------------------|
| addr   | 4-7   | Source address                       |
| len    | 8-9   | Number of activation bytes           |

---

### MATMUL (0x03) — Matrix Multiply

```
[0]      [1]     [2-3]   [4-5]    [6-7]    [8-9]    [10-15]
+-------+-------+-------+--------+--------+--------+----------+
| 0x03  | flags | rsvd  | M      | N      | K      | reserved |
+-------+-------+-------+--------+--------+--------+----------+
```

| Field  | Bits  | Description                |
|--------|-------|----------------------------|
| M      | 4-5   | Output rows (weight rows)  |
| N      | 6-7   | Output cols (activation cols) |
| K      | 8-9   | Inner dimension / reduction |

Computes: `C[M×N] = A[M×K] × B[K×N]`

---

### ACT (0x04) — Activation Function

```
[0]      [1]     [2-3]   [4]      [5]     [6-7]    [8-15]
+-------+-------+-------+--------+--------+--------+----------+
| 0x04  | flags | rsvd  | fn     | rsvd   | len    | reserved |
+-------+-------+-------+--------+--------+--------+----------+
```

| Field  | Bits  | Description          |
|--------|-------|----------------------|
| fn     | 4     | Activation function  |
| len    | 6-7   | Number of elements   |

Activation Function Codes:

| Code | Function  | Formula                      |
|------|----------|------------------------------|
| 0    | ReLU     | max(0, x)                    |
| 1    | Sigmoid  | 1 / (1 + exp(-x))            |
| 2    | Tanh     | tanh(x)                      |
| 3    | GELU     | x * Phi(x)                   |
| 4    | Swish    | x * sigmoid(x)               |
| 5    | LeakyReLU| x if x>0 else 0.01x          |
| 6    | None     | Identity (pass-through)      |

---

### STORE (0x05) — Store Result

```
[0]      [1]     [2-3]   [4-7]     [8-9]    [10-11]  [12-15]
+-------+-------+-------+---------+--------+--------+----------+
| 0x05  | flags | rsvd  | addr    | len    | rsvd   | reserved |
+-------+-------+-------+---------+--------+--------+----------+
```

| Field  | Bits  | Description                     |
|--------|-------|---------------------------------|
| addr   | 4-7   | Destination address             |
| len    | 8-9   | Number of result bytes to store |

---

### SYNC (0x06) — Synchronization

```
[0]      [1]     [2-3]   [4-15]
+-------+-------+-------+----------+
| 0x06  | flags | rsvd  | reserved |
+-------+-------+-------+----------+
```

Full pipeline stall until all in-flight operations complete.

---

### LOOP_BEG (0x07) — Loop Begin

```
[0]      [1]     [2-3]   [4-5]    [6-7]     [8-15]
+-------+-------+-------+--------+---------+----------+
| 0x07  | flags | rsvd  | count  | stride  | reserved |
+-------+-------+-------+--------+---------+----------+
```

| Field  | Bits  | Description                      |
|--------|-------|----------------------------------|
| count  | 4-5   | Loop iteration count             |
| stride | 6-7   | Address/offset stride per iter   |

Starts a hardware-counted loop. Max nest depth: 6.

---

### LOOP_END (0x08) — Loop End

```
[0]      [1]     [2-3]   [4-15]
+-------+-------+-------+----------+
| 0x08  | flags | rsvd  | reserved |
+-------+-------+-------+----------+
```

Decrements innermost loop counter. If counter > 0, jumps back to matching `LOOP_BEG`.

---

### BARRIER (0x09) — Named Barrier

```
[0]      [1]     [2-3]   [4-5]    [6-15]
+-------+-------+-------+--------+----------+
| 0x09  | flags | rsvd  | id     | reserved |
+-------+-------+-------+--------+----------+
```

| Field  | Bits  | Description           |
|--------|-------|-----------------------|
| id     | 4-5   | Barrier identifier    |

---

### DMA_LD (0x0A) — DMA Load

```
[0]      [1]     [2-3]   [4-7]    [8-11]   [12-13]  [14-15]
+-------+-------+-------+--------+--------+--------+----------+
| 0x0A  | flags | rsvd  | src    | dst    | len    | reserved |
+-------+-------+-------+--------+--------+--------+----------+
```

| Field  | Bits  | Description              |
|--------|-------|--------------------------|
| src    | 4-7   | Source address (DRAM)    |
| dst    | 8-11  | Destination (SRAM buffer)|
| len    | 12-13 | Transfer length (bytes)  |

---

### DMA_ST (0x0B) — DMA Store

```
[0]      [1]     [2-3]   [4-7]    [8-11]   [12-13]  [14-15]
+-------+-------+-------+--------+--------+--------+----------+
| 0x0B  | flags | rsvd  | src    | dst    | len    | reserved |
+-------+-------+-------+--------+--------+--------+----------+
```

Same as DMA_LD but direction is SRAM to DRAM.

---

### SET_TILE (0x0C) — Configure Tiling

```
[0]      [1]     [2-3]   [4-5]    [6-7]    [8-9]    [10-15]
+-------+-------+-------+--------+--------+--------+----------+
| 0x0C  | flags | rsvd  | Tm     | Tn     | Tk     | reserved |
+-------+-------+-------+--------+--------+--------+----------+
```

| Field  | Bits  | Description         |
|--------|-------|---------------------|
| Tm     | 4-5   | M-dimension tile    |
| Tn     | 6-7   | N-dimension tile    |
| Tk     | 8-9   | K-dimension tile    |

---

### WAIT (0x0D) — Wait for Barrier

```
[0]      [1]     [2-3]   [4-5]    [6-15]
+-------+-------+-------+--------+----------+
| 0x0D  | flags | rsvd  | id     | reserved |
+-------+-------+-------+--------+----------+
```

Stall until the named barrier is released.

---

### HALT (0x0E) — Halt

```
[0]      [1]     [2-3]   [4-15]
+-------+-------+-------+----------+
| 0x0E  | flags | rsvd  | reserved |
+-------+-------+-------+----------+
```

Stop execution. Sets the `halted` status flag.

## Programming Examples

### Tiled Matrix Multiplication (M×N = A[M×K] × B[K×N])

```
SET_TILE    64, 64, 64        // Tm=64, Tn=64, Tk=64
LOOP_BEG    K/64, 64          // iterate over K tiles
  LOOP_BEG  M/64, 64          // iterate over M tiles
    LOOP_BEG N/64, 64          // iterate over N tiles
      LOAD_W   &A[m_tile][k_tile], 64, 64
      LOAD_A   &B[k_tile][n_tile], 64
      MATMUL   64, 64, 64
      ACT      ReLU, 4096
      STORE    &C[m_tile][n_tile], 4096
    LOOP_END
  LOOP_END
LOOP_END
SYNC
HALT
```

### Convolution (im2col + MatMul)

```
// Convert convolution to matrix multiply via im2col
// im2col: (H_out * W_out, K * K * C_in)

LOAD_W   &weight_matrix, C_out, K*K*C_in     // Weight matrix
LOAD_A   &im2col_matrix, K*K*C_in, H_out*W_out // Input matrix
MATMUL   C_out, H_out*W_out, K*K*C_in
ACT      ReLU, C_out*H_out*W_out
STORE    &output, C_out*H_out*W_out
```

### Attention (Multi-Head Self-Attention)

```
// Q = X × W_Q
LOAD_W   &W_Q, seq_len, d_head   // Weight
LOAD_A   &X, d_model, seq_len    // Input (heads processed independently)
MATMUL   seq_len, d_head, d_model
STORE    &Q, seq_len * d_head

// K = X × W_K  (same pattern, different weight)
LOAD_W   &W_K, seq_len, d_head
MATMUL   seq_len, d_head, d_model

// V = X × W_V
LOAD_W   &W_V, seq_len, d_head
MATMUL   seq_len, d_head, d_model

// Attention = softmax(Q × K^T / sqrt(d_head)) × V
// (Softmax and scaling handled by host or activation unit)
```

## Performance Counters

| Counter          | Description                           |
|------------------|---------------------------------------|
| `cycle`          | Total clock cycles since start        |
| `inst_retired`   | Instructions completed                |
| `mac_counter`    | Total MAC operations executed         |
| `idle_cycles`    | Cycles with no active PE computation  |
| `skip_count`     | Zero-skipped operations               |

## Register Map (AXI4-Lite)

| Offset | Register      | Width | Access | Description           |
|--------|---------------|-------|--------|-----------------------|
| 0x00   | CTRL          | 32    | R/W    | Control (bit 0: start)|
| 0x04   | STATUS        | 32    | R      | Status                |
| 0x08   | W_ADDR        | 32    | R/W    | Weight base address   |
| 0x0C   | I_ADDR        | 32    | R/W    | Input base address    |
| 0x10   | O_ADDR        | 32    | R/W    | Output base address   |
| 0x14   | M_DIM         | 32    | R/W    | Matrix M dimension    |
| 0x18   | N_DIM         | 32    | R/W    | Matrix N dimension    |
| 0x1C   | K_DIM         | 32    | R/W    | Matrix K dimension    |
| 0x20   | PERF_CNT      | 32    | R      | Performance counter   |
| 0x24   | POWER_STAT    | 32    | R      | Power/energy status   |

## Error Codes

| Code  | Error                 |
|-------|-----------------------|
| 0x00  | No error              |
| 0x01  | Instruction queue overflow |
| 0x02  | Invalid opcode        |
| 0x03  | DMA timeout           |
| 0x04  | Loop nest overflow    |
| 0x05  | MAC overflow          |
| 0x06  | Buffer full           |
| 0x0F  | Unknown error         |
