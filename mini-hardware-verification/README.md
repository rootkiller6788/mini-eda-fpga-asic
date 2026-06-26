# mini-hardware-verification

Hardware Functional Verification Framework — UVM-alike testbench architecture,
constrained-random stimulus, coverage-driven methodology, formal property checking,
and event-driven RTL simulation.

## Module Status: COMPLETE

- **L1-L6**: Complete
- **L7**: Complete (3 applications)
- **L8**: Partial (2/5 advanced topics — SAT/BMC, k-induction)
- **L9**: Partial (documented, not implemented)

---

## Nine-Layer Knowledge Coverage

| Level | Name | Status | Key Items |
|-------|------|--------|-----------|
| **L1** | Definitions | Complete | DUT, signal, UVM components, TLM types, covergroup/bin, assertion, SAT literal, transition system, event queue |
| **L2** | Core Concepts | Complete | Stratified event queue, TLM ports, CDV methodology, CSP formalism, temporal logic (LTL/CTL), UVM phasing, verification plan |
| **L3** | Engineering Structures | Complete | UVM component hierarchy, BMC unrolling engine, stratified simulation kernel, event-driven architecture, timing wheel |
| **L4** | Standards/Theorems | Complete | IEEE 1800.2-2020 UVM, IEEE 1800-2017 SVA, IEEE 1364 simulation semantics, Cook-Levin Theorem (SAT is NP-complete), Amdahl's Law |
| **L5** | Algorithms/Methods | Complete | DPLL/CDCL SAT solving, BMC (Biere 1999), k-induction (Sheeran 2000), round-robin arbitration, backtracking CSP, xorshift128+ PRNG, delta-cycle processing |
| **L6** | Canonical Problems | Complete | FIFO verification, RISC-V ALU verification, AXI bus protocol verification |
| **L7** | Applications | Complete | RISC-V core verification env, AXI4-Lite protocol checker, waveform dumping (VCD-style) |
| **L8** | Advanced Topics | Partial | SAT-based bounded model checking, k-induction for unbounded proofs, timing wheel (Varghese & Lauck), coverage gap analysis |
| **L9** | Industry Frontiers | Partial | AI-driven coverage closure (doc only), ML-based assertion generation (doc only), emulation verification (doc only) |

---

## Core Definitions (L1)

- **DUT** (`hv_dut_t`): Device Under Test descriptor with signals, eval callback
- **Signal** (`hv_signal_t`): 4-state logic signal (0/1/X/Z) with force/release
- **Transaction** (`hv_transaction_t`): TLM generic payload with addr/data/cmd/resp
- **UVM Components**: Monitor, Driver, Sequencer, Scoreboard, Agent, Environment
- **TLM**: Port/Export/Imp for transaction-level communication
- **Coverage**: Covergroup, Coverpoint, Bin (auto/manual/transition/cross/illegal)
- **Constraint**: Random variable, constraint block, CSP formalism
- **Assertion**: Immediate/concurrent, sequence, property, implication
- **SAT Solver**: Variable, literal, clause, DPLL state
- **Transition System**: State, transition, Kripke structure
- **Simulation**: Stratified event queue (Active/Inactive/NBA/Monitor/Future)

---

## Core Theorems (L4)

### Cook-Levin Theorem (1971)
**Statement**: SAT is NP-complete. Every problem in NP can be reduced to SAT in polynomial time.

**Application in BMC**: Bounded model checking reduces the model checking problem (M |= phi) to SAT:
```
M |=_k phi  iff  SAT( BMC_enc(M, phi, k) ) is UNSAT
```
Where `BMC_enc` encodes the k-step unrolling of the transition relation and
the negation of the property as a propositional formula.

**Code**: `formal_proof.c:bmc_encode_to_sat()`

### k-Induction Principle (Sheeran et al., 2000)
For safety property P:
- **Base**: P holds for initial states up to depth k
- **Step**: If P holds for k consecutive reachable states, then P holds for the (k+1)-th state
- If both base and step are SAT-UNSAT, P is an invariant.

**Code**: `formal_proof.c:k_ind_prove()`

### IEEE 1364 Stratified Event Queue
The Verilog simulation semantics guarantee determinism through event stratification:
```
Active -> Inactive -> NBA -> Monitor -> Future -> (next time step)
```
Each region is fully processed before moving to the next, ensuring zero-delay
glitches cannot cause non-deterministic behavior.

**Code**: `simulation_core.c:hv_sim_process_delta()`

---

## Core Algorithms (L5)

| Algorithm | Source | Complexity | Reference |
|-----------|--------|-----------|-----------|
| DPLL/CDCL SAT | `formal_proof.c` | O(2^n) worst-case, polynomial average | Davis et al. 1962 |
| BMC unrolling | `formal_proof.c` | O(k * |M|) encoding | Biere et al. 1999 |
| k-Induction | `formal_proof.c` | O(k * |M|) per step | Sheeran et al. 2000 |
| Round-Robin Arb. | `uvm_components.c` | O(n) per arbitration | - |
| Backtracking CSP | `constraint_solver.c` | O(d^n) worst-case | - |
| xorshift128+ PRNG | `constraint_solver.c` | O(1) per call | Vigna 2014 |
| Delta-cycle process | `simulation_core.c` | O(E) per delta | IEEE 1364 |
| Coverage gap analysis | `coverage_model.c` | O(G * C * B) | Piziali 2004 |

---

## Canonical Problems (L6)

1. **FIFO Verification** (`examples/example_fifo_verify.c`)
   - DUT model, assertions (full/empty mutex), coverage, simulation
2. **RISC-V ALU Verification** (`examples/example_riscv_verify.c`)
   - Constrained random, scoreboard, coverage model, formal BMC
3. **AXI Bus Protocol Verification** (`examples/example_bus_verify.c`)
   - Protocol handshake checking, constraint-random addresses, cross-coverage

---

## Course Mapping

| School | Course | Topic Coverage |
|--------|--------|---------------|
| **MIT** | 6.004 Computation Structures | RTL verification, simulation semantics |
| **UT Austin** | ECE 382V VLSI Verification | UVM, CDV, constrained random, assertions |
| **CMU** | 15-414 Bug Catching | SAT, BMC, formal methods |
| **CMU** | 18-240 Digital Systems | Testbench methodology |
| **Stanford** | EE 272 Design Projects | Verification planning |
| **ETH** | 263-0006 Computer Architecture | RTL verification |
| **Berkeley** | CS 152 RISC-V Architecture | Processor verification |
| **Cambridge** | Part II Concurrent Systems | Protocol verification |

---

## Build & Test

```bash
make          # build library + tests + examples
make test     # run all tests (one-click pass)
make examples # build examples only
make clean    # remove build artifacts
make count    # line count statistics
```

**Requirements**: GCC (C11), GNU Make, libm.

---

## Directory Structure

```
mini-hardware-verification/
├── Makefile              # Build system
├── README.md             # This file
├── include/              # 6 header files
│   ├── hw_verify.h       # Core framework (DUT, signal, plan, config)
│   ├── uvm_components.h  # UVM hierarchy (Monitor, Driver, Sequencer, ...)
│   ├── formal_proof.h    # Formal verification (SAT, BMC, k-induction, LTL)
│   ├── coverage_model.h  # CDV (covergroup, coverpoint, bin, cross)
│   ├── constraint_solver.h # Constrained random (CSP, PRNG)
│   ├── assertion_engine.h  # SVA engine
│   └── simulation_core.h   # Event-driven sim kernel
├── src/                  # 6 source files
│   ├── hw_verify.c
│   ├── uvm_components.c
│   ├── formal_proof.c
│   ├── coverage_model.c
│   ├── constraint_solver.c
│   ├── assertion_engine.c
│   └── simulation_core.c
├── tests/
│   └── test_all.c        # Comprehensive test suite (11 tests)
├── examples/
│   ├── example_fifo_verify.c   # FIFO verification
│   ├── example_riscv_verify.c  # RISC-V ALU verification
│   └── example_bus_verify.c    # AXI bus verification
├── demos/
├── benches/
└── docs/
```
