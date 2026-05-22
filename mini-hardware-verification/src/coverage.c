#include "coverage.h"
#include <stdio.h>
#include <string.h>
void cov_init(Coverage *c) { memset(c, 0, sizeof(*c)); }
int cov_new_group(Coverage *c, const char *name, CovType type) { if (c->group_count >= MAX_COV_GROUPS) return -1; CovGroup *g = &c->groups[c->group_count]; strncpy(g->name, name, sizeof(g->name)-1); g->type = type; g->bin_count = 0; return c->group_count++; }
void cov_add_bin(Coverage *c, int group_id, int bin_id) { if (group_id < 0 || group_id >= c->group_count || c->groups[group_id].bin_count >= MAX_COV_BINS) return; c->groups[group_id].bins[c->groups[group_id].bin_count++] = bin_id; c->groups[group_id].bstats.total++; }
void cov_sample(Coverage *c, int group_id, int bin_id) { if (group_id < 0 || group_id >= c->group_count) return; CovGroup *g = &c->groups[group_id]; for (int i = 0; i < g->bin_count; i++) { if (g->bins[i] == bin_id) { g->bstats.hit++; c->total_samples++; return; } } }
void cov_merge(Coverage *c, Coverage *other) { for (int i = 0; i < other->group_count; i++) c->total_samples += other->groups[i].bstats.hit; }
void cov_report(Coverage *c) { printf("=== Coverage Report ===\n"); double total = 0; int n = 0; for (int i = 0; i < c->group_count; i++) { CovGroup *g = &c->groups[i]; double cv = g->bstats.total > 0 ? 100.0 * g->bstats.hit / g->bstats.total : 0; printf("  %s: %.1f%% (%d/%d)\n", g->name, cv, g->bstats.hit, g->bstats.total); total += cv; n++; } c->overall_coverage = n > 0 ? total / n : 0; printf("Overall: %.1f%%\n", c->overall_coverage); }
double cov_get_coverage(Coverage *c, int group_id) { if (group_id < 0 || group_id >= c->group_count) return 0; CovGroup *g = &c->groups[group_id]; return g->bstats.total > 0 ? 100.0 * g->bstats.hit / g->bstats.total : 0; }
bool cov_goal_met(Coverage *c, double target) { cov_report(c); return c->overall_coverage >= target; }
