#include "uvm_methodology.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
   Example: UVM Testbench
   Full testbench hierarchy: test → env → agent → driver/monitor/scoreboard
   example_uvm_tb.c
   ================================================================ */

/* ----- DUT register model (simplified) ----- */
#define DUT_REG_COUNT 8
static uint32_t g_dut_regs[DUT_REG_COUNT] = {0};
static uint64_t g_sim_cycle = 0;

typedef struct {
    uint32_t addr;
    uint32_t data;
    bool     is_write;
} bus_transaction_t;

/* ----- Driver: drives bus transactions to DUT ----- */
void my_driver_run(uvm_driver_t* drv) {
    printf("  [%s] Driver running at cycle %llu\n", drv->name, (unsigned long long)g_sim_cycle);

    uvm_sequence_item_t* item = uvm_sequencer_pop_item(drv->sequencer);
    if (!item) return;

    bus_transaction_t* bt = (bus_transaction_t*)item->data;
    drv->item_count++;
    g_sim_cycle++;

    if (bt->is_write) {
        if (bt->addr < DUT_REG_COUNT) {
            g_dut_regs[bt->addr] = bt->data;
        }
        printf("  [%s] DUT WRITE: reg[%u] = 0x%08X\n", drv->name, bt->addr, bt->data);
    } else {
        bt->data = (bt->addr < DUT_REG_COUNT) ? g_dut_regs[bt->addr] : 0;
        printf("  [%s] DUT READ:  reg[%u] = 0x%08X\n", drv->name, bt->addr, bt->data);
    }
}

/* ----- Monitor: observes bus activity, sends to scoreboard ----- */
void my_monitor_sample(uvm_monitor_t* mon, const uvm_sequence_item_t* item) {
    bus_transaction_t* bt = (bus_transaction_t*)item->data;
    printf("  [%s] Monitored: %s addr=%u data=0x%08X\n",
        mon->name, bt->is_write ? "WR" : "RD", bt->addr, bt->data);
}

/* ----- Scoreboard compare function: predicted vs actual ----- */
bool my_sb_compare(uvm_scoreboard_t* sb,
    const uvm_sequence_item_t* predicted,
    const uvm_sequence_item_t* actual)
{
    (void)sb;
    bus_transaction_t* p = (bus_transaction_t*)predicted->data;
    bus_transaction_t* a = (bus_transaction_t*)actual->data;
    bool match = (p->addr == a->addr) && (p->data == a->data) && (p->is_write == a->is_write);
    if (!match) {
        printf("  [Scoreboard] MISMATCH: expected addr=%u data=0x%08X, got addr=%u data=0x%08X\n",
            p->addr, p->data, a->addr, a->data);
    }
    return match;
}

/* ----- Test run phase ----- */
void my_test_run(uvm_test_t* test) {
    printf("\n[%s] === Test run phase ===\n", test->name);

    uvm_env_t* env = test->env;
    if (!env) return;

    uvm_agent_t* agent = env->agents[0];
    uvm_sequencer_t* sqr = agent->sequencer;

    uvm_phase_raise_objection(uvm_phase_ctrl_get());

    /* Generate 5 write transactions */
    for (int i = 0; i < 5; i++) {
        uvm_sequence_item_t* item = uvm_seq_item_create("bus_trans", sizeof(bus_transaction_t));
        bus_transaction_t* bt = calloc(1, sizeof(bus_transaction_t));
        bt->addr    = (uint32_t)(i % DUT_REG_COUNT);
        bt->data    = (uint32_t)(0xDEAD0000u | (uint32_t)i);
        bt->is_write = (i < 3);
        uvm_seq_item_set_data(item, bt, sizeof(bus_transaction_t));
        uvm_sequencer_push_item(sqr, item);

        /* Add expected to scoreboard */
        if (bt->is_write) {
            bus_transaction_t* exp = calloc(1, sizeof(bus_transaction_t));
            memcpy(exp, bt, sizeof(bus_transaction_t));
            uvm_sequence_item_t* exp_item = uvm_seq_item_create("bus_trans_expected", sizeof(bus_transaction_t));
            uvm_seq_item_set_data(exp_item, exp, sizeof(bus_transaction_t));
            uvm_scoreboard_add_expected(env->scoreboard, exp_item);
            free(exp);
        }

        /* Run driver to consume the item */
        uvm_driver_run(agent->driver);

        /* Monitor the item */
        uvm_monitor_sample(agent->monitor, item);

        /* Scoreboard check */
        if (!bt->is_write) {
            uvm_scoreboard_check_actual(env->scoreboard, item);
        }

        free(bt);
    }

    uvm_phase_drop_objection(uvm_phase_ctrl_get());

    if (env->scoreboard->mismatch_count > 0) {
        test->pass = false;
        test->fail_count = (int)env->scoreboard->mismatch_count;
    }
}

/* ----- Main: build and run the UVM testbench ----- */
int main(void) {
    printf("========================================\n");
    printf(" UVM Testbench Example\n");
    printf("========================================\n\n");

    /* Phase 1: Build */
    printf("[PHASE] build\n");

    uvm_test_t* test = uvm_test_create("reg_access_test");
    test->seed = 0xCAFE1234;
    uvm_test_set_run_handler(test, my_test_run);

    uvm_env_t* env = uvm_env_create("reg_env");

    uvm_agent_t* agent = uvm_agent_create("bus_agent", UVM_AGENT_ACTIVE);
    agent->sequencer = uvm_sequencer_create("bus_sequencer");
    agent->driver    = uvm_driver_create("bus_driver", agent->sequencer);
    agent->driver->run_phase = my_driver_run;
    agent->monitor   = uvm_monitor_create("bus_monitor");
    agent->monitor->sample = my_monitor_sample;
    uvm_agent_connect(agent);

    uvm_env_add_agent(env, agent);

    env->scoreboard = uvm_scoreboard_create("reg_sb", my_sb_compare);
    uvm_env_set_scoreboard(env, env->scoreboard);

    uvm_test_set_env(test, env);

    /* Phase 2: Connect */
    printf("[PHASE] connect\n");
    tlm_port_t* drv_port = tlm_port_create("drv_tlm", TLM_PORT_PUT);
    tlm_port_t* mon_port = tlm_port_create("mon_tlm", TLM_PORT_ANALYSIS);
    tlm_port_connect(drv_port, mon_port);

    /* Phase 3: End of elaboration */
    printf("[PHASE] end_of_elaboration\n");

    /* Phase 4: Run */
    printf("[PHASE] run\n");
    bool passed = uvm_test_run(test);

    /* Phase 5: Report */
    printf("[PHASE] report\n");
    uvm_scoreboard_report(env->scoreboard);
    uvm_test_report(test);

    /* Phase 6: Final */
    printf("[PHASE] final\n");

    tlm_port_destroy(drv_port);
    tlm_port_destroy(mon_port);
    uvm_test_destroy(test);

    printf("\n=== Result: %s ===\n", passed ? "PASSED" : "FAILED");
    return passed ? 0 : 1;
}
