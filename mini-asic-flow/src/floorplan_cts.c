#include "floorplan_cts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void floorplan_init(floorplan_t *fp, double core_w, double core_h,
                    int metal_layers)
{
    int i;
    memset(fp, 0, sizeof(*fp));
    fp->core_width_um  = core_w;
    fp->core_height_um = core_h;
    fp->core_area_um2  = core_w * core_h;
    fp->die_width_um   = core_w * 1.1;
    fp->die_height_um  = core_h * 1.1;
    fp->metal_layers   = metal_layers;

    for (i = 0; i < 12; i++) {
        fp->metal_pitch_um[i] = (i < 4) ? 0.064 : (i < 8) ? 0.080 : 0.160;
        fp->metal_width_um[i] = fp->metal_pitch_um[i] * 0.4;
    }
    fp->pg_vdd_v           = 0.8;
    fp->pg_ir_drop_target_mv = 20.0;
    fp->row_height_um      = 0.6;
}

int floorplan_place_macro(floorplan_t *fp, const macro_block_t *macro)
{
    int i;
    if (fp->macro_count >= MAX_MACROS) return -1;
    for (i = 0; i < fp->macro_count; i++) {
        double m1x = fp->macros[i].x_um;
        double m1y = fp->macros[i].y_um;
        double m1w = fp->macros[i].width_um;
        double m1h = fp->macros[i].height_um;
        if (macro->x_um < (m1x + m1w) && (macro->x_um + macro->width_um) > m1x &&
            macro->y_um < (m1y + m1h) && (macro->y_um + macro->height_um) > m1y) {
            return -2;
        }
    }
    fp->macros[fp->macro_count++] = *macro;
    fp->macro_utilization = 0.0;
    for (i = 0; i < fp->macro_count; i++) {
        fp->macro_utilization += (fp->macros[i].width_um * fp->macros[i].height_um);
    }
    fp->macro_utilization /= fp->core_area_um2;
    return 0;
}

int floorplan_add_io_pad(floorplan_t *fp, const io_pad_t *pad)
{
    if (fp->io_count >= MAX_IO_PADS) return -1;
    fp->io_pads[fp->io_count++] = *pad;
    return 0;
}

int floorplan_auto_place_io_ring(floorplan_t *fp, double pitch_um)
{
    int i, total;
    double perimeter, pads_per_side, x, y;
    total     = (int)((2.0 * fp->die_width_um + 2.0 * fp->die_height_um) / pitch_um);
    perimeter = 2.0 * (fp->die_width_um + fp->die_height_um);
    if (fp->io_count > 0) total = fp->io_count;
    pads_per_side = (double)total / 4.0;

    fp->io_count = 0;
    for (i = 0; i < total; i++) {
        int side = i / (int)(pads_per_side + 0.5);
        double pos = (double)(i % (int)(pads_per_side + 0.5)) * pitch_um;
        io_pad_t pad;
        pad.id = i;
        snprintf(pad.name, sizeof(pad.name), "PAD_%d", i);
        pad.type      = IO_SIGNAL;
        pad.width_um  = pitch_um * 0.8;
        pad.height_um = pitch_um * 0.4;
        pad.pitch_um  = pitch_um;
        pad.drive_ma  = 8.0;
        pad.side      = side;

        switch (side) {
        case 0: pad.x_um = pos;                    pad.y_um = 0;                      break;
        case 1: pad.x_um = fp->die_width_um;       pad.y_um = pos;                     break;
        case 2: pad.x_um = fp->die_width_um - pos; pad.y_um = fp->die_height_um;       break;
        case 3: pad.x_um = 0;                      pad.y_um = fp->die_height_um - pos; break;
        }
        if (fp->io_count < MAX_IO_PADS) {
            fp->io_pads[fp->io_count++] = pad;
        }
    }
    fp->io_pitch_um = pitch_um;
    (void)perimeter;
    return fp->io_count;
}

int floorplan_create_pg_grid(floorplan_t *fp, double width_um, double pitch_um,
                              int layers, double vdd_v)
{
    fp->pg_metal_width_um = width_um;
    fp->pg_pitch_um       = pitch_um;
    fp->pg_layers         = layers;
    fp->pg_vdd_v          = vdd_v;
    return 0;
}

double floorplan_utilization(const floorplan_t *fp)
{
    return fp->cell_utilization + fp->macro_utilization;
}

double floorplan_chip_area(const floorplan_t *fp)
{
    return fp->die_width_um * fp->die_height_um;
}

int floorplan_check_macro_overlap(const floorplan_t *fp)
{
    int i, j;
    for (i = 0; i < fp->macro_count; i++) {
        for (j = i + 1; j < fp->macro_count; j++) {
            double ax1 = fp->macros[i].x_um;
            double ay1 = fp->macros[i].y_um;
            double ax2 = ax1 + fp->macros[i].width_um;
            double ay2 = ay1 + fp->macros[i].height_um;
            double bx1 = fp->macros[j].x_um;
            double by1 = fp->macros[j].y_um;
            double bx2 = bx1 + fp->macros[j].width_um;
            double by2 = by1 + fp->macros[j].height_um;
            if (ax1 < bx2 && ax2 > bx1 && ay1 < by2 && ay2 > by1) return (i * 100 + j);
        }
    }
    return 0;
}

void clock_tree_init(clock_tree_t *ct, clock_topology_t topo)
{
    memset(ct, 0, sizeof(*ct));
    ct->topology = topo;
    ct->max_transition_ps = 200.0;
    ct->max_capacitance_ff = 50.0;
    ct->max_fanout = 16.0;
}

int clock_tree_add_domain(clock_tree_t *ct, const char *name,
                          double period_ns, double skew_target_ps)
{
    clock_domain_t *d;
    if (ct->domain_count >= MAX_CLOCK_DOMAINS) return -1;
    d = &ct->domains[ct->domain_count++];
    d->id              = ct->domain_count - 1;
    strncpy(d->name, name, sizeof(d->name) - 1);
    d->period_ns        = period_ns;
    d->skew_target_ps  = skew_target_ps;
    d->latency_target_ns = period_ns * 0.05;
    return d->id;
}

int clock_tree_build_h_tree(clock_tree_t *ct, int domain_id,
                             double cx_um, double cy_um,
                             double width_um, double height_um,
                             int depth)
{
    int i;
    double hw = width_um / 2.0, hh = height_um / 2.0;
    for (i = 0; i < depth; i++) {
        cts_buffer_t *buf;
        if (ct->buffer_count >= MAX_CTS_BUFFERS) break;
        buf = &ct->buffers[ct->buffer_count++];
        buf->id             = ct->buffer_count;
        buf->x_um           = cx_um + (i * 0.1 * width_um);
        buf->y_um           = cy_um + (i * 0.1 * height_um);
        buf->stage          = i;
        buf->drive_strength = (i == 0) ? DRIVE_X16 : (i < 3) ? DRIVE_X8 : DRIVE_X4;
        buf->is_root        = (i == 0);
        buf->parent_id      = (i == 0) ? -1 : (ct->buffer_count - 1);
        buf->delay_ps       = 20.0 + (double)i * 5.0;
    }
    ct->domains[domain_id].buffer_stages = depth;
    ct->domains[domain_id].sink_count    = 1 << depth;
    (void)hw; (void)hh;
    return ct->buffer_count;
}

int clock_tree_build_clock_mesh(clock_tree_t *ct, int domain_id,
                                 double x0, double y0,
                                 double x1, double y1,
                                 int rows, int cols)
{
    int r, c;
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            cts_wire_segment_t *ws;
            if (ct->wire_count >= MAX_GRID_SEGMENTS) break;
            ws = &ct->wires[ct->wire_count++];
            ws->domain_id = domain_id;
            ws->x1_um = x0 + (double)c * (x1 - x0) / (double)cols;
            ws->y1_um = y0 + (double)r * (y1 - y0) / (double)rows;
            ws->x2_um = ws->x1_um + (x1 - x0) / (double)cols;
            ws->y2_um = ws->y1_um;
            ws->wire_rc_ps = 2.0;
        }
    }
    ct->domains[domain_id].sink_count = rows * cols;
    return ct->wire_count;
}

int clock_tree_insert_buffers(clock_tree_t *ct, int domain_id,
                               int drive_strength, int stages)
{
    int i;
    double step = 1.0 / (double)(stages + 1);
    for (i = 0; i < stages; i++) {
        cts_buffer_t *buf;
        if (ct->buffer_count >= MAX_CTS_BUFFERS) break;
        buf = &ct->buffers[ct->buffer_count++];
        buf->id             = ct->buffer_count;
        buf->x_um           = 10.0 + (double)i * 20.0;
        buf->y_um           = 10.0;
        buf->stage          = i + 1;
        buf->drive_strength = drive_strength;
        buf->is_root        = (i == 0);
        buf->parent_id      = (i == 0) ? -1 : (ct->buffer_count - 1);
        buf->delay_ps       = 15.0 + (double)i * 3.0;
    }
    ct->domains[domain_id].buffer_count  += stages;
    ct->domains[domain_id].buffer_stages += stages;
    (void)step;
    return 0;
}

int clock_tree_compute_skew(const clock_tree_t *ct, int domain_id,
                             double *global_skew_ps, double *local_skew_ps)
{
    double min_delay = 1e18, max_delay = 0.0, sum_delay = 0.0;
    int i, count = 0;
    for (i = 0; i < ct->buffer_count; i++) {
        if (!ct->buffers[i].is_root) {
            double d = ct->buffers[i].delay_ps;
            if (d < min_delay) min_delay = d;
            if (d > max_delay) max_delay = d;
            sum_delay += d;
            count++;
        }
    }
    if (count == 0) { *global_skew_ps = 0.0; *local_skew_ps = 0.0; return -1; }
    *global_skew_ps = max_delay - min_delay;
    *local_skew_ps  = (max_delay - sum_delay / (double)count) * 0.3;
    (void)domain_id;
    return 0;
}

int clock_tree_balance_skew(clock_tree_t *ct, int domain_id,
                             double target_ps)
{
    double gs, ls;
    int i, adjusted = 0;
    clock_tree_compute_skew(ct, domain_id, &gs, &ls);
    if (gs <= target_ps) return 0;
    for (i = 0; i < ct->buffer_count; i++) {
        if (!ct->buffers[i].is_root) {
            ct->buffers[i].delay_ps *= (target_ps / (gs + 1.0));
            adjusted++;
        }
    }
    return adjusted;
}

double cts_elmore_delay(double resistance_ohm, double capacitance_ff,
                        double driver_res_ohm)
{
    return (resistance_ohm * capacitance_ff * 1e-12 +
            driver_res_ohm * capacitance_ff * 1e-12) * 1e12;
}

double cts_wire_rc(double length_um, double width_um)
{
    double r_per_um = 0.1 / (width_um + 0.01);
    double c_per_um = 0.2 * width_um + 0.05;
    return r_per_um * c_per_um * length_um;
}

int cts_optimal_buffer_depth(int sink_count, double max_fanout)
{
    int depth = 0, covered = 1;
    while (covered < sink_count && depth < 20) {
        covered *= (int)max_fanout;
        depth++;
    }
    return depth;
}

void floorplan_report(const floorplan_t *fp)
{
    int i;
    printf("=== Floorplan Report ===\n");
    printf("Core: %.1f x %.1f um  Area: %.1f um2\n",
           fp->core_width_um, fp->core_height_um, fp->core_area_um2);
    printf("Die:  %.1f x %.1f um\n", fp->die_width_um, fp->die_height_um);
    printf("Metal layers: %d  PG: %.1f um width, %.1f um pitch, %d layers\n",
           fp->metal_layers, fp->pg_metal_width_um, fp->pg_pitch_um,
           fp->pg_layers);
    printf("Macros: %d  IOs: %d  Macro util: %.1f%%  Cell util: %.1f%%\n",
           fp->macro_count, fp->io_count,
           fp->macro_utilization * 100.0, fp->cell_utilization * 100.0);
    for (i = 0; i < fp->macro_count; i++) {
        printf("  Macro[%d] %s: %.1f x %.1f um @ (%.1f, %.1f)\n",
               i, fp->macros[i].name, fp->macros[i].width_um,
               fp->macros[i].height_um, fp->macros[i].x_um, fp->macros[i].y_um);
    }
}

void clock_tree_report(const clock_tree_t *ct)
{
    int i;
    printf("=== Clock Tree Report ===\n");
    printf("Topology: %d  Domains: %d  Total sinks: %d\n",
           ct->topology, ct->domain_count, ct->total_sinks);
    for (i = 0; i < ct->domain_count; i++) {
        printf("  Domain[%d] %s: period=%.2f ns  skew_target=%.1f ps\n",
               ct->domains[i].id, ct->domains[i].name,
               ct->domains[i].period_ns, ct->domains[i].skew_target_ps);
        printf("    Buffers: %d  Stages: %d  Sinks: %d\n",
               ct->domains[i].buffer_count, ct->domains[i].buffer_stages,
               ct->domains[i].sink_count);
    }
    printf("  Total buffers: %d  Total wires: %d\n",
           ct->buffer_count, ct->wire_count);
}
