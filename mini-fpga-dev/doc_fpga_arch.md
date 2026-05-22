# FPGA Architecture Reference

## Overview

The FPGA architecture model implements the fundamental hardware primitives found in modern SRAM-based FPGAs (Xilinx 7-series / UltraScale compatible).

## Architecture Components

### 1. LUT (Look-Up Table)

**6-input LUT** — the basic combinational element. Each LUT can implement any 6-input boolean function via its 64-bit truth table (config mask).

```
Inputs: I5 I4 I3 I2 I1 I0  →  O (1-bit output)
```

- LUT5 mode: largest LUT using 2 x LUT5 + MUXF7
- LUT6 mode: single 6-input function (default)
- ROM mode: configured as small synchronous ROM

**API:**
- `fpga_lut_init()` — Initialize
- `fpga_lut_set_mask(mask)` — Set truth table (64-bit)
- `fpga_lut_eval(inputs)` — Combinational evaluation
- `fpga_lut_set_input(idx, signal)` — Route an input signal

### 2. Flip-Flop (Flip-Flop)

D-type register with optional enable, synchronous/asynchronous set/reset.

**Types:**
- `FF_TYPE_DFF` — Simple D flip-flop
- `FF_TYPE_DFFE` — DFF with clock enable (CE)
- `FF_TYPE_DFFSR` — DFF with set/reset inputs

**API:**
- `fpga_ff_init(ff, type)` — Initialize with type
- `fpga_ff_reset()` — Reset to initial value
- `fpga_ff_clock(data, en, set, reset)` — Positive-edge clock
- `fpga_ff_get_q()` — Read Q output

### 3. Block RAM (BRAM)

True dual-port synchronous RAM with configurable width/depth.

**Configuration:**
- Depth: 512 (36 Kb)
- Width: 36-bit (x36), 18-bit (x18), 9-bit (x9), 4-bit (x4), 2-bit (x2), 1-bit (x1)
- Ports: 2 independent (A and B), each with own clock, address, data, WE, CE

**API:**
- `fpga_bram_init(bram, width_mode)` — Initialize
- `fpga_bram_write(port, addr, data)` — Write to port
- `fpga_bram_read(port, addr)` — Combinational read
- `fpga_bram_clock()` — Clock both ports

### 4. DSP Slice

Multiply-Accumulate engine with pipeline registers.

**Datapath:**
```
A (25-bit)  → [A_REG] → ┐
                         ├ → Multiplier (48-bit) → [M_REG] → [P_REG]
B (18-bit)  → [B_REG] → ┘      ↑                           ↑
C (48-bit)  ──────────────────→ (accumulate input)
```

**Opmode:**
- 0: Multiply only (P = A × B)
- 1: Multiply-Accumulate (P = A × B + C)
- 2: Multiply-Subtract (P = A × B - C)
- 3: Multiply-Accumulate with pre-add
- 4: Add only (P = A + B)

**API:**
- `fpga_dsp_init(dsp)` — Initialize
- `fpga_dsp_set_inputs(a, b, c)` — Set input operands
- `fpga_dsp_compute(opmode)` — Compute and return result
- `fpga_dsp_clock()` — Positive-edge clock

### 5. Routing Architecture

#### Switch Box
Programmable crossbar connecting wires on 4 sides of a routing tile.

**F_s (Switch Box Flexibility)**: 0.5 — each incoming wire connects to 50% of other sides.

#### Connection Box
Connects CLB input/output pins to routing tracks.

**F_c (Connection Box Flexibility)**: 0.6 — each CLB pin connects to 60% of routing tracks.

#### Routing Channel
Set of parallel wires (tracks) spanning a tile boundary.

- Width: 8 to 256 tracks
- Per-track delay: 50 ps baseline + 2.5 ps/track (modeling RC)

**API:**
- `fpga_switchbox_init(chan_w)` — Initialize switch box
- `fpga_switchbox_connect(side, track, to_side, to_track)` — Create connection
- `fpga_connbox_init(chan_w, flex, pins)` — Initialize connection box
- `fpga_connbox_bind(pin, track)` — Bind pin to track
- `fpga_channel_init(width)` — Initialize channel
- `fpga_channel_allocate(&track)` — Allocate a track
- `fpga_channel_get_delay(track)` — Get track delay

## Constants

| Parameter | Value | Description |
|-----------|-------|-------------|
| LUT_INPUT_COUNT | 6 | LUT inputs |
| BRAM_DEPTH | 512 | BRAM depth |
| BRAM_WIDTH | 36 | BRAM data width |
| DSP_A_WIDTH | 25 | DSP A operand width |
| DSP_B_WIDTH | 18 | DSP B operand width |
| CHAN_W_MIN | 8 | Minimum channel width |
| CHAN_W_MAX | 256 | Maximum channel width |
| CONN_BOX_FLEX | 0.6 | Connection box flexibility |
| SWITCH_BOX_FLEX | 0.5 | Switch box flexibility |
