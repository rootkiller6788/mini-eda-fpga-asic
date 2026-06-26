# mini-hardware-verification — 硬件验证 (C 语言实现)

A comprehensive hardware verification library in C99 covering the full
functional verification flow used in ASIC/FPGA design.

## Modules

| Module | Header | Description |
|--------|--------|-------------|
| UVM Methodology | `uvm_methodology.h` | Testbench hierarchy, TLM ports, factory, phases |
| Assertions | `assertion_check.h` | SVA/PSL assertions, concurrent & immediate, coverage |
| Formal Verification | `formal_verify.h` | BMC, k-induction, SymbiYosys, counterexamples |
| Coverage | `coverage_mdl.h` | Code + functional coverage, cross, CDV loop |
| Verification IP | `verification_ip.h` | AXI4/PCIe/DDR VIPs, BFM, scoreboard, regression |

## Build & Run

```bash
make all        # Build all
make demo       # Full verification flow demo
make regression # Regression testing demo
make clean
```

## UVM Testbench Hierarchy

```
uvm_test_t (test)
  └── uvm_env_t (environment)
       ├── uvm_agent_t (agent, active: driver+sequencer+monitor)
       ├── uvm_agent_t (agent, passive: monitor only)
       ├── uvm_scoreboard_t (scoreboard)
       └── tlm_port_t (TLM connections)
```

## Formal Verification Flow

```
property definition → BMC (bounded model checking)
                    → k-induction proof
                    → counterexample trace generation
                    → SymbiYosys .sby config
```

## Examples

- `example_uvm_tb.c` — Full UVM testbench with register DUT
- `example_assertion.c` — Immediate + concurrent assertions for FIFO
- `example_coverage.c` — Code + functional coverage for ALU

## Demos

- `demo_verif_flow.c` — End-to-end: UVM + assertions + coverage + formal
- `demo_regression.c` — Multi-test regression with AXI4 BFM and DDR model
