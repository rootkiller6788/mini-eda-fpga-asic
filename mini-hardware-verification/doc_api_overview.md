# Mini-Hardware-Verification API Overview

## Project Structure

```
mini-hardware-verification/
├── README.md
├── Makefile
│
├── uvm_methodology.h        # UVM testbench hierarchy & TLM
├── assertion_check.h        # SVA/PSL assertions & formal
├── formal_verify.h          # BMC, induction, SymbiYosys
├── coverage_mdl.h           # Code & functional coverage
├── verification_ip.h        # Protocol VIPs, BFM, scoreboard
│
├── uvm_methodology.c        # UVM implementation
├── assertion_check.c        # Assertion implementation
├── formal_verify.c          # Formal verification implementation
├── coverage_mdl.c           # Coverage implementation
├── verification_ip.c        # VIP implementation
│
├── example_uvm_tb.c         # UVM testbench example
├── example_assertion.c      # Assertion example
├── example_coverage.c       # Coverage example
│
├── demo_verif_flow.c        # Full verification flow demo
├── demo_regression.c        # Regression testing demo
│
├── doc_api_overview.md      # This file
└── doc_user_guide.md        # User guide
```

## Headers & APIs

### 1. uvm_methodology.h — UVM Methodology
```
uvm_test_t          → Test (top-level)
  └─ uvm_env_t      → Environment
       └─ uvm_agent_t  → Agent (active: driver+sequencer+monitor; passive: monitor)
            ├─ uvm_driver_t    → Drives DUT pins
            ├─ uvm_sequencer_t → Arbitrates sequences
            └─ uvm_monitor_t   → Observes DUT, sends to scoreboard
  └─ uvm_scoreboard_t → Compares expected vs actual

tlm_port_t          → TLM ports (put, get, analysis, fifo)
uvm_factory_t       → Factory with override support
uvm_phase_ctrl_t    → Phase controller (build → connect → ... → final)
```

### 2. assertion_check.h — Assertions
```
assertion_def_t     → Assertion definition (immediate / concurrent)
sva_expr_t          → SVA temporal expression
sva_sequence_step_t → SVA sequence step chain
assertion_clock_t   → Clock specification for concurrent assertions
psl_property_t      → PSL property (always, never, eventually, until)
assertion_coverage_t → Assertion coverage tracking
assertion_bank_t    → Manages multiple assertions
```

### 3. formal_verify.h — Formal Verification
```
formal_state_t      → State representation (bit-vector)
formal_transition_t → Transition relation
formal_property_t   → Safety/liveness/invariant property
formal_bmc_t        → Bounded Model Checking engine
formal_induction_t  → k-Induction proof
formal_cex_t        → Counterexample trace
symbiyosys_flow_t   → SymbiYosys flow configuration
formal_equiv_t      → Combinational equivalence check
```

### 4. coverage_mdl.h — Coverage Model
```
cov_code_item_t     → Single code coverage item (line/branch/toggle/FSM)
cov_code_db_t       → Code coverage database
cov_coverpoint_t    → Functional coverpoint with bins
cov_cross_t         → Cross coverage (2+ coverpoints)
cov_covergroup_t    → Covergroup container
cov_closure_t       → Coverage closure tracker
cov_waiver_t        → Coverage exclusion/waiver
cov_cdv_t           → Coverage-driven verification iterator
```

### 5. verification_ip.h — Verification IP
```
axi4_bfm_t          → AXI4 Bus Functional Model
axi4_trans_t        → AXI4 transaction
pcie_tlp_t          → PCIe TLP packet
ddr_mem_t           → DDR4/DDR5 memory model
vip_sb_t            → VIP scoreboard (predict + compare)
regression_suite_t  → Regression test suite
regression_reg_t    → Individual regression test
seed_mgr_t          → Random seed manager
pf_tracker_t        → Pass/fail tracker
proto_analyzer_t    → Protocol analyzer (latency, throughput)
uvc_t               → Universal Verification Component
vcfg_t              → Verification environment config
```

## Key Workflows

1. **UVM Testbench**: Create test → env → agents → drivers/sequencers/monitors → scoreboard
2. **Assertions**: Define immediate/concurrent assertions → bank them → check during simulation
3. **Formal**: Define properties → create BMC/induction engine → run → analyze counterexamples
4. **Coverage**: Create code items + coverpoints + crosses → sample → check closure
5. **Regression**: Define tests → add to suite → run with seed → report pass/fail
