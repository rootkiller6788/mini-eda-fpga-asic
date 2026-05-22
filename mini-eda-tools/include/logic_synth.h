#ifndef LOGIC_SYNTH_H
#define LOGIC_SYNTH_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_MINTERMS    64
#define MAX_IMPLICANTS  128
#define MAX_LITERALS    32
#define MAX_GATES       256
#define MAX_NODES       256

typedef enum {
    GATE_AND, GATE_OR, GATE_NOT, GATE_NAND, GATE_NOR,
    GATE_XOR, GATE_XNOR, GATE_BUF, GATE_MUX
} GateType;

typedef struct {
    GateType  type;
    int       inputs[4];
    int       input_count;
    char      name[32];
    int       output_wire;
} LogicGate;

typedef struct {
    LogicGate gates[MAX_GATES];
    int       gate_count;
    int       wire_names[MAX_NODES];
    int       wire_count;
    char      name[64];
} LogicNetwork;

typedef struct {
    int  terms[MAX_IMPLICANTS][MAX_LITERALS];
    bool dc[MAX_IMPLICANTS][MAX_LITERALS];
    int  term_count;
    int  var_count;
} MintermTable;

void network_init(LogicNetwork *net, const char *name);
int  network_add_gate(LogicNetwork *net, GateType type, int *inputs, int input_count);
void network_optimize(LogicNetwork *net);
void network_constant_fold(LogicNetwork *net);
int  network_factor(LogicNetwork *net);
void network_print(LogicNetwork *net);
int  network_node_count(LogicNetwork *net);
int  network_gate_count(LogicNetwork *net);

void minterm_init(MintermTable *mt, int var_count);
void minterm_add(MintermTable *mt, int *values, int count);
void minterm_dont_care(MintermTable *mt, int *values, int count);
bool minterm_reduce(MintermTable *mt);
void minterm_print(MintermTable *mt);
int  minterm_essential_prime(MintermTable *mt, int *selected);

bool synth_quine_mccluskey(MintermTable *mt, MintermTable *result);

#endif
