#ifndef FORMAL_CHECK_H
#define FORMAL_CHECK_H
#include <stdbool.h>
#define MAX_FC_STATES 64
#define MAX_FC_VARS 16
typedef struct { int vars[MAX_FC_VARS]; int var_count; } FmState;
typedef struct { FmState states[MAX_FC_STATES]; int state_count; } FmStateSpace;
typedef bool (*FmProperty)(FmState *s);
typedef FmState (*FmTransition)(FmState *s);
void fm_state_init(FmState *s, int var_count);
void fm_state_set(FmState *s, int var, int val);
int fm_state_get(FmState *s, int var);
bool formal_bmc(FmState *init, FmTransition next, FmProperty prop, int bound);
bool formal_induction(FmState *init, FmTransition next, FmProperty prop, int max_depth);
bool formal_equivalence(FmState *a, FmState *b, FmTransition ta, FmTransition tb, int bound);
void formal_print_result(bool passed, const char *name);
#endif
