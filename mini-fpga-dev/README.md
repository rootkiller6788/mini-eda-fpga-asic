# mini-fpga-dev - FPGA Development & Architecture (C ??)

> ?? UT Austin ECE 382V (VLSI Design Automation), MIT 6.371 (VLSI Design), CMU 18-760 (Digital Design Automation), Stanford EE272 (EDA Tools)

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (3 applications)
- L8: Partial (5/7 advanced topics implemented)
- L9: Partial (documented + partial implementation)

## ????

| ?? | ??? | ?? |
|------|--------|------|
| `include/` | 9 | 1,423 |
| `src/` | 9 | 4,066 |
| `tests/` | 9 | ~1,200 |
| **?? (include+src)** | **18** | **5,489 ? 3,000** |

## ?????? (L1-L9)

### L1: Core Definitions (????) - COMPLETE
| ?? | ?? | ?? |
|------|------|------|
| `FpgaLut` | `include/fpga_arch.h` | K-input Look-Up Table (??6??) |
| `FpgaFlipFlop` | `include/fpga_arch.h` | DFF/DFFE/DFFSR/DFFAR ??? |
| `FpgaClb` | `include/fpga_arch.h` | Configurable Logic Block (2 Slices x 4 LUTs) |
| `FpgaSlice` | `include/fpga_arch.h` | SLICEL/SLICEM ??? |
| `FpgaFabric` | `include/fpga_arch.h` | Island-Style FPGA ?? |
| `FpgaBitstream` | `include/bitstream_gen.h` | ???????? |
| `FpgaAtom` | `include/clb_pack.h` | ????????? |
| `FpgaRrNode` | `include/routing_fabric.h` | ??????? |
| `FpgaTimingNode` | `include/timing_fpga.h` | ????? |
| `FpgaIoPad` | `include/io_planning.h` | I/O ???? |
| `FpgaFlow` | `include/fpga_flow.h` | ?????? |

### L2: Core Concepts (????) - COMPLETE
| ?? | ?? | ?? |
|------|------|------|
| LUT-Based Logic | `src/fpga_arch.c` | `fpga_lut_eval()`, Shannon ???? |
| Island-Style FPGA | `src/fpga_arch.c` | 2D CLB ?? + ???? |
| Configuration Memory | `src/bitstream_gen.c` | ?????? (FAR) ??? |
| Switch Matrix Routing | `src/routing_fabric.c` | Wilton/Disjoint/Universal ?? |
| Static Timing Analysis | `src/timing_fpga.c` | Arrival/Required/Slack ?? |
| Technology Mapping | `src/techmap_fpga.c` | LUT ??, ???? |
| CLB Packing | `src/clb_pack.c` | T-VPack ???? |
| IO Bank Architecture | `src/io_planning.c` | HP/HR/HD Bank ?? |

### L3: Engineering Structures (????) - COMPLETE
| ?? | ?? | ?? |
|------|------|------|
| Grid-based Tile Array | `src/fpga_arch.c` | tile[x][y] 2D ?? |
| Routing Channel Network | `src/fpga_arch.c` | H-channels + V-channels ?? |
| Routing Resource Graph | `src/routing_fabric.c` | SOURCE/SINK/CHANX/CHANY ??? |
| Configuration Frame | `src/bitstream_gen.c` | 101-word ? 32-bit ??? |
| End-to-End Flow Pipeline | `src/fpga_flow.c` | 8-stage ???? |

### L4: Standards/Theorems (??/??) - COMPLETE
| ?? | ?? | ?? | ?? |
|------|------|------|------|
| Shannon Expansion | f = x_i?f\|xi=1 + x_i'?f\|xi=0 | `src/techmap_fpga.c` | `shannon_decompose()` |
| Rent's Rule | T = A ? N^p | `src/fpga_arch.c` | `fpga_rent_io_estimate()` |
| Elmore Delay Model | ? = ? R_k ? C_downstream_k | `src/timing_fpga.c` | `elmore_delay()` |
| CRC-32 | IEEE 802.3 polynomial | `src/bitstream_gen.c` | `bitstream_compute_crc()` |
| Setup/Hold Constraints | T_clk ? t_cq + t_logic + t_su | `src/timing_fpga.c` | `sta_check_setup()` |
| HPWL Theorem | HPWL ? WL ? (p/2)?HPWL | `src/place_fpga.c` | `placement_hpwl()` |
| Moore Graph Bound | W_min ? ???N | `src/fpga_arch.c` | `fpga_min_channel_width()` |
| Transmission Line | ? = (Z_L - Z_0)/(Z_L + Z_0) | `src/io_planning.c` | `io_reflection_coefficient()` |

### L5: Algorithms/Methods (??/??) - COMPLETE
| ?? | ??? | ?? | ?? |
|------|--------|------|------|
| **FlowMap** | O(N?K?) | `src/techmap_fpga.c` | Cong & Ding, TCAD 1994 |
| **T-VPack** | O(N?) | `src/clb_pack.c` | Betz & Rose, FPGA 1998 |
| **Simulated Annealing** | O(I?M?N) | `src/place_fpga.c` | Kirkpatrick et al., Science 1983 |
| **Lee's Maze (BFS)** | O(V+E) | `src/routing_fabric.c` | Lee, IRE Trans. EC 1961 |
| **A* Search** | O(N log N) | `src/routing_fabric.c` | Hart et al., IEEE TSSC 1968 |
| **PathFinder** | O(I?N?V) | `src/routing_fabric.c` | McMurchie & Ebeling, FPGA 1995 |
| **CPM (STA)** | O(V+E) | `src/timing_fpga.c` | Kirkpatrick & Clark, IBM JRD 1966 |
| **Quadratic Placement** | O(I?N?) | `src/place_fpga.c` | Kleinhans et al., TCAD 1991 |
| **DAGMap** | O(N?K?) | `src/techmap_fpga.c` | Chen & Cong, IEEE D&T 2001 |
| **RLE Compression** | O(N?F) | `src/bitstream_gen.c` | ?????? |

### L6: Canonical Problems (??????) - COMPLETE
| ?? | ???? | ?? |
|------|---------|------|
| End-to-End FPGA Flow | `examples/fpga_demo.c` | ?? 8-stage ???? |
| Bitstream Generation | `src/bitstream_gen.c` | ????? + CRC ?? |
| I/O Ring Planning | `src/io_planning.c` | ? SSO ????? I/O ?? |
| Full-Chip Routing | `src/routing_fabric.c` | PathFinder ???? |
| Timing Closure | `src/timing_fpga.c` | Setup/Hold ???? |

### L7: Applications (??) - COMPLETE (3 ?)
1. **XDC ????** (`src/io_planning.c` `io_write_pinout()`) - ?? Xilinx Design Constraints
2. **bitsream ?? I/O** (`src/bitstream_gen.c` `bitstream_write/read_file()`) - ???/HEX ??
3. **?????** (`src/fpga_flow.c` `flow_generate_download()`) - .bin + .xdc ??

### L8: Advanced Topics (????) - PARTIAL (5/7)
| ?? | ?? | ?? |
|------|------|------|
| Statistical STA (SSTA) | `src/timing_fpga.c` `ssta_propagate_arrival()` | ??? |
| Clock Domain Crossing | `src/timing_fpga.c` `cdc_mtbf()` | ??? |
| Fracturable LUT Packing (ALM) | `src/clb_pack.c` `clb_pack_fracturable()` | ??? |
| Redundant Routing | `src/routing_fabric.c` `route_with_redundancy()` | ??? |
| Multi-FPGA Partitioning | `src/place_fpga.c` `place_partition_multi_fpga()` | ??? |

### L9: Industry Frontiers (????) - PARTIAL
| ?? | ?? | ?? |
|------|------|------|
| Partial Reconfiguration | `src/bitstream_gen.c` `bitstream_generate_partial()` | ??? |
| AI-Assisted Auto-Tuning | `src/fpga_flow.c` `flow_auto_tune_place_params()` | ??? |
| ML Congestion Prediction | `src/routing_fabric.c` `route_predict_congestion()` | ??? |

## ??????

| ?? | ?? | ????? |
|------|------|-----------|
| **MIT** | 6.371 VLSI Design | FPGA ??, ???? |
| **Stanford** | EE272 EDA Tools | ?? EDA ??, ???? |
| **CMU** | 18-760 Digital Design Automation | ????, ????, ?? |
| **UT Austin** | ECE 382V VLSI Design Automation | FPGA ??, VPR ?? |
| **ETH** | 263-0006 Computer Architecture | FPGA ??, ???? |
| **Cambridge** | Part II: VLSI Design | Place & Route, STA |
| **??** | ??????? | FPGA ???? |
| **Georgia Tech** | CS 6290 HPCA | FPGA ????? |

## ?????

```bash
make          # ?????? (?? gcc)
make test     # ????????
make clean    # ??????
```

## ??

- C99 compiler (gcc/clang)
- libc + libm only
- `assert.h` (????)

## ????????

- [x] include/ + src/ ??? ? 3000 (??: 5489)
- [x] make test ????
- [x] README.md ????? COMPLETE
- [x] ? TODO/FIXME/stub/placeholder
- [x] ???????? (???????????)
- [x] L1-L6 ??? Complete
- [x] L7 ??? Partial+ (?2 applications)
- [x] L8 ??? Partial+ (?1 advanced topic)
- [x] L9 ??? Partial (???)
