# Mini-Hardware-Verification User Guide

## Quick Start

```bash
make all          # Build all examples and demos
make test         # Run example_uvm_tb
make assertions   # Run example_assertion
make coverage     # Run example_coverage
make demo         # Run demo_verif_flow
make regression   # Run demo_regression
make clean        # Clean all outputs
```

## 1. Writing a UVM Testbench

```c
#include "uvm_methodology.h"

// 1. Create test
uvm_test_t* test = uvm_test_create("my_test");
test->seed = 42;

// 2. Create environment
uvm_env_t* env = uvm_env_create("my_env");

// 3. Create agent (active: driver + sequencer + monitor)
uvm_agent_t* ag = uvm_agent_create("my_agent", UVM_AGENT_ACTIVE);
ag->sequencer = uvm_sequencer_create("my_sqr");
ag->driver    = uvm_driver_create("my_drv", ag->sequencer);
ag->monitor   = uvm_monitor_create("my_mon");
uvm_agent_connect(ag);
uvm_env_add_agent(env, ag);

// 4. Create scoreboard
env->scoreboard = uvm_scoreboard_create("my_sb", my_compare_fn);

// 5. Run test
uvm_test_set_env(test, env);
uvm_test_set_run_handler(test, my_test_run);
uvm_test_run(test);
uvm_test_report(test);

// 6. Cleanup
uvm_test_destroy(test);
```

### Factory Override Example

```c
uvm_factory_t* f = uvm_factory_get();
uvm_factory_register(f, "my_driver",      my_driver_create_fn);
uvm_factory_register(f, "my_driver_v2",   my_driver_v2_create_fn);
uvm_factory_set_override(f, "my_driver", "my_driver_v2", my_driver_v2_create_fn);

// Now creating "my_driver" will return "my_driver_v2" instead
void* comp = uvm_factory_create_component(f, "my_driver", "inst");
```

## 2. Using Assertions

### Immediate Assertion

```c
assertion_def_t* a = assertion_create("data_valid", ASSERT_IMMEDIATE);
assertion_set_severity(a, ASSERT_SEVERITY_ERROR);

// In simulation loop:
ASSERT_IMMEDIATE_COND(a, (data != 0), ASSERT_SEVERITY_ERROR, "data must not be 0");
```

### Concurrent (SVA-style) Assertion

```c
assertion_def_t* a = assertion_create("req_ack_handshake", ASSERT_CONCURRENT);
assertion_set_clock(a, my_clk);

sva_expr_t* req = sva_expr_create("req", eval_req, &dut);
sva_expr_t* ack = sva_expr_create("ack", eval_ack, &dut);

sva_sequence_step_t* ant = sva_seq_step_create(req, SEQ_CYCLE_DELAY, 1);
sva_sequence_step_t* con = sva_seq_step_create(ack, SEQ_CYCLE_DELAY, 0);

a->antecedent = ant;
a->consequent = con;

// Check during simulation:
assert_concurrent_tick(a, &dut);
```

### PSL Properties

```c
psl_property_t* p = psl_property_create("never_fifo_overflow", PSL_NEVER);
p->left  = full_expr;
p->right = wr_en_expr;
p->clock = my_clk;

bool holds = psl_property_check(p, &dut);
```

### Assertion Coverage

```c
assertion_coverage_t* cov = assertion_coverage_create("cover_write");
assertion_coverage_record(cov, my_assertion);
assertion_coverage_report(cov);
```

## 3. Formal Verification

### BMC (Bounded Model Checking)

```c
formal_property_t* prop = formal_property_create(
    "fifo_no_overflow", FORMAL_PROP_SAFETY, safety_check_fn, NULL);

formal_transition_t* trans = formal_trans_create("fifo_step", fifo_step_fn, NULL);

formal_init_t init;
init.is_init = fifo_init_fn;
init.ctx = NULL;

formal_bmc_t* bmc = formal_bmc_create();
formal_bmc_set_bound(bmc, 100);
formal_bmc_set_transition(bmc, trans);
formal_bmc_set_initial(bmc, &init);
formal_bmc_add_property(bmc, prop);

formal_result_t res = formal_bmc_run(bmc);
if (res == FORMAL_RESULT_FAIL) {
    const formal_cex_t* cex = formal_bmc_get_cex(bmc);
    formal_cex_print(cex, stdout);
    formal_cex_export_vcd(cex, "cex.vcd");
}
```

### k-Induction

```c
formal_ind_t* ind = formal_induction_create();
formal_induction_set_depth(ind, 50);
formal_induction_set_transition(ind, trans);
formal_induction_set_initial(ind, &init);
formal_induction_add_property(ind, prop);

formal_result_t res = formal_induction_prove(ind);
// PASS = property proven by induction
```

### SymbiYosys Flow

```c
symbiyosys_flow_t* flow = symbiyosys_create();
symbiyosys_add_design(flow, "counter.v");
symbiyosys_set_top(flow, "counter");
symbiyosys_set_mode(flow, SYMBI_MODE_PROVE);
symbiyosys_set_engine(flow, FORMAL_ENGINE_SMT);
symbiyosys_set_solver(flow, SOLVER_Z3);
symbiyosys_add_property(flow, prop);
symbiyosys_generate_config(flow);
symbiyosys_run(flow);
```

## 4. Coverage Model

### Code Coverage

```c
cov_code_db_t* db = cov_code_db_create();

cov_code_item_t* line = cov_code_item_create("alu.v:42", COV_CODE_LINE, "alu.v", 42);
cov_code_db_add(db, line);

cov_code_item_t* branch = cov_code_item_create("alu.v:branch42", COV_CODE_BRANCH, "alu.v", 42);
cov_code_db_add(db, branch);

// Mark hits during simulation:
cov_code_item_hit(line);
cov_code_item_hit(branch);

cov_code_db_update_metrics(db);
cov_code_db_report(db, stdout);
```

### Functional Coverage

```c
cov_covergroup_t* cg = cov_covergroup_create("cg_alu_ops");
cg->goal = 95.0;

cov_coverpoint_t* cp_op = cov_coverpoint_create("opcode", 8);
cov_coverpoint_set_range(cp_op, 0.0, 7.0);
cov_coverpoint_set_at_least(cp_op, 1);

cov_coverpoint_t* cp_res = cov_coverpoint_create("result", 4);
cov_coverpoint_set_range(cp_res, 0.0, 255.0);

cov_cross_t* cross = cov_cross_create("op_x_res");
cov_cross_add_coverpoint(cross, cp_op);
cov_cross_add_coverpoint(cross, cp_res);
cov_cross_build(cross);

cov_covergroup_add_coverpoint(cg, cp_op);
cov_covergroup_add_coverpoint(cg, cp_res);
cov_covergroup_add_cross(cg, cross);

// Sampling:
cov_coverpoint_sample(cp_op, opcode_value);
cov_coverpoint_sample(cp_res, result_value);
cov_covergroup_sample(cg);
```

### Coverage-Driven Verification (CDV)

```c
cov_closure_t* closure = cov_closure_create();
cov_closure_add_group(closure, cg_alu);
cov_closure_set_code_db(closure, db);
cov_closure_set_goals(closure, 100.0, 95.0);

cov_cdv_t* cdv = cov_cdv_create(closure, 12345);
while (!cov_cdv_has_converged(cdv)) {
    run_random_test(cdv);
    cov_cdv_iterate(cdv);
}
```

### Waivers

```c
cov_waiver_t* w = cov_waiver_create("cp_data[7]", "Bin 7 unused by design", "team");
cov_waiver_apply_to_coverpoint(w, cp_data);
// or for code coverage:
cov_waiver_apply_to_item(w, code_item_42);
```

## 5. Verification IP (VIP)

### AXI4 BFM

```c
uint64_t mem[256] = {0};
axi4_bfm_t* bfm = axi4_bfm_create("axi_master", true);
axi4_bfm_set_memory(bfm, mem, 256);
bfm->enable_backdoor = true;

axi4_trans_t* wr = axi4_trans_create();
axi4_trans_set_addr(wr, 0x100);
axi4_trans_set_burst(wr, 3, 3, AXI_BURST_INCR);
axi4_trans_set_data(wr, 0, 0xDEADBEEF, ~0ull);
axi4_bfm_write(bfm, wr);

axi4_trans_t* rd = axi4_trans_create();
axi4_trans_set_addr(rd, 0x100);
axi4_trans_set_burst(rd, 3, 3, AXI_BURST_INCR);
axi4_bfm_read(bfm, rd);
```

### DDR Memory Model

```c
ddr_mem_t* ddr = ddr_mem_create("ddr_sim", VIP_PROTO_DDR4, 1ULL << 30);
ddr_mem_initialize(ddr, true, 0xCAFE);

ddr_mem_issue_command(ddr, DDR_CMD_ACT, 0, 0, 0, NULL);
ddr_mem_issue_command(ddr, DDR_CMD_WRITE, 0, 0, 0, &write_data);
ddr_mem_issue_command(ddr, DDR_CMD_READ, 0, 0, 0, &read_data);
ddr_mem_issue_command(ddr, DDR_CMD_PRE, 0, 0, 0, NULL);
```

### VIP Scoreboard

```c
vip_sb_t* sb = vip_sb_create("axi_sb", 256);
vip_sb_set_predict(sb, my_predict_fn, &my_model);
vip_sb_set_compare(sb, my_compare_fn, NULL);

vip_sb_push_expected(sb, &expected_data);
bool match = vip_sb_check_actual(sb, &actual_data);
vip_sb_report(sb, stdout);
```

### Regression Testing

```c
regression_suite_t* suite = regression_suite_create("full_regression");
regression_suite_set_seed(suite, 0xFEEDBEEF);
suite->stop_on_fail = false;

regression_reg_t* t1 = regression_reg_create("axi_basic", test_fn, &ctx, 1001);
regression_reg_t* t2 = regression_reg_create("ddr_basic", test_fn2, &ctx, 1002);

regression_suite_add_test(suite, t1);
regression_suite_add_test(suite, t2);

regression_suite_run(suite);
regression_suite_report(suite, stdout);
```

### Random Seed Management

```c
seed_mgr_t* mgr = seed_mgr_create(0xDEADBEEF);

uint32_t s1 = seed_mgr_next(mgr);
uint32_t s2 = seed_mgr_next(mgr);
seed_mgr_save_state(mgr);  // save current state

seed_mgr_reset(mgr);        // back to master seed
uint32_t s1_again = seed_mgr_next(mgr);  // same as s1
```

### Protocol Analyzer

```c
proto_analyzer_t* pa = proto_analyzer_create("axi_analyzer", VIP_PROTO_AXI4);
proto_analyzer_log(pa, "TX", &txn);
proto_analyzer_record_latency(pa, latency_cycles);
proto_analyzer_report(pa, stdout);
```

## Compilation & Linking

```bash
# Basic build
gcc -std=c99 -Wall -Wextra -O2 -c uvm_methodology.c -o uvm_methodology.o
gcc -std=c99 -Wall -Wextra -O2 -c assertion_check.c -o assertion_check.o
# ... etc

# Build an example
gcc -std=c99 -Wall -Wextra -O2 \
    example_uvm_tb.c uvm_methodology.o assertion_check.o \
    formal_verify.o coverage_mdl.o verification_ip.o \
    -o example_uvm_tb -lm
```

Or simply: `make all`
