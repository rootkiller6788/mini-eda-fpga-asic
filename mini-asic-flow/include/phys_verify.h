#ifndef PHYS_VERIFY_H
#define PHYS_VERIFY_H

#include <stdbool.h>

#define MAX_DRC_RULES 32
#define MAX_DRC_ERRORS 256

typedef enum { DRC_WIDTH, DRC_SPACING, DRC_ENCLOSURE, DRC_AREA, DRC_ANTENNA, DRC_DENSITY } DrcRuleType;

typedef struct {
    DrcRuleType type;
    char        layer[16];
    double      min_value;
    double      max_value;
    char        desc[64];
} DrcRule;

typedef struct {
    int      id;
    char     rule_name[32];
    double   x, y;
    double   value;
    double   expected;
    char     desc[128];
} DrcError;

typedef struct {
    DrcRule    rules[MAX_DRC_RULES];
    int        rule_count;
    DrcError   errors[MAX_DRC_ERRORS];
    int        error_count;
} DrcChecker;

typedef struct {
    char      layer[16];
    double    width;
    double    spacing;
    double    area;
    double    density_min;
    double    density_max;
} DrcSpec;

void drc_init(DrcChecker *drc);
int  drc_add_rule(DrcChecker *drc, DrcRuleType type, const char *layer, double min, double max);
bool drc_check_width(DrcChecker *drc, const char *layer, double width, double x, double y);
bool drc_check_spacing(DrcChecker *drc, const char *layer, double spacing, double x, double y);
bool drc_check_area(DrcChecker *drc, const char *layer, double area, double x, double y);
int  drc_run_all(DrcChecker *drc);
void drc_report(DrcChecker *drc);
int  drc_error_count(DrcChecker *drc);
bool drc_clean(DrcChecker *drc);

bool lvs_compare(const char *schematic_netlist, const char *layout_netlist);

#endif
