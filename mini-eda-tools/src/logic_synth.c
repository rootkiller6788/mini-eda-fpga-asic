#include "logic_synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void network_init(LogicNetwork *net, const char *name) {
    strncpy(net->name, name, sizeof(net->name) - 1);
    net->gate_count = 0;
    net->wire_count = 0;
    memset(net->wire_names, 0, sizeof(net->wire_names));
}

int network_add_gate(LogicNetwork *net, GateType type, int *inputs, int input_count) {
    if (net->gate_count >= MAX_GATES) return -1;
    LogicGate *g = &net->gates[net->gate_count];
    g->type = type;
    g->input_count = input_count;
    for (int i = 0; i < input_count && i < 4; i++) g->inputs[i] = inputs[i];
    snprintf(g->name, sizeof(g->name), "g%d", net->gate_count);
    g->output_wire = net->wire_count++;
    return net->gate_count++;
}

void network_optimize(LogicNetwork *net) {
    for (int i = 0; i < net->gate_count; i++) {
        LogicGate *g = &net->gates[i];
        if (g->type == GATE_NOT && g->input_count == 1) {
            for (int j = i + 1; j < net->gate_count; j++) {
                if (net->gates[j].type == GATE_NOT &&
                    net->gates[j].input_count == 1 &&
                    net->gates[j].inputs[0] == g->output_wire) {
                    net->gates[j].type = GATE_BUF;
                    net->gates[j].inputs[0] = g->inputs[0];
                }
            }
        }
        if (g->type == GATE_AND && g->input_count == 1) {
            g->type = GATE_BUF;
        }
        if (g->type == GATE_OR && g->input_count == 1) {
            g->type = GATE_BUF;
        }
    }
}

void network_constant_fold(LogicNetwork *net) {
    for (int i = 0; i < net->gate_count; i++) {
        LogicGate *g = &net->gates[i];
        if (g->type == GATE_AND) {
            bool has_zero = false;
            int real_inputs[4]; int ri = 0;
            for (int j = 0; j < g->input_count; j++) {
                if (g->inputs[j] == 0) { has_zero = true; break; }
                if (g->inputs[j] != 1) real_inputs[ri++] = g->inputs[j];
            }
            if (has_zero) {
                g->type = GATE_BUF; g->inputs[0] = 0; g->input_count = 1;
            } else if (ri == 0) {
                g->type = GATE_BUF; g->inputs[0] = 1; g->input_count = 1;
            }
        }
        if (g->type == GATE_OR) {
            bool has_one = false;
            int real_inputs[4]; int ri = 0;
            for (int j = 0; j < g->input_count; j++) {
                if (g->inputs[j] == 1) { has_one = true; break; }
                if (g->inputs[j] != 0) real_inputs[ri++] = g->inputs[j];
            }
            if (has_one) {
                g->type = GATE_BUF; g->inputs[0] = 1; g->input_count = 1;
            }
        }
    }
}

int network_factor(LogicNetwork *net) {
    int removed = 0;
    for (int i = 0; i < net->gate_count; i++) {
        if (net->gates[i].type == GATE_AND && net->gates[i].input_count >= 3) {
            int half = net->gates[i].input_count / 2;
            if (half >= 1) {
                network_add_gate(net, GATE_AND, net->gates[i].inputs, half);
                net->gates[i].inputs[0] = net->gates[net->gate_count - 1].output_wire;
                net->gates[i].inputs[1] = net->gates[i].inputs[half];
                net->gates[i].input_count = 2;
                removed++;
            }
        }
    }
    return removed;
}

void network_print(LogicNetwork *net) {
    printf("Network: %s (%d gates, %d wires)\n", net->name, net->gate_count, net->wire_count);
    for (int i = 0; i < net->gate_count; i++) {
        LogicGate *g = &net->gates[i];
        const char *tname[] = {"AND","OR","NOT","NAND","NOR","XOR","XNOR","BUF","MUX"};
        printf("  %s[%s]( ", g->name, tname[g->type]);
        for (int j = 0; j < g->input_count; j++) {
            printf("w%d ", g->inputs[j]);
        }
        printf(") -> w%d\n", g->output_wire);
    }
}

int network_node_count(LogicNetwork *net) { return net->wire_count; }
int network_gate_count(LogicNetwork *net) { return net->gate_count; }

void minterm_init(MintermTable *mt, int var_count) {
    mt->term_count = 0;
    mt->var_count = var_count;
    memset(mt->terms, -1, sizeof(mt->terms));
    memset(mt->dc, 0, sizeof(mt->dc));
}

void minterm_add(MintermTable *mt, int *values, int count) {
    if (mt->term_count >= MAX_IMPLICANTS) return;
    for (int i = 0; i < count && i < MAX_LITERALS; i++)
        mt->terms[mt->term_count][i] = values[i];
    for (int i = count; i < MAX_LITERALS; i++)
        mt->terms[mt->term_count][i] = -1;
    mt->term_count++;
}

void minterm_dont_care(MintermTable *mt, int *values, int count) {
    if (mt->term_count >= MAX_IMPLICANTS) return;
    for (int i = 0; i < count && i < MAX_LITERALS; i++)
        mt->terms[mt->term_count][i] = values[i];
    for (int i = 0; i < MAX_LITERALS; i++)
        mt->dc[mt->term_count][i] = true;
    mt->term_count++;
}

static int count_diff_bits(int t1[], int t2[], int n) {
    int diff = 0;
    for (int i = 0; i < n; i++) {
        if (t1[i] == -1 || t2[i] == -1) return -1;
        if (t1[i] != t2[i]) diff++;
    }
    return diff;
}

bool minterm_reduce(MintermTable *mt) {
    if (mt->term_count < 2) return false;
    bool used[MAX_IMPLICANTS] = {false};
    MintermTable next;
    minterm_init(&next, mt->var_count);
    int new_count = 0;

    for (int i = 0; i < mt->term_count; i++) {
        for (int j = i + 1; j < mt->term_count; j++) {
            if (count_diff_bits(mt->terms[i], mt->terms[j], mt->var_count) == 1) {
                used[i] = used[j] = true;
                for (int k = 0; k < mt->var_count; k++) {
                    next.terms[new_count][k] = mt->terms[i][k];
                    if (mt->terms[i][k] != mt->terms[j][k])
                        next.terms[new_count][k] = -1;
                }
                next.dc[new_count][0] = mt->dc[i][0] && mt->dc[j][0];
                new_count++;
                if (new_count >= MAX_IMPLICANTS) break;
            }
        }
    }

    for (int i = 0; i < mt->term_count; i++) {
        if (!used[i]) {
            for (int k = 0; k < mt->var_count; k++)
                next.terms[new_count][k] = mt->terms[i][k];
            next.dc[new_count][0] = mt->dc[i][0];
            new_count++;
        }
    }
    next.term_count = new_count;
    *mt = next;
    return true;
}

void minterm_print(MintermTable *mt) {
    printf("Minterm Table (%d terms, %d vars):\n", mt->term_count, mt->var_count);
    for (int i = 0; i < mt->term_count; i++) {
        printf("  ");
        for (int j = 0; j < mt->var_count; j++) {
            if (mt->terms[i][j] == -1) printf("-");
            else printf("%d", mt->terms[i][j]);
        }
        if (mt->dc[i][0]) printf(" (dc)");
        printf("\n");
    }
}

int minterm_essential_prime(MintermTable *mt, int *selected) {
    int sel_count = 0;
    bool covered[MAX_IMPLICANTS] = {false};

    for (int i = 0; i < mt->term_count; i++) {
        if (mt->dc[i][0]) continue;
        int covering = -1, count = 0;
        for (int j = 0; j < mt->term_count; j++) {
            bool covers = true;
            for (int k = 0; k < mt->var_count; k++) {
                if (mt->terms[j][k] != -1 && mt->terms[i][k] != -1 &&
                    mt->terms[j][k] != mt->terms[i][k]) {
                    covers = false; break;
                }
            }
            if (covers) { covering = j; count++; }
        }
        if (count == 1 && covering >= 0 && !covered[covering]) {
            selected[sel_count++] = covering;
            covered[covering] = true;
        }
    }
    return sel_count;
}

bool synth_quine_mccluskey(MintermTable *mt, MintermTable *result) {
    *result = *mt;
    int iterations = 0;
    while (result->term_count > 1 && iterations < 20) {
        if (!minterm_reduce(result)) break;
        iterations++;
    }
    return result->term_count > 0;
}
