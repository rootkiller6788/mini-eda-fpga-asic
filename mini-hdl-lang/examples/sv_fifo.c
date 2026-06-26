#include "systemverilog_sim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    SvSimulator sim;
    sv_init(&sim);

    int mod_idx = sv_add_module(&sim, "fifo");
    SvModule *mod = sv_get_module(&sim, mod_idx);

    sv_add_port(mod, "clk", SV_PORT_INPUT, 1);
    sv_add_port(mod, "rst_n", SV_PORT_INPUT, 1);
    sv_add_port(mod, "wr_en", SV_PORT_INPUT, 1);
    sv_add_port(mod, "rd_en", SV_PORT_INPUT, 1);
    sv_add_port(mod, "data_in", SV_PORT_INPUT, 8);
    sv_add_port(mod, "data_out", SV_PORT_OUTPUT, 8);
    sv_add_port(mod, "full", SV_PORT_OUTPUT, 1);
    sv_add_port(mod, "empty", SV_PORT_OUTPUT, 1);

    int sig_clk = sv_add_signal(mod, "clk", 1, true);
    int sig_rst = sv_add_signal(mod, "rst_n", 1, true);
    int sig_wr = sv_add_signal(mod, "wr_en", 1, true);
    int sig_rd = sv_add_signal(mod, "rd_en", 1, true);
    int sig_din = sv_add_signal(mod, "data_in", 8, true);
    int sig_dout = sv_add_signal(mod, "data_out", 8, true);
    int sig_full = sv_add_signal(mod, "full", 1, true);
    int sig_empty = sv_add_signal(mod, "empty", 1, true);
    int sig_wr_ptr = sv_add_signal(mod, "wr_ptr", 3, false);
    int sig_rd_ptr = sv_add_signal(mod, "rd_ptr", 3, false);
    int sig_count = sv_add_signal(mod, "count", 4, false);

    sv_set_signal(mod, sig_empty, SV_LOGIC_1);
    sv_set_signal(mod, sig_full, SV_LOGIC_0);

    int enum_idx = sv_add_enum(mod, "fifo_state_e");
    SvEnum *e = &mod->enums[enum_idx];
    sv_add_enum_member(e, "IDLE", 0);
    sv_add_enum_member(e, "WRITE", 1);
    sv_add_enum_member(e, "READ", 2);
    sv_add_enum_member(e, "FULL", 3);
    sv_add_enum_member(e, "EMPTY", 4);

    int iface_idx = sv_add_interface(mod, "fifo_if");
    SvInterface *iface = &mod->interfaces[iface_idx];

    int always_ff = sv_add_always_ff(mod, "clk", true);
    SvAlwaysBlock *blk_ff = &mod->always_blocks[always_ff];
    SvLogicValue val1 = SV_LOGIC_1, val0 = SV_LOGIC_0;
    int stmt0 = sv_add_stmt(blk_ff, SV_STMT_IF);
    blk_ff->stmts[stmt0].cond_signal = sig_rst;
    int stmt_rst = sv_add_stmt(blk_ff, SV_STMT_ASSIGN);
    sv_add_assign(blk_ff, stmt_rst, sig_wr_ptr, &val0, 1);
    blk_ff->stmts[stmt0].true_next = stmt_rst;
    blk_ff->stmts[stmt0].false_next = -1;

    int always_comb = sv_add_always_comb(mod);
    SvAlwaysBlock *blk_comb = &mod->always_blocks[always_comb];

    int assert_idx = sv_add_assertion(mod, "wr_ptr < 8", SV_ASSERT_IMMEDIATE);
    sv_add_assertion(mod, "rd_ptr < 8", SV_ASSERT_IMMEDIATE);

    printf("=== SystemVerilog FIFO Simulation ===\n");
    printf("Module: %s\n", mod->name);
    printf("Enums: %d, Interfaces: %d\n", mod->enum_count, mod->interface_count);
    printf("Always blocks: %d (FF=%d, comb=%d)\n", mod->always_count, 1, 1);
    printf("Assertions: %d\n\n", mod->assertion_count);

    sv_set_signal(mod, sig_rst, SV_LOGIC_1);
    sv_set_signal(mod, sig_wr, SV_LOGIC_0);
    sv_set_signal(mod, sig_rd, SV_LOGIC_0);

    sv_run(&sim, 10);
    sv_set_signal(mod, sig_rst, SV_LOGIC_0);

    printf("Reset de-asserted. Writing data...\n");
    int test_data[] = {0xAA, 0x55, 0x0F, 0xF0};
    for (int i = 0; i < 4; i++) {
        sv_set_signal(mod, sig_din, (SvLogicValue)(test_data[i] & 0x3));
        sv_set_signal(mod, sig_wr, SV_LOGIC_1);
        sv_run(&sim, 2);
        sv_set_signal(mod, sig_wr, SV_LOGIC_0);
        sv_run(&sim, 2);
    }

    sv_display_module(mod);
    sv_free(&sim);

    printf("\nSimulation complete.\n");
    return 0;
}
