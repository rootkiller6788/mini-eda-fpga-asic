#include "interposer_tech.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

static const double si_thermal_k    = 149.0;

void interposer_init(interposer_t *ip, interposer_type_t type,
                     double width_mm, double height_mm) {
    if (!ip) return;
    memset(ip, 0, sizeof(*ip));
    ip->spec.type = type;
    ip->spec.width_mm = width_mm;
    ip->spec.height_mm = height_mm;
    ip->spec.thickness_um = 100.0;
    ip->spec.num_rdl_layers = 4;
    ip->spec.dielectric_constant = 3.9;
    ip->spec.loss_tangent = 0.002;
    ip->ir_drop_mv = 0.0;

    switch (type) {
    case INTERPOSER_SILICON:
        ip->spec.youngs_modulus_gpa = 170.0;
        ip->spec.cte_ppm_per_k = 2.6;
        break;
    case INTERPOSER_ORGANIC_EMIB:
        ip->spec.youngs_modulus_gpa = 25.0;
        ip->spec.cte_ppm_per_k = 17.0;
        ip->spec.dielectric_constant = 3.2;
        ip->spec.loss_tangent = 0.004;
        break;
    case INTERPOSER_GLASS:
        ip->spec.youngs_modulus_gpa = 73.0;
        ip->spec.cte_ppm_per_k = 8.5;
        ip->spec.dielectric_constant = 5.0;
        ip->spec.loss_tangent = 0.005;
        break;
    case INTERPOSER_SILICON_BRIDGE:
        ip->spec.youngs_modulus_gpa = 170.0;
        ip->spec.cte_ppm_per_k = 2.6;
        ip->spec.num_rdl_layers = 2;
        break;
    case INTERPOSER_RDL_FANOUT:
        ip->spec.youngs_modulus_gpa = 10.0;
        ip->spec.cte_ppm_per_k = 10.0;
        ip->spec.num_rdl_layers = 3;
        break;
    default:
        break;
    }
    ip->spec.warpage_um = 0.0;
}

int interposer_place_die(interposer_t *ip, const die_geometry_t *die) {
    if (!ip || !die) return -1;
    if (ip->num_dies >= INTERPOSER_MAX_DIES) return -2;

    for (uint32_t i = 0; i < ip->num_dies; i++) {
        double x_overlap = fmax(0.0, fmin(ip->dies[i].x_um + ip->dies[i].width_um,
                                          die->x_um + die->width_um) -
                                   fmax(ip->dies[i].x_um, die->x_um));
        double y_overlap = fmax(0.0, fmin(ip->dies[i].y_um + ip->dies[i].height_um,
                                          die->y_um + die->height_um) -
                                   fmax(ip->dies[i].y_um, die->y_um));
        if (x_overlap > 0.0 && y_overlap > 0.0) return -3;
    }

    memcpy(&ip->dies[ip->num_dies], die, sizeof(die_geometry_t));
    ip->num_dies++;
    ip->total_power_w += die->power_w;
    return 0;
}

int interposer_remove_die(interposer_t *ip, uint32_t die_index) {
    if (!ip || die_index >= ip->num_dies) return -1;
    for (uint32_t i = die_index; i < ip->num_dies - 1; i++)
        memcpy(&ip->dies[i], &ip->dies[i + 1], sizeof(die_geometry_t));
    ip->num_dies--;
    return 0;
}

int interposer_add_microbump(interposer_t *ip, const microbump_array_t *mb) {
    if (!ip || !mb) return -1;
    if (ip->num_microbumps >= INTERPOSER_MAX_ROUTES) return -2;
    memcpy(&ip->microbumps[ip->num_microbumps], mb, sizeof(microbump_array_t));
    ip->microbumps[ip->num_microbumps].id = ip->num_microbumps;
    ip->num_microbumps++;
    return 0;
}

int interposer_add_tsv(interposer_t *ip, const tsv_site_t *tsv) {
    if (!ip || !tsv) return -1;
    if (ip->num_tsvs >= INTERPOSER_MAX_TSVS) return -2;
    memcpy(&ip->tsvs[ip->num_tsvs], tsv, sizeof(tsv_site_t));
    ip->tsvs[ip->num_tsvs].id = ip->num_tsvs;
    ip->num_tsvs++;
    return 0;
}

int interposer_add_rdl_trace(interposer_t *ip, const rdl_trace_t *trace) {
    if (!ip || !trace) return -1;
    if (ip->num_traces >= INTERPOSER_MAX_ROUTES) return -2;
    memcpy(&ip->traces[ip->num_traces], trace, sizeof(rdl_trace_t));
    ip->traces[ip->num_traces].id = ip->num_traces;
    ip->num_traces++;
    return 0;
}

double interposer_calc_warpage(const interposer_t *ip, double delta_t_k) {
    if (!ip) return 0.0;
    double L = ip->spec.width_mm * 2.0 + ip->spec.height_mm * 2.0;
    double cte = ip->spec.cte_ppm_per_k;
    double thickness = ip->spec.thickness_um * 1e-3;
    double strain = cte * 1e-6 * delta_t_k;
    double curvature = (6.0 * strain) / (thickness * 2.0);
    double warpage = curvature * (L * L) / 8.0;
    return warpage * 1e3;
}

double interposer_calc_thermal_resistance(const interposer_t *ip) {
    if (!ip) return 0.0;
    double area = ip->spec.width_mm * ip->spec.height_mm;
    double thickness_mm = ip->spec.thickness_um * 1e-3;
    double k = si_thermal_k;
    return thickness_mm / (k * area);
}

double interposer_calc_ir_drop(const interposer_t *ip, double total_current_a) {
    if (!ip) return 0.0;
    double total_resistance = 0.0;
    for (uint32_t i = 0; i < ip->num_tsvs; i++)
        total_resistance += ip->tsvs[i].resistance_mohm;
    for (uint32_t i = 0; i < ip->num_microbumps; i++)
        total_resistance += 10.0;
    if (ip->num_tsvs > 0)
        total_resistance /= (double)ip->num_tsvs;
    else
        total_resistance = 50.0;
    return total_current_a * total_resistance;
}

double interposer_calc_signal_delay(const rdl_trace_t *trace) {
    if (!trace) return 0.0;
    double er = 3.9;
    double velocity = 3e8 / sqrt(er);
    double delay_ps = (trace->length_um * 1e-6) / velocity * 1e12;
    return delay_ps;
}

int interposer_route_die_to_die(interposer_t *ip,
                                uint32_t die_a, uint32_t die_b,
                                uint32_t signal_count, uint8_t rdl_layer) {
    if (!ip) return -1;
    if (die_a >= ip->num_dies || die_b >= ip->num_dies) return -2;
    if (ip->num_traces + signal_count > INTERPOSER_MAX_ROUTES) return -3;

    double start_x = ip->dies[die_a].x_um + ip->dies[die_a].width_um / 2.0;
    double start_y = ip->dies[die_a].y_um + ip->dies[die_a].height_um / 2.0;
    double end_x   = ip->dies[die_b].x_um + ip->dies[die_b].width_um / 2.0;
    double end_y   = ip->dies[die_b].y_um + ip->dies[die_b].height_um / 2.0;

    for (uint32_t s = 0; s < signal_count; s++) {
        rdl_trace_t t;
        memset(&t, 0, sizeof(t));
        t.id = ip->num_traces;
        t.layer = rdl_layer;
        t.start_x_um = start_x + (double)s * 2.0;
        t.start_y_um = start_y;
        t.end_x_um = end_x + (double)s * 2.0;
        t.end_y_um = end_y;
        t.width_um = 2.0;
        double dx = t.end_x_um - t.start_x_um;
        double dy = t.end_y_um - t.start_y_um;
        t.length_um = sqrt(dx * dx + dy * dy);
        t.signal_id = s;
        interposer_add_rdl_trace(ip, &t);
    }
    return 0;
}

void interposer_optimize_placement(interposer_t *ip) {
    if (!ip || ip->num_dies < 2) return;
    double total_area = 0.0;
    for (uint32_t i = 0; i < ip->num_dies; i++)
        total_area += ip->dies[i].width_um * ip->dies[i].height_um;

    double sqrt_n = sqrt((double)ip->num_dies);
    double grid_cols = ceil(sqrt_n);
    double spacing_x = ip->spec.width_mm * 1000.0 / grid_cols;
    double spacing_y = 10000.0;

    double x = 1000.0, y = 1000.0;
    for (uint32_t i = 0; i < ip->num_dies; i++) {
        ip->dies[i].x_um = x;
        ip->dies[i].y_um = y;
        x += spacing_x;
        if (x + ip->dies[i].width_um > ip->spec.width_mm * 1000.0) {
            x = 1000.0;
            y += spacing_y;
        }
    }
}

int interposer_verify_drc(const interposer_t *ip) {
    if (!ip) return -1;
    if (ip->num_dies == 0) return 1;
    if (ip->num_dies > INTERPOSER_MAX_DIES) return 2;

    for (uint32_t i = 0; i < ip->num_dies; i++) {
        if (ip->dies[i].x_um < 0.0 || ip->dies[i].y_um < 0.0) return 3;
        if (ip->dies[i].x_um + ip->dies[i].width_um > ip->spec.width_mm * 1000.0)
            return 4;
        if (ip->dies[i].y_um + ip->dies[i].height_um > ip->spec.height_mm * 1000.0)
            return 5;
    }

    for (uint32_t i = 0; i < ip->num_traces; i++) {
        if (ip->traces[i].width_um < 0.4) return 6;
        double dx = ip->traces[i].end_x_um - ip->traces[i].start_x_um;
        double dy = ip->traces[i].end_y_um - ip->traces[i].start_y_um;
        double len = sqrt(dx * dx + dy * dy);
        if (len > 50000.0) return 7;
    }

    return 0;
}

void interposer_print_summary(const interposer_t *ip) {
    if (!ip) return;
    const char *type_names[] = {"Silicon", "Organic EMIB", "Glass",
                                "Silicon Bridge", "RDL Fanout"};
    printf("Interposer Summary:\n");
    printf("  Type: %s\n", type_names[ip->spec.type]);
    printf("  Size: %.1f x %.1f mm\n", ip->spec.width_mm, ip->spec.height_mm);
    printf("  Thickness: %.1f um\n", ip->spec.thickness_um);
    printf("  RDL Layers: %u\n", ip->spec.num_rdl_layers);
    printf("  Dies placed: %u\n", ip->num_dies);
    printf("  Microbumps: %u\n", ip->num_microbumps);
    printf("  TSVs: %u\n", ip->num_tsvs);
    printf("  RDL traces: %u\n", ip->num_traces);
    printf("  Total power: %.2f W\n", ip->total_power_w);
    printf("  IR drop: %.2f mV\n", ip->ir_drop_mv);
    printf("  Warpage: %.2f um\n", ip->spec.warpage_um);
    printf("  Young's Modulus: %.1f GPa\n", ip->spec.youngs_modulus_gpa);
    printf("  CTE: %.1f ppm/K\n", ip->spec.cte_ppm_per_k);
}

void interposer_export_3d_view(const interposer_t *ip, const char *filename) {
    if (!ip || !filename) return;
    printf("[3D View] Exporting to %s (placeholder)\n", filename);
}

double emib_bridge_capacity_gbps(double bridge_width_mm, uint32_t layer_count) {
    double signals_per_mm = 250.0;
    double signals = bridge_width_mm * signals_per_mm * (double)layer_count;
    return signals * 8.0;
}
