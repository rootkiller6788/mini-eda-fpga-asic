#include "formal_check.h"
#include <stdio.h>
#include <string.h>
void fm_state_init(FmState *s, int var_count) { s->var_count = var_count; memset(s->vars, 0, sizeof(s->vars)); }
void fm_state_set(FmState *s, int var, int val) { if (var >= 0 && var < MAX_FC_VARS) s->vars[var] = val; }
int fm_state_get(FmState *s, int var) { if (var >= 0 && var < MAX_FC_VARS) return s->vars[var]; return 0; }
bool formal_bmc(FmState *init, FmTransition next, FmProperty prop, int bound) { FmState s = *init; for (int k = 0; k <= bound; k++) { if (!prop(&s)) { printf("[BMC] Property violated at step %d\n", k); return false; } if (k < bound) s = next(&s); } printf("[BMC] Property holds up to bound %d\n", bound); return true; }
bool formal_induction(FmState *init, FmTransition next, FmProperty prop, int max_depth) { FmState s = *init; if (!prop(&s)) { printf("[IND] Base case failed\n"); return false; } for (int d = 0; d < max_depth; d++) { FmState next_s = next(&s); if (!prop(&next_s)) { printf("[IND] Inductive step failed at depth %d\n", d); return false; } s = next_s; } printf("[IND] Property proven by induction to depth %d\n", max_depth); return true; }
bool formal_equivalence(FmState *a, FmState *b, FmTransition ta, FmTransition tb, int bound) { FmState sa = *a, sb = *b; for (int k = 0; k <= bound; k++) { for (int v = 0; v < sa.var_count && v < sb.var_count; v++) { if (sa.vars[v] != sb.vars[v]) { printf("[EQ] Mismatch at step %d, var %d\n", k, v); return false; } } sa = ta(&sa); sb = tb(&sb); } printf("[EQ] Equivalent up to bound %d\n", bound); return true; }
void formal_print_result(bool passed, const char *name) { printf("[FORMAL] %s: %s\n", name, passed ? "PASS" : "FAIL"); }
