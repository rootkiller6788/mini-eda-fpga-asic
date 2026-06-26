#include "synthesis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int next_id = 0;

bool syn_create_netlist(const char *module, const char *name, netlist_t *nl) {
    if (!nl) return false;
    memset(nl, 0, sizeof(*nl));
    strncpy(nl->module, module, sizeof(nl->module) - 1);
    strncpy(nl->name, name, sizeof(nl->name) - 1);
    nl->gates = NULL;
    nl->num_gates = 0;
    nl->num_pi = 0;
    nl->num_po = 0;
    nl->num_seq = 0;
    next_id = 0;
    return true;
}

bool syn_add_gate(netlist_t *nl, gate_type_e type, const char *name) {
    if (!nl) return false;
    nl->num_gates++;
    nl->gates = (gate_node_t*)realloc(nl->gates,
                    nl->num_gates * sizeof(gate_node_t));
    if (!nl->gates) return false;
    gate_node_t *g = &nl->gates[nl->num_gates - 1];
    memset(g, 0, sizeof(*g));
    g->id = next_id++;
    strncpy(g->name, name ? name : "", sizeof(g->name) - 1);
    g->type = type;
    g->inputs = NULL;
    g->outputs = NULL;
    g->num_inputs = 0;
    g->num_outputs = 0;
    g->mapped_cell = -1;
    return true;
}

bool syn_connect(netlist_t *nl, int src_gate, int src_port,
                 int dst_gate, int dst_port) {
    if (!nl || src_gate >= nl->num_gates || dst_gate >= nl->num_gates)
        return false;
    gate_node_t *sg = &nl->gates[src_gate];
    gate_node_t *dg = &nl->gates[dst_gate];

    sg->num_outputs = (src_port >= sg->num_outputs) ?
                       src_port + 1 : sg->num_outputs;
    sg->outputs = (int*)realloc(sg->outputs, sg->num_outputs * sizeof(int));

    dg->num_inputs = (dst_port >= dg->num_inputs) ?
                      dst_port + 1 : dg->num_inputs;
    dg->inputs = (int*)realloc(dg->inputs, dg->num_inputs * sizeof(int));

    sg->outputs[src_port] = dst_gate;
    dg->inputs[dst_port] = src_gate;
    return true;
}

bool syn_set_pi(netlist_t *nl, int gate_id) {
    if (!nl || gate_id >= nl->num_gates) return false;
    nl->gates[gate_id].is_pi = true;
    nl->num_pi++;
    return true;
}

bool syn_set_po(netlist_t *nl, int gate_id) {
    if (!nl || gate_id >= nl->num_gates) return false;
    nl->gates[gate_id].is_po = true;
    nl->num_po++;
    return true;
}

bool syn_set_clock(netlist_t *nl, int gate_id) {
    if (!nl || gate_id >= nl->num_gates) return false;
    nl->gates[gate_id].is_clock = true;
    nl->gates[gate_id].is_seq = true;
    nl->num_seq++;
    return true;
}

void syn_free_netlist(netlist_t *nl) {
    if (!nl) return;
    for (int i = 0; i < nl->num_gates; i++) {
        free(nl->gates[i].inputs);
        free(nl->gates[i].outputs);
    }
    free(nl->gates);
    memset(nl, 0, sizeof(*nl));
}

bool syn_load_tech_lib(const char *filename, tech_lib_t *lib) {
    if (!lib) return false;
    memset(lib, 0, sizeof(*lib));
    strncpy(lib->name, filename, sizeof(lib->name) - 1);
    lib->vendor = TECH_CUSTOM;
    lib->vdd_v = 1.0;
    lib->temp_c = 25.0;
    lib->process_nm = 28.0;
    lib->num_cells = 18;
    lib->cells = (std_cell_t*)calloc(lib->num_cells, sizeof(std_cell_t));

    static const struct { const char *n; gate_type_e t; uint32_t in;
        double a, dr, df, le; } tmpl[] = {
        {"INV_X1",  GATE_INV,  1, 0.6, 10.0, 12.0, 1.2},
        {"INV_X2",  GATE_INV,  1, 1.0,  6.0,  8.0, 1.8},
        {"INV_X4",  GATE_INV,  1, 1.8,  4.0,  5.0, 2.5},
        {"BUF_X1",  GATE_BUF,  1, 0.8, 15.0, 16.0, 1.4},
        {"NAND2_X1",GATE_NAND, 2, 0.9, 14.0, 16.0, 1.6},
        {"NAND2_X2",GATE_NAND, 2, 1.4, 10.0, 12.0, 2.2},
        {"NOR2_X1", GATE_NOR,  2, 1.0, 16.0, 18.0, 1.8},
        {"AND2_X1", GATE_AND,  2, 1.2, 18.0, 20.0, 2.0},
        {"OR2_X1",  GATE_OR,   2, 1.2, 18.0, 20.0, 2.0},
        {"XOR2_X1", GATE_XOR,  2, 2.0, 25.0, 28.0, 3.0},
        {"MUX2_X1", GATE_MUX,  3, 2.2, 22.0, 24.0, 3.2},
        {"DFF_X1",  GATE_DFF,  1, 3.5, 35.0, 38.0, 5.0},
        {"DFF_X2",  GATE_DFF,  1, 5.0, 28.0, 30.0, 7.0},
        {"DLAT_X1", GATE_DLAT, 1, 2.8, 30.0, 32.0, 4.5},
        {"AOI21_X1",GATE_AOI21,3, 1.5, 16.0, 18.0, 2.5},
        {"CLKGATE_X1",GATE_CLKGATE,2,1.8,18.0,20.0,2.8},
        {"FADD_X1", GATE_FADD, 3, 3.0, 30.0, 32.0, 4.0},
        {"TIEHI_X1",GATE_TIEHI, 0, 0.2, 0.0, 0.0, 0.1},
    };

    for (int i = 0; i < lib->num_cells; i++) {
        std_cell_t *c = &lib->cells[i];
        strncpy(c->name, tmpl[i].n, sizeof(c->name) - 1);
        c->type = tmpl[i].t;
        c->inputs = tmpl[i].in;
        c->area_um2 = tmpl[i].a;
        c->delay_rising_ps = tmpl[i].dr;
        c->delay_falling_ps = tmpl[i].df;
        c->leakage_pw = tmpl[i].le;
        c->drive_strength = 1.0;
        c->cap_input_ff = 0.5;
        c->cap_output_ff = 1.0;
        c->is_sequential =
            (tmpl[i].t == GATE_DFF || tmpl[i].t == GATE_DLAT);
        c->has_scan = c->is_sequential;
        c->vt_level = (i % 3 == 0) ? VT_LOW :
                       (i % 3 == 1) ? VT_STD : VT_HIGH;
    }
    return true;
}

std_cell_t *syn_find_cell(tech_lib_t *lib, gate_type_e type,
                          double area_limit) {
    if (!lib) return NULL;
    std_cell_t *best = NULL;
    double best_area = 1e12;
    for (int i = 0; i < lib->num_cells; i++) {
        if (lib->cells[i].type == type &&
            lib->cells[i].area_um2 <= area_limit &&
            lib->cells[i].area_um2 < best_area) {
            best = &lib->cells[i];
            best_area = lib->cells[i].area_um2;
        }
    }
    return best;
}

bool syn_run_boolean_optimize(netlist_t *nl) {
    bool changed = true;
    int iter = 0;
    while (changed && iter < 10) {
        changed = false;
        for (int i = 0; i < nl->num_gates; i++) {
            gate_node_t *g = &nl->gates[i];
            if (g->type == GATE_BUF) {
                for (int j = 0; j < nl->num_gates; j++) {
                    if (j == i) continue;
                    gate_node_t *h = &nl->gates[j];
                    for (int k = 0; k < h->num_inputs; k++) {
                        if (h->inputs[k] == i && g->num_inputs > 0) {
                            h->inputs[k] = g->inputs[0];
                            changed = true;
                        }
                    }
                }
            }
            if (g->type == GATE_INV && g->num_inputs > 0) {
                int drv = g->inputs[0];
                if (drv >= 0 && drv < nl->num_gates &&
                    nl->gates[drv].type == GATE_INV &&
                    nl->gates[drv].num_inputs > 0) {
                    int src = nl->gates[drv].inputs[0];
                    for (int j = 0; j < nl->num_gates; j++) {
                        for (int k = 0; k < nl->gates[j].num_inputs; k++) {
                            if (nl->gates[j].inputs[k] == i)
                                nl->gates[j].inputs[k] = src;
                        }
                    }
                    changed = true;
                }
            }
        }
        iter++;
    }
    return true;
}

bool syn_run_tech_map(netlist_t *nl, tech_lib_t *lib, synth_params_t *p) {
    if (!nl || !lib) return false;
    for (int i = 0; i < nl->num_gates; i++) {
        gate_node_t *g = &nl->gates[i];
        if (g->is_pi || g->is_po) continue;
        double area_lim = p ? p->max_area_um2 / (nl->num_gates + 1) : 1000.0;
        std_cell_t *cell = syn_find_cell(lib, g->type, area_lim);
        g->mapped_cell = cell ? (int)(cell - lib->cells) : 0;
    }
    return true;
}

bool syn_run_fanout_repair(netlist_t *nl, int max_fanout) {
    int added = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].num_outputs > max_fanout) {
            int num_bufs = (nl->gates[i].num_outputs + max_fanout - 1) /
                           max_fanout;
            for (int b = 0; b < num_bufs; b++) {
                char bname[64];
                snprintf(bname, sizeof(bname), "repair_buf_%d_%d", i, b);
                syn_add_gate(nl, GATE_BUF, bname);
            }
            added++;
        }
    }
    return true;
}

bool syn_run_clock_gating(netlist_t *nl) {
    int cg = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].type == GATE_DFF) {
            char cname[64];
            snprintf(cname, sizeof(cname), "icg_%d", i);
            syn_add_gate(nl, GATE_CLKGATE, cname);
            cg++;
        }
    }
    return true;
}

bool syn_run_rtl_elaborate(netlist_t *nl, synth_params_t *p) {
    (void)p;
    syn_run_boolean_optimize(nl);
    return true;
}

bool syn_run_retiming(netlist_t *nl, double target_period_ps) {
    int moves = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].type == GATE_DFF) {
            for (int j = 0; j < nl->gates[i].num_inputs; j++) {
                int prev = nl->gates[i].inputs[j];
                if (prev >= 0 && prev < nl->num_gates &&
                    nl->gates[prev].type != GATE_DFF) {
                    moves++;
                }
            }
        }
    }
    (void)target_period_ps;
    return true;
}

bool syn_decompose_aig(netlist_t *nl) {
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].num_inputs > 2) {
            char dname[64];
            snprintf(dname, sizeof(dname), "and2_%d", i);
            syn_add_gate(nl, GATE_AND, dname);
        }
    }
    return true;
}

bool syn_area_recovery(netlist_t *nl, tech_lib_t *lib) {
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].type == GATE_INV && i + 1 < nl->num_gates) {
            std_cell_t *c = syn_find_cell(lib, GATE_INV, 1.0);
            if (c) nl->gates[i].mapped_cell = (int)(c - lib->cells);
        }
    }
    return true;
}

bool syn_constant_propagation(netlist_t *nl) {
    for (int i = 0; i < nl->num_gates; i++) {
        if (nl->gates[i].type == GATE_TIEHI ||
            nl->gates[i].type == GATE_TIELO) {
            (void)nl; (void)i;
        }
    }
    return true;
}

bool syn_dead_code_elimination(netlist_t *nl) {
    int *used = (int*)calloc(nl->num_gates, sizeof(int));
    for (int i = 0; i < nl->num_gates; i++) {
        for (int j = 0; j < nl->gates[i].num_inputs; j++) {
            int in = nl->gates[i].inputs[j];
            if (in >= 0 && in < nl->num_gates) used[in] = 1;
        }
    }
    free(used);
    return true;
}

bool syn_synthesize(netlist_t *nl, tech_lib_t *lib,
                    synth_params_t *p, synth_result_t *r) {
    if (!nl || !lib || !r) return false;
    memset(r, 0, sizeof(*r));
    syn_run_rtl_elaborate(nl, p);
    syn_run_boolean_optimize(nl);
    syn_decompose_aig(nl);
    syn_run_tech_map(nl, lib, p);
    if (p && p->max_fanout > 0)
        syn_run_fanout_repair(nl, p->max_fanout);
    if (p && p->allow_clock_gating)
        syn_run_clock_gating(nl);
    syn_area_recovery(nl, lib);
    syn_constant_propagation(nl);
    syn_dead_code_elimination(nl);

    r->area_um2 = syn_estimate_area(nl, lib);
    r->leakage_uw = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        int mc = nl->gates[i].mapped_cell;
        if (mc >= 0 && mc < lib->num_cells)
            r->leakage_uw += lib->cells[mc].leakage_pw;
    }
    r->leakage_uw *= 1e-6;
    r->total_cells = syn_count_cells(nl);
    r->total_nets = 0;
    for (int i = 0; i < nl->num_gates; i++)
        r->total_nets += nl->gates[i].num_inputs;
    r->netlist = nl;
    r->tech_lib = lib;
    return true;
}

double syn_estimate_area(const netlist_t *nl, const tech_lib_t *lib) {
    double area = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        int mc = nl->gates[i].mapped_cell;
        if (mc >= 0 && mc < lib->num_cells)
            area += lib->cells[mc].area_um2;
    }
    return area;
}

double syn_estimate_delay(const netlist_t *nl, const tech_lib_t *lib) {
    double max_d = 0;
    for (int i = 0; i < nl->num_gates; i++) {
        int mc = nl->gates[i].mapped_cell;
        if (mc >= 0 && mc < lib->num_cells) {
            double d = lib->cells[mc].delay_rising_ps +
                       lib->cells[mc].delay_falling_ps;
            if (d > max_d) max_d = d;
        }
    }
    return max_d * nl->num_gates / 4.0;
}

int syn_count_cells(const netlist_t *nl) {
    int c = 0;
    for (int i = 0; i < nl->num_gates; i++)
        if (!nl->gates[i].is_pi && !nl->gates[i].is_po) c++;
    return c;
}

void syn_print_netlist(const netlist_t *nl) {
    if (!nl) return;
    printf("Netlist: %s (module: %s)\n", nl->name, nl->module);
    printf("  gates=%d  PI=%d  PO=%d  Seq=%d\n",
           nl->num_gates, nl->num_pi, nl->num_po, nl->num_seq);
    for (int i = 0; i < nl->num_gates; i++) {
        gate_node_t *g = &nl->gates[i];
        printf("  [%d] %s type=%d in=%d out=%d mapped=%d\n",
               g->id, g->name, (int)g->type,
               g->num_inputs, g->num_outputs, g->mapped_cell);
    }
}

void syn_print_stats(const synth_result_t *r) {
    if (!r) return;
    printf("Synthesis Result:\n");
    printf("  Area:     %.2f um2\n", r->area_um2);
    printf("  Leakage:  %.4f uW\n", r->leakage_uw);
    printf("  Cells:    %d\n", r->total_cells);
    printf("  Nets:     %d\n", r->total_nets);
    printf("  Runtime:  %.2f ms\n", r->runtime_ms);
}

void syn_write_verilog(const netlist_t *nl, const char *filename) {
    if (!nl || !filename) return;
    FILE *f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "module %s(", nl->module);
    fprintf(f, ");\n");
    for (int i = 0; i < nl->num_gates; i++) {
        gate_node_t *g = &nl->gates[i];
        static const char *gnames[] = {
            "AND","NAND","OR","NOR","XOR","XNOR","INV","BUF",
            "MUX","DFF","DLAT","AOI21","OAI21","FADD","CLKGATE",
            "TIEHI","TIELO","NONE"
        };
        fprintf(f, "  %s %s (", gnames[g->type], g->name);
        fprintf(f, ");\n");
    }
    fprintf(f, "endmodule\n");
    fclose(f);
}
