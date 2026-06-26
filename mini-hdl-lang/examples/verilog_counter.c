#include "verilog_sim.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    VerilogSimulator sim;
    vs_init(&sim);

    int mod_idx = vs_add_module(&sim, "counter");
    VerilogModule *mod = vs_get_module(&sim, mod_idx);

    vs_add_port(mod, "clk", VS_PORT_INPUT, 1);
    vs_add_port(mod, "rst_n", VS_PORT_INPUT, 1);
    vs_add_port(mod, "count", VS_PORT_OUTPUT, 4);

    int clk_net = vs_add_net(mod, "clk", VS_NET_WIRE, 1);
    int rst_net = vs_add_net(mod, "rst_n", VS_NET_REG, 1);
    int count_net = vs_add_net(mod, "count", VS_NET_REG, 4);
    int next_count = vs_add_net(mod, "next_count", VS_NET_WIRE, 4);

    mod->ports[0].net_index = clk_net;
    mod->ports[1].net_index = rst_net;
    mod->ports[2].net_index = count_net;

    int always_idx = vs_add_always(mod);
    VerilogAlwaysBlock *blk = &mod->always_blocks[always_idx];
    vs_add_sensitivity(blk, VS_SENS_POSEDGE, clk_net);

    int stmt0 = vs_add_stmt(blk, VS_STMT_IF);
    VerilogValue zero_val = VS_VAL_0;
    blk->stmts[stmt0].cond_net = rst_net;
    int stmt1 = vs_add_stmt(blk, VS_STMT_BLOCKING_ASSIGN);
    vs_add_blocking_assign(blk, stmt1, count_net, &zero_val, 1);
    blk->stmts[stmt0].true_next = stmt1;
    blk->stmts[stmt0].false_next = -1;

    vs_vcd_open(&sim, "counter_wave.vcd");

    vs_set_net_value(mod, clk_net, VS_VAL_0);
    vs_set_net_value(mod, rst_net, VS_VAL_1);

    VerilogValue one = VS_VAL_1;

    for (int t = 0; t < 200; t++) {
        VerilogValue clk_val = (t % 10 < 5) ? VS_VAL_0 : VS_VAL_1;
        VerilogValue old_clk = vs_get_net_value(mod, clk_net);

        vs_set_net_value(mod, clk_net, clk_val);

        if (t > 5) vs_set_net_value(mod, rst_net, VS_VAL_0);

        if (old_clk == VS_VAL_0 && clk_val == VS_VAL_1) {
            vs_schedule_event(&sim, t, VS_EVT_POSEDGE, clk_net, mod_idx);
        }

        vs_vcd_print_time(&sim, t);
        vs_vcd_dump_signals(&sim);
    }

    vs_run(&sim, 200);
    vs_display_signals(mod, sim.current_time);
    vs_vcd_close(&sim);

    printf("Simulation complete. Waveform saved to counter_wave.vcd\n");

    vs_free(&sim);
    return 0;
}
