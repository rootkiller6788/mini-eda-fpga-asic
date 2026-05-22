#ifndef COVERAGE_H
#define COVERAGE_H
#include <stdbool.h>
#define MAX_COV_BINS 128
#define MAX_COV_GROUPS 16
typedef enum { COV_CODE, COV_FUNCTIONAL, COV_FSM, COV_TOGGLE } CovType;
typedef struct { int total, hit; double ratio; } BinCount;
typedef struct { char name[64]; CovType type; int bins[MAX_COV_BINS]; int bin_count; BinCount bstats; bool auto_sample; } CovGroup;
typedef struct { CovGroup groups[MAX_COV_GROUPS]; int group_count; int total_samples; double overall_coverage; } Coverage;
void cov_init(Coverage *c);
int cov_new_group(Coverage *c, const char *name, CovType type);
void cov_add_bin(Coverage *c, int group_id, int bin_id);
void cov_sample(Coverage *c, int group_id, int bin_id);
void cov_merge(Coverage *c, Coverage *other);
void cov_report(Coverage *c);
double cov_get_coverage(Coverage *c, int group_id);
bool cov_goal_met(Coverage *c, double target);
#endif
