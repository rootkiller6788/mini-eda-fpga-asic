#include "std_cells.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void std_cell_init(std_cell_t *cell, const char *name, cell_category_t cat,
                   vt_type_t vt, drive_strength_t drive, int track_height)
{
    memset(cell, 0, sizeof(*cell));
    strncpy(cell->name, name, sizeof(cell->name) - 1);
    cell->category     = cat;
    cell->vt           = vt;
    cell->drive        = drive;
    cell->track_height = track_height;

    cell->cell_height_nm = (double)(track_height * 200.0);
    cell->cell_width_nm  = (double)((drive + 1) * 320.0);
    cell->area_nm2       = cell->cell_height_nm * cell->cell_width_nm;

    cell->input_count    = 1;
    cell->output_count   = 1;
    cell->is_inverting   = 0;
}

void std_cell_add_timing_arc(std_cell_t *cell, timing_arc_type_t type,
                             double rise_delay, double fall_delay,
                             double input_cap, double output_cap)
{
    timing_arc_t *arc;
    if (cell->timing_arc_count >= MAX_TIMING_ARCS) return;
    arc = &cell->timing_arcs[cell->timing_arc_count++];
    memset(arc, 0, sizeof(*arc));
    arc->type         = type;
    arc->cell_rise    = rise_delay;
    arc->cell_fall    = fall_delay;
    arc->input_cap_ff = input_cap;
    arc->output_cap_ff = output_cap;
}

void std_cell_add_power(std_cell_t *cell, int index,
                        double internal_uw, double leakage_nw)
{
    power_entry_t *pe;
    if (cell->power_entry_count >= MAX_POWER_ENTRIES) return;
    pe = &cell->power_entries[cell->power_entry_count++];
    pe->index            = index;
    pe->internal_power_uw  = internal_uw;
    pe->leakage_power_nw   = leakage_nw;
    pe->switching_power_uw = internal_uw * 0.7;
    pe->total_power_uw     = pe->internal_power_uw + pe->switching_power_uw;
}

void std_cell_add_delay_lut(std_cell_t *cell, timing_arc_type_t arc_type,
                            double slew_ps, double load_ff,
                            double delay_ps, double out_slew_ps)
{
    delay_lut_t *lut;
    delay_lut_entry_t *ent;
    int i;
    for (i = 0; i < cell->delay_lut_count; i++) {
        if (cell->delay_luts[i].arc_type == arc_type) {
            lut = &cell->delay_luts[i];
            if (lut->row_count >= MAX_LUT_ROWS) return;
            ent = &lut->entries[lut->row_count++];
            ent->index         = lut->row_count;
            ent->input_slew_ps = slew_ps;
            ent->output_load_ff = load_ff;
            ent->cell_delay_ps = delay_ps;
            ent->output_slew_ps = out_slew_ps;
            return;
        }
    }
    if (cell->delay_lut_count >= MAX_TIMING_ARCS) return;
    lut = &cell->delay_luts[cell->delay_lut_count++];
    lut->arc_type  = arc_type;
    lut->row_count = 1;
    ent = &lut->entries[0];
    ent->index         = 0;
    ent->input_slew_ps = slew_ps;
    ent->output_load_ff = load_ff;
    ent->cell_delay_ps = delay_ps;
    ent->output_slew_ps = out_slew_ps;
}

void std_cell_lib_init(std_cell_lib_t *lib, const char *name,
                       int tech_node_nm, double track_height_nm,
                       int num_tracks, double supply_v)
{
    memset(lib, 0, sizeof(*lib));
    strncpy(lib->name, name, sizeof(lib->name) - 1);
    lib->tech_node_nm     = tech_node_nm;
    lib->track_height_nm  = track_height_nm;
    lib->num_tracks       = num_tracks;
    lib->supply_voltage_v = supply_v;

    lib->fin_pitch_nm     = tech_node_nm <= 5 ? 27.0 : (tech_node_nm <= 7 ? 30.0 : 42.0);
    lib->fin_height_nm    = tech_node_nm <= 5 ? 30.0 : (tech_node_nm <= 7 ? 32.0 : 40.0);
    lib->gate_length_nm   = (double)tech_node_nm;
    lib->gate_pitch_nm    = (double)(tech_node_nm * 7);
    lib->metal1_pitch_nm  = tech_node_nm <= 7 ? 40.0 : 64.0;
    lib->fin_count        = tech_node_nm <= 5 ? 3 : 2;

    lib->n_fin_width_nm   = tech_node_nm <= 7 ? 6.0 : 8.0;
    lib->p_fin_width_nm   = tech_node_nm <= 7 ? 8.0 : 10.0;
    lib->contacted_poly_pitch_nm = (double)(tech_node_nm * 12);
}

void std_cell_lib_add_cell(std_cell_lib_t *lib, const std_cell_t *cell)
{
    if (lib->cell_count >= MAX_LIB_CELLS) return;
    lib->cells[lib->cell_count++] = *cell;
}

const std_cell_t *std_cell_lib_find(const std_cell_lib_t *lib, const char *name)
{
    int i;
    for (i = 0; i < lib->cell_count; i++) {
        if (strcmp(lib->cells[i].name, name) == 0) return &lib->cells[i];
    }
    return NULL;
}

const std_cell_t *std_cell_lib_find_by_vt(const std_cell_lib_t *lib, vt_type_t vt)
{
    int i;
    for (i = 0; i < lib->cell_count; i++) {
        if (lib->cells[i].vt == vt) return &lib->cells[i];
    }
    return NULL;
}

double std_cell_get_delay(const std_cell_t *cell, timing_arc_type_t arc_type,
                          double slew_ps, double load_ff)
{
    int i, j;
    for (i = 0; i < cell->delay_lut_count; i++) {
        if (cell->delay_lut[i].arc_type == arc_type) {
            const delay_lut_t *lut = &cell->delay_lut[i];
            double best_dist = 1e18, best_delay = 0.0;
            for (j = 0; j < lut->row_count; j++) {
                double ds = slew_ps - lut->entries[j].input_slew_ps;
                double dl = load_ff - lut->entries[j].output_load_ff;
                double dist = ds * ds + dl * dl;
                if (dist < best_dist) {
                    best_dist  = dist;
                    best_delay = lut->entries[j].cell_delay_ps;
                }
            }
            return best_delay;
        }
    }
    for (i = 0; i < cell->timing_arc_count; i++) {
        if (cell->timing_arcs[i].type == arc_type) {
            return (cell->timing_arcs[i].cell_rise +
                    cell->timing_arcs[i].cell_fall) / 2.0;
        }
    }
    return 0.0;
}

double std_cell_get_power(const std_cell_t *cell, double activity_factor,
                          double freq_mhz)
{
    double switching, internal, leakage;
    (void)activity_factor;
    internal   = cell->internal_power_uw;
    leakage    = cell->leakage_power_nw * 1e-3;
    switching  = cell->internal_power_uw * 0.5 * (freq_mhz / 1000.0);
    return switching + internal + leakage;
}

const char *vt_type_name(vt_type_t vt)
{
    static const char *names[] = { "LVT", "SVT", "HVT", "ULVT" };
    if (vt < 0 || vt >= VT_COUNT) return "?";
    return names[vt];
}

const char *drive_strength_name(drive_strength_t drive)
{
    static const char *names[] = { "X1","X2","X4","X8","X16","X32" };
    if (drive < 0 || drive >= DRIVE_COUNT) return "?";
    return names[drive];
}

const char *cell_category_name(cell_category_t cat)
{
    static const char *names[] = {
        "COMB", "SEQ", "CG", "FILL", "TAP", "ANT", "ECO", "PHY"
    };
    if (cat < 0 || cat > CELL_PHYSICAL_ONLY) return "?";
    return names[cat];
}

void std_cell_lib_report(const std_cell_lib_t *lib)
{
    int i;
    printf("=== Standard Cell Library: %s ===\n", lib->name);
    printf("Tech: %d nm  Tracks: %d  Track Height: %.1f nm  VDD: %.2f V\n",
           lib->tech_node_nm, lib->num_tracks, lib->track_height_nm,
           lib->supply_voltage_v);
    printf("Fin pitch: %.1f nm  Fin height: %.1f nm  Gate length: %.1f nm\n",
           lib->fin_pitch_nm, lib->fin_height_nm, lib->gate_length_nm);
    printf("Contacted poly pitch: %.1f nm  M1 pitch: %.1f nm\n",
           lib->contacted_poly_pitch_nm, lib->metal1_pitch_nm);
    printf("Cells: %d\n\n", lib->cell_count);
    for (i = 0; i < lib->cell_count; i++) {
        printf("  %-10s  %-4s %-3s %-3s  %.0f x %.0f nm  Leak: %.2f nW\n",
               lib->cells[i].name, cell_category_name(lib->cells[i].category),
               vt_type_name(lib->cells[i].vt),
               drive_strength_name(lib->cells[i].drive),
               lib->cells[i].cell_width_nm, lib->cells[i].cell_height_nm,
               lib->cells[i].leakage_power_nw);
    }
}

void std_cell_report(const std_cell_t *cell)
{
    printf("Cell: %s  Category: %s  VT: %s  Drive: %s\n",
           cell->name, cell_category_name(cell->category),
           vt_type_name(cell->vt), drive_strength_name(cell->drive));
    printf("  Size: %.0f x %.0f nm  Area: %.0f nm2  Tracks: %d\n",
           cell->cell_width_nm, cell->cell_height_nm, cell->area_nm2,
           cell->track_height);
    printf("  Inputs: %d  Outputs: %d  Inverting: %d\n",
           cell->input_count, cell->output_count, cell->is_inverting);
    printf("  Leakage: %.3f nW  Internal: %.3f uW\n",
           cell->leakage_power_nw, cell->internal_power_uw);
}

double finfet_drive_current_nA(double fin_width_nm, double fin_height_nm,
                               double gate_length_nm, vt_type_t vt)
{
    double mobility, vth, tox, coeff;
    (void)fin_height_nm;
    mobility = (vt == VT_LVT)  ? 250.0 :
               (vt == VT_SVT)  ? 220.0 :
               (vt == VT_HVT)  ? 180.0 : 280.0;
    vth  = (vt == VT_LVT)  ? 0.15 :
           (vt == VT_SVT)  ? 0.25 :
           (vt == VT_HVT)  ? 0.35 : 0.10;
    tox  = 1.2;
    coeff = mobility * 3.9 * 8.854e-12 / (tox * 1e-9);
    return coeff * (fin_width_nm * 1e-9 / (gate_length_nm * 1e-9)) *
           (0.8 - vth) * (0.8 - vth) * 1e9;
}

double finfet_capacitance_ff(double fin_width_nm, double fin_height_nm,
                             double gate_length_nm)
{
    double cox = 3.9 * 8.854e-12 / (1.2e-9);
    double area = fin_width_nm * gate_length_nm * 1e-18;
    double cap = cox * area * 1e15;
    (void)fin_height_nm;
    return cap;
}
