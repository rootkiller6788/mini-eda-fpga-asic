#include "testbench.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void tb_init(Testbench *tb, const char *name) { memset(tb, 0, sizeof(*tb)); strncpy(tb->name, name, sizeof(tb->name)-1); tb->phase = TB_IDLE; }
int tb_add_signal(Testbench *tb, const char *name, int width) { if (tb->signal_count >= MAX_TB_SIGNALS) return -1; TbSignal *s = &tb->signals[tb->signal_count]; strncpy(s->name, name, sizeof(s->name)-1); s->width = width; s->val = 0; return tb->signal_count++; }
void tb_drive(Testbench *tb, const char *sig, int value) { for (int i = 0; i < tb->signal_count; i++) { if (strcmp(tb->signals[i].name, sig) == 0) { tb->signals[i].val = value; if (tb->verbose) printf("[TB:%s] drive %s = %d @ cycle %d\n", tb->name, sig, value, tb->cycle); return; } } }
int tb_monitor(Testbench *tb, const char *sig) { for (int i = 0; i < tb->signal_count; i++) { if (strcmp(tb->signals[i].name, sig) == 0) { int v = tb->signals[i].val; if (tb->verbose) printf("[TB:%s] monitor %s = %d @ cycle %d\n", tb->name, sig, v, tb->cycle); return v; } } return 0; }
bool tb_compare(Testbench *tb, const char *sig, int expected) { int actual = tb_monitor(tb, sig); if (actual != expected) { printf("[TB:%s] MISMATCH: %s expected %d, got %d @ cycle %d\n", tb->name, sig, expected, actual, tb->cycle); tb->errors++; return false; } return true; }
void tb_cycle(Testbench *tb) { tb->cycle++; if (tb->phase == TB_IDLE) tb->phase = TB_DRIVE; }
void tb_run(Testbench *tb, int cycles) { for (int i = 0; i < cycles; i++) tb_cycle(tb); tb->phase = TB_DONE; }
void tb_report(Testbench *tb) { printf("=== Testbench '%s' Report ===\n  Cycles: %d, Errors: %d, Phase: %d\n", tb->name, tb->cycle, tb->errors, tb->phase); }
