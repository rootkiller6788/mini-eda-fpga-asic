# NoC Design Guide

## Architecture Overview

```
+------+  +------+  +------+
| Core |  | Core |  | Core |     PE (Processing Elements)
|  0   |  |  1   |  |  2   |
+--+---+  +--+---+  +--+---+
   |         |         |          NI (Network Interface / AXI Bridge)
+--+----+----+----+----+----+
|  R0   |  R1   |  R2   |  R3  |  Router nodes
+-------+-------+-------+------+
|  R4   |  R5   |  R6   |  R7  |
+-------+-------+-------+------+
|  R8   |  R9   |  R10  |  R11 |
+-------+-------+-------+------+
|  R12  |  R13  |  R14  |  R15 |  4x4 Mesh example
+-------+-------+-------+------+
```

## Router Microarchitecture (5-Stage Pipeline)

```
 IN ----> [RC] --> [VA] --> [SA] --> [ST] --> [LT] ---> OUT
           ^        ^        ^        ^        ^
           |        |        |        |        |
    Route   Virtual   Switch   Switch   Link
    Compute Channel   Alloc    Traversal Traversal
           Alloc
```

### Stage Descriptions

| Stage | Name | Function |
|-------|------|----------|
| RC | Route Computation | Determine output port from header destination |
| VA | VC Allocation | Assign output VC to incoming flit |
| SA | Switch Allocation | Grant crossbar access (input→output) |
| ST | Switch Traversal | Flit passes through crossbar |
| LT | Link Traversal | Flit travels to next router |

### Pipeline Optimizations

**2-Stage Pipeline** (Speculative):
- Stage 0: RC + VA (speculative on SA grant)
- Stage 1: ST + LT
- Works when VC allocation succeeds without conflict

**3-Stage Pipeline** (Classic):
- Stage 0: RC
- Stage 1: VA
- Stage 2: SA + ST + LT

### Look-Ahead Routing
Next-hop output port computed one hop ahead. Reduces RC from critical path.

## AXI4-to-NoC Bridge

```
AXI Master --> [AW/W/B] --+--> [Flit Encoder] --> NoC
              [AR/R]    --+
AXI Slave  <-- [AW/W/B] --+--> [Flit Decoder] <-- NoC
              [AR/R]    --+
```

Mapping:
- AXI address → NoC destination node (via address decode)
- AXI transaction ID → NoC VC assignment
- AXI burst length → NoC packet size (flit count)
- AXI response → NoC tail flit status

## VC & Wormhole Switching

### Deadlock Avoidance
- **Escape VC (VC0)**: Guaranteed forward progress, used when congestion detected
- **Adaptive VCs (VC1-3)**: Performance-oriented, may stall

### Wormhole Flow
```
Packet: [H][B1][B2][B3][T]
         |   |   |   |   |
         v   v   v   v   v
Router:  RC  --forward--> next flits follow same path
```

### Credit-Based Flow Control
```
Upstream has N credits (buffer slots)
Each flit sent consumes 1 credit
Credit returned when downstream buffer slot freed
Send iff credits > 0
```

## Performance Metrics

| Metric | Formula | Unit |
|--------|---------|------|
| Latency | T_delivery - T_injection | cycles |
| Throughput | (total_flits × width) / (total_time) | Gbps |
| Injection Rate | packets / (node × cycle) | flits/node/cycle |
| Saturation | point where latency spikes | injection rate |

## Build & Run

```bash
make all
./demo_noc_simulation
./demo_router_pipeline
./example_mesh_topology
./example_vc_wormhole
./example_axi_noc_bridge
```

## File Map

| File | Content | Lines |
|------|---------|-------|
| noc_topology.h/c | Topology, XY routing, deadlock | 50+130 |
| vc_wormhole.h/c | VC, wormhole, credit flow | 60+140 |
| axi_protocol.h/c | AXI4 channels, handshake | 70+140 |
| noc_performance.h/c | Latency, throughput, sweep | 60+150 |
| noc_verilog_rtl.h/c | Router uarch, pipeline | 80+160 |
| example_*.c | Usage examples (3) | 50-80 |
| demo_*.c | Full simulations (2) | 250+ |
