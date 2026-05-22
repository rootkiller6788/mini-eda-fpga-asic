#ifndef SYNTHESIS_H
#define SYNTHESIS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GATE_AND,   GATE_NAND,  GATE_OR,    GATE_NOR,
    GATE_XOR,   GATE_XNOR,  GATE_INV,   GATE_BUF,
    GATE_MUX,   GATE_DFF,   GATE_DLAT,  GATE_AOI21,
    GATE_OAI21, GATE_FADD,  GATE_CLKGATE, GATE_TIEHI,
    GATE_TIELO, GATE_NONE
} gate_type_e;

typedef enum {
    TECH_TSMC, TECH_SMIC, TECH_GF, TECH_CUSTOM
} tech_vendor_e;

typedef struct {
    char           name[64];
    gate_type_e    type;
    uint32_t       inputs;
    double         area_um2;
    double         drive_strength;
    double         leakage_pw;
    double         delay_rising_ps;
    double         delay_falling_ps;
    double         cap_input_ff;
    double         cap_output_ff;
    bool           is_sequential;
    bool           has_scan;
    int            vt_level;
} std_cell_t;

typedef enum { BOOL_SOP, BOOL_POS, BOOL_AIG, BOOL_MIG } bool_form_e;

typedef struct {
    int            id;
    char           name[64];
    gate_type_e    type;
    int            num_inputs;
    int            num_outputs;
    int           *inputs;
    int           *outputs;
    double         x, y;
    bool           is_pi;
    bool           is_po;
    bool           is_seq;
    bool           is_clock;
    int            mapped_cell;
} gate_node_t;

typedef struct {
    gate_node_t   *gates;
    int            num_gates;
    int            num_pi;
    int            num_po;
    int            num_seq;
    char           name[128];
    char           module[128];
} netlist_t;

typedef struct {
    std_cell_t    *cells;
    int            num_cells;
    char           name[64];
    tech_vendor_e  vendor;
    double         vdd_v;
    double         temp_c;
    double         process_nm;
} tech_lib_t;

typedef struct {
    double target_freq_mhz;
    double max_area_um2;
    double max_leakage_uw;
    bool   allow_multi_vt;
    bool   allow_clock_gating;
    bool   preserve_hierarchy;
    bool   optimize_timing;
    bool   optimize_area;
    bool   optimize_power;
    int    max_fanout;
    int    effort_level;
} synth_params_t;

typedef struct {
    netlist_t      *netlist;
    tech_lib_t     *tech_lib;
    synth_params_t  params;
    double          area_um2;
    double          leakage_uw;
    double          critical_path_ps;
    int             total_cells;
    int             total_nets;
    double          runtime_ms;
} synth_result_t;

typedef struct {
    int      id;
    char     name[64];
    bool     value;
    double   arrival_time_ps;
    double   slew_ps;
} primary_input_t;

typedef struct {
    int  gate_id;
    bool pin_a : 1;
    bool pin_b : 1;
    bool pin_c : 1;
} decomposition_t;

bool        syn_create_netlist(const char *module, const char *name, netlist_t *nl);
bool        syn_add_gate(netlist_t *nl, gate_type_e type, const char *name);
bool        syn_connect(netlist_t *nl, int src_gate, int src_port,
                        int dst_gate, int dst_port);
bool        syn_set_pi(netlist_t *nl, int gate_id);
bool        syn_set_po(netlist_t *nl, int gate_id);
bool        syn_set_clock(netlist_t *nl, int gate_id);
void        syn_free_netlist(netlist_t *nl);

bool        syn_load_tech_lib(const char *filename, tech_lib_t *lib);
std_cell_t *syn_find_cell(tech_lib_t *lib, gate_type_e type, double area_limit);

bool        syn_run_rtl_elaborate(netlist_t *nl, synth_params_t *p);
bool        syn_run_boolean_optimize(netlist_t *nl);
bool        syn_run_tech_map(netlist_t *nl, tech_lib_t *lib, synth_params_t *p);
bool        syn_run_fanout_repair(netlist_t *nl, int max_fanout);
bool        syn_run_clock_gating(netlist_t *nl);
bool        syn_run_retiming(netlist_t *nl, double target_period_ps);

bool        syn_synthesize(netlist_t *nl, tech_lib_t *lib,
                           synth_params_t *p, synth_result_t *r);
void        syn_print_netlist(const netlist_t *nl);
void        syn_print_stats(const synth_result_t *r);
void        syn_write_verilog(const netlist_t *nl, const char *filename);

double      syn_estimate_area(const netlist_t *nl, const tech_lib_t *lib);
double      syn_estimate_delay(const netlist_t *nl, const tech_lib_t *lib);
int         syn_count_cells(const netlist_t *nl);

bool        syn_decompose_aig(netlist_t *nl);
bool        syn_area_recovery(netlist_t *nl, tech_lib_t *lib);
bool        syn_constant_propagation(netlist_t *nl);
bool        syn_dead_code_elimination(netlist_t *nl);

#ifdef __cplusplus
}
#endif
#endif /* SYNTHESIS_H */
