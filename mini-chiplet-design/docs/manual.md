# mini-chiplet-design Documentation

## Overview

This library provides a C99 implementation for chiplet-based design modeling, covering:

| Module | Header | Source | Description |
|--------|--------|--------|-------------|
| UCIe D2D | `ucie_d2d.h` | `ucie_d2d.c` | Universal Chiplet Interconnect Express die-to-die interface |
| Interposer | `interposer_tech.h` | `interposer_tech.c` | Silicon/organic/glass interposer with TSV, microbumps, RDL |
| MCM | `mcm_integration.h` | `mcm_integration.c` | Multi-Chip Module with HBM integration |
| D2D PHY | `d2d_phy.h` | `d2d_phy.c` | Die-to-Die PHY: clocking, eye diagram, BER, PRBS |
| Thermal/Power | `thermal_power.h` | `thermal_power.c` | Thermal modeling, hotspot mitigation, PDN analysis |

## Building

```
make          # build library and all targets
make lib      # build static library only (libchiplet.a)
make examples # build example programs
make demo     # build demo programs
make clean    # clean build artifacts
make run      # build and run all demos
```

## Library Structure

```
mini-chiplet-design/
  include/           C99 header files
  src/               C99 source files
  examples/          Example programs
  demo/              Full demo programs
  docs/              Documentation
  Makefile           Build system
  README.md
```

## Key Concepts

### 1. UCIe Die-to-Die Interface

UCIe defines the physical and protocol layers for die-to-die communication:
- PHY layer: electrical signaling, lane management
- Link layer: flit framing, CRC, sequencing, retry
- Protocol layer: CXL.mem, CXL.cache, PCIe, streaming

Key specs:
- Bandwidth: 16-32 GT/s per lane
- BER < 1e-27
- Latency < 2ns
- Up to 64 lanes per link

### 2. Interposer Technologies

- **Silicon**: Best thermal expansion match to silicon dies, supports fine-pitch RDL
- **Organic (EMIB)**: Lower cost, embedded multi-die interconnect bridge
- **Glass**: Good dimensional stability, moderate cost
- **Silicon Bridge**: Localized silicon bridges for high-density die-to-die
- **RDL Fanout**: Redistribution layer on molded substrate

Key considerations:
- Warpage: CTE mismatch between interposer and die
- IR drop: resistive losses across interposer power delivery
- RDL routing: trace width, spacing, layer count

### 3. MCM Integration with HBM

HBM3 specs:
- 1024-bit wide interface per stack
- 8-Hi stack (8 layers of DRAM)
- 16 pseudo-channels per stack
- 6.4 Gbps per pin (HBM3), 9.2 Gbps (HBM3e)
- Capacity: 16-36 GB per stack

### 4. D2D PHY Design

Clocking options:
- Forwarded clock: clock sent alongside data (lowest jitter, short reach)
- Source-synchronous: clock embedded in data stream
- SerDes: serializer/deserializer for longer reach

Key measurements:
- Eye diagram: vertical opening (mV), horizontal opening (ps/UI)
- BER: bit error rate measurement via PRBS patterns
- Deskew: per-lane delay adjustment (training)

### 5. Thermal and Power Delivery

Thermal model:
- Theta_JA: junction-to-ambient thermal resistance
- Theta_JC: junction-to-case thermal resistance
- Hotspot modeling: localized high-power-density regions

Power delivery:
- IR drop across interposer and package
- Decoupling capacitance placement
- PDN impedance vs frequency
- Voltage ripple under transient load

## Example Programs

- `example_d2d_link`: UCIe link initialization, training, flit transfer
- `example_interposer`: Interposer design with die placement, routing, DRC
- `example_mcm`: MCM module build with HBM, PHY connections

## Demo Programs

- `demo_ucie`: Comprehensive UCIe demo with BER sweeps, PRBS, eye diagrams
- `demo_full_system`: Full chiplet system: interposer + MCM + D2D + thermal/power

## References

- UCIe 2.0 Specification (PCI-SIG)
- HBM3 JEDEC Standard JESD238A
- IEEE 3D-IC Test Standards
- ITRS Interconnect Roadmap
