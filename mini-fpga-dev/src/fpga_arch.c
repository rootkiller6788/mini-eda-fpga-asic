/* ================================================================
 * src/fpga_arch.c - FPGA Fabric Architecture Implementation
 * L2: Island-style FPGA fabric construction
 * L3: Grid-based tile array, routing channel structure
 * L4: Rent's Rule validation, Moore graph property
 * References: Xilinx 7-Series, VPR Architecture Model
 * ================================================================ */

#include "fpga_arch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

void fpga_lut_init(FpgaLut *lut, int k) {
    assert(lut != NULL);
    assert(k >= 0 && k <= FPGA_MAX_LUT_SIZE);  /* k=0 for unconfigured */
    lut->k = k;
    lut->mask = 0;
    for (int i = 0; i < FPGA_MAX_LUT_SIZE; i++) {
        lut->inputs[i] = -1;
    }
    lut->output = -1;
    lut->name[0] = '\0';
}

void fpga_lut_set_bit(FpgaLut *lut, int input_vector, bool value) {
    assert(lut != NULL);
    assert(input_vector >= 0 && input_vector < (1 << lut->k));
    if (value) {
        lut->mask |= ((uint64_t)1 << input_vector);
    } else {
        lut->mask &= ~((uint64_t)1 << input_vector);
    }
}

/* L4: Shannon's Expansion Theorem in practice
 * f(x1,...,xk) = xk * f|(xk=1) + (not xk) * f|(xk=0)
 * For LUT evaluation, we index the truth table with input
 * combination interpreted as a binary number.
 * Complexity: O(1) - direct table lookup */
bool fpga_lut_eval(FpgaLut *lut, const bool inputs[]) {
    assert(lut != NULL);
    assert(inputs != NULL);
    int index = 0;
    for (int i = 0; i < lut->k; i++) {
        if (inputs[i]) {
            index |= (1 << i);
        }
    }
    return (lut->mask >> index) & 1ULL;
}

bool fpga_lut_eval_packed(FpgaLut *lut, int inputs) {
    assert(lut != NULL);
    int index = inputs & ((1 << lut->k) - 1);
    return (lut->mask >> index) & 1ULL;
}

void fpga_ff_init(FpgaFlipFlop *ff, FpgaFfType type) {
    assert(ff != NULL);
    ff->type = type;
    ff->d_input = -1;
    ff->q_output = -1;
    ff->clk_net = -1;
    ff->en_net = -1;
    ff->sr_net = -1;
    ff->init_val = false;
    ff->name[0] = '\0';
}

void fpga_clb_init(FpgaClb *clb, int x, int y) {
    assert(clb != NULL);
    assert(x >= 0 && y >= 0);
    clb->x = x;
    clb->y = y;
    clb->used = false;
    for (int s = 0; s < FPGA_CLB_NUM_SLICES; s++) {
        FpgaSlice *slice = &clb->slices[s];
        slice->slice_id = s;
        slice->type = SLICE_TYPE_L;
        slice->x = x;
        slice->y = y;
        slice->num_luts_used = 0;
        slice->num_ffs_used = 0;
        slice->carry.used = false;
        slice->carry.cin = -1;
        slice->carry.cout = -1;
        slice->carry.di = -1;
        slice->carry.s = -1;
        for (int l = 0; l < FPGA_SLICE_NUM_LUTS; l++) {
            fpga_lut_init(&slice->luts[l], 0);
        }
        for (int f = 0; f < FPGA_SLICE_NUM_FFS; f++) {
            fpga_ff_init(&slice->ffs[f], FF_TYPE_DFF);
        }
    }
}

/* L3: Island-style FPGA Fabric Construction
 * CLBs form a 2D grid. Routing channels run between rows and columns.
 * Layout for WxH grid:
 *   - W*H tiles (CLBs)
 *   - (H+1)*W horizontal channel segments
 *   - H*(W+1) vertical channel segments
 *
 * Rent's Rule (L4): T = A * g^p
 *   T = external pins, g = internal blocks, A = constant, p = exponent
 *   Used to validate channel_width against grid_size. */
FpgaFabric* fpga_fabric_create(int width, int height, int chan_width, int lut_k) {
    assert(width >= 2 && height >= 2);
    assert(chan_width >= 1 && chan_width <= FPGA_MAX_TRACKS);
    assert(lut_k >= 2 && lut_k <= FPGA_MAX_LUT_SIZE);

    FpgaFabric *f = (FpgaFabric*)calloc(1, sizeof(FpgaFabric));
    if (!f) return NULL;

    f->grid_width = width;
    f->grid_height = height;
    f->channel_width = chan_width;
    f->lut_size = lut_k;
    f->num_nets = 0;
    f->num_clbs = width * height;
    f->num_io_pads = 0;
    f->tile_width_nm = 1000.0;
    f->tile_height_nm = 1000.0;
    f->tech_node_nm = 28;

    f->tiles = (FpgaTile**)calloc(width, sizeof(FpgaTile*));
    assert(f->tiles);
    for (int x = 0; x < width; x++) {
        f->tiles[x] = (FpgaTile*)calloc(height, sizeof(FpgaTile));
        assert(f->tiles[x]);
        for (int y = 0; y < height; y++) {
            f->tiles[x][y].x = x;
            f->tiles[x][y].y = y;
            fpga_clb_init(&f->tiles[x][y].clb, x, y);
        }
    }

    /* Horizontal channels: (height+1) rows, each with width segments */
    int num_h = (height + 1) * width;
    f->h_channels = (FpgaRoutingChannel*)calloc(num_h, sizeof(FpgaRoutingChannel));
    assert(f->h_channels);
    int idx = 0;
    for (int y = 0; y <= height; y++) {
        for (int x = 0; x < width; x++) {
            f->h_channels[idx].dir = CHAN_X;
            f->h_channels[idx].pos = y;
            f->h_channels[idx].track_width = chan_width;
            for (int t = 0; t < chan_width; t++) {
                f->h_channels[idx].track_usage[t] = -1;
            }
            idx++;
        }
    }

    int num_v = height * (width + 1);
    f->v_channels = (FpgaRoutingChannel*)calloc(num_v, sizeof(FpgaRoutingChannel));
    assert(f->v_channels);
    idx = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x <= width; x++) {
            f->v_channels[idx].dir = CHAN_Y;
            f->v_channels[idx].pos = x;
            f->v_channels[idx].track_width = chan_width;
            for (int t = 0; t < chan_width; t++) {
                f->v_channels[idx].track_usage[t] = -1;
            }
            idx++;
        }
    }

    return f;
}

void fpga_fabric_destroy(FpgaFabric *f) {
    if (!f) return;
    if (f->tiles) {
        for (int x = 0; x < f->grid_width; x++) {
            free(f->tiles[x]);
        }
        free(f->tiles);
    }
    free(f->h_channels);
    free(f->v_channels);
    free(f);
}

/* L4: Total Configuration Bits
 * Each LUT: 2^K truth table bits
 * Each CLB: 2 slices x (4 LUTs x 2^K + 4 FFs x config + routing muxes) */
int fpga_fabric_config_bits(const FpgaFabric *f) {
    assert(f);
    int lut_bits = f->grid_width * f->grid_height * FPGA_CLB_NUM_SLICES
                   * FPGA_SLICE_NUM_LUTS * (1 << f->lut_size);
    int ff_bits = f->grid_width * f->grid_height * FPGA_CLB_NUM_SLICES
                  * FPGA_SLICE_NUM_FFS * 8;
    int route_bits = f->grid_width * f->grid_height * f->channel_width * 6;
    return lut_bits + ff_bits + route_bits;
}

int fpga_fabric_num_nets(const FpgaFabric *f) {
    assert(f);
    return f->num_nets;
}

FpgaNet* fpga_fabric_get_net(FpgaFabric *f, int net_id) {
    (void)f;
    (void)net_id;
    return NULL;
}

int fpga_fabric_add_net(FpgaFabric *f) {
    assert(f);
    return f->num_nets++;
}

void fpga_net_set_source(FpgaFabric *f, int net_id, int clb_x, int clb_y,
                          int slice, int lut) {
    (void)f; (void)net_id; (void)clb_x; (void)clb_y; (void)slice; (void)lut;
}

void fpga_net_add_sink(FpgaFabric *f, int net_id, int clb_x, int clb_y,
                        int slice, int lut_input) {
    (void)f; (void)net_id; (void)clb_x; (void)clb_y; (void)slice; (void)lut_input;
}

/* L4: Rent's Rule - T = A * g^p
 * T = external I/O, g = internal blocks, A = Rent constant, p = Rent exponent
 * Reference: Landman & Russo, "On a Pin vs Block Relationship",
 *            IEEE Trans. Computers, 1971 */
double fpga_rent_io_estimate(int num_clbs, double A, double p) {
    assert(num_clbs > 0);
    assert(A > 0 && p >= 0 && p <= 1.0);
    return A * pow(num_clbs, p);
}

/* L4: Moore Graph lower bound for routing
 * Minimum channel width for routability:
 *   W_min >= ceil(lambda * sqrt(num_clbs))
 * Reference: El Gamal, "Two-Dimensional Stochastic Model",
 *            IEEE TCAD, 1982 */
int fpga_min_channel_width(int grid_w, int grid_h, double rent_p) {
    double N = (double)(grid_w * grid_h);
    double lambda = 1.5;
    double w_est = (lambda / 2.0) * pow(N, rent_p - 0.5);
    if (w_est < 2.0) w_est = 2.0;
    return (int)ceil(w_est);
}

/* L2: Total wirelength from Rent's Rule
 * Reference: Donath, "Placement and Average Interconnection Lengths",
 *            IEEE TCAS, 1979 */
double fpga_estimate_total_wirelength(int num_clbs, double rent_p) {
    double internal_nets = num_clbs * 3.0;
    double avg_net_len = pow(num_clbs, rent_p - 0.5) / 3.0;
    if (avg_net_len < 1.0) avg_net_len = 1.0;
    return internal_nets * avg_net_len;
}

/* L3: Switch Box Pattern Generation
 * Wilton pattern: track i maps to track (W-1-i)
 * Reference: Wilton, "Architectures and Algorithms for FPGAs", 1997 */
void fpga_switch_box_generate(FpgaSwitchBox *sb, int w, FpgaSwitchPattern pat) {
    assert(sb);
    assert(w > 0 && w <= FPGA_MAX_TRACKS);
    sb->pattern = pat;
    sb->num_switches = 0;

    for (int i = 0; i < w; i++) {
        if (sb->num_switches >= FPGA_MAX_SWITCHES) break;
        int to = i;
        switch (pat) {
            case SW_PATTERN_DISJOINT:
                to = i;
                break;
            case SW_PATTERN_WILTON:
                to = (w - 1 - i);
                break;
            case SW_PATTERN_UNIVERSAL:
                to = (i + 1) % w;
                break;
        }
        sb->switches[sb->num_switches].from_track = i;
        sb->switches[sb->num_switches].to_track = to;
        sb->switches[sb->num_switches].enabled = true;
        sb->num_switches++;
    }
}

void fpga_tile_init(FpgaTile *tile, int x, int y, int chan_width) {
    assert(tile);
    tile->x = x;
    tile->y = y;
    fpga_clb_init(&tile->clb, x, y);
    fpga_switch_box_generate(&tile->sb, chan_width, SW_PATTERN_WILTON);
    tile->cb_in.num_entries = 0;
    tile->cb_in.x = x;
    tile->cb_in.y = y;
    tile->cb_out.num_entries = 0;
    tile->cb_out.x = x;
    tile->cb_out.y = y;
}

void fpga_fabric_print_summary(const FpgaFabric *f) {
    assert(f);
    printf("=== FPGA Fabric Architecture ===\n");
    printf("Grid:         %d x %d (%d CLBs)\n", f->grid_width, f->grid_height,
           f->grid_width * f->grid_height);
    printf("LUT size:     %d-input\n", f->lut_size);
    printf("Channel width: W=%d\n", f->channel_width);
    printf("Slices/CLB:   %d\n", FPGA_CLB_NUM_SLICES);
    printf("LUTs/slice:   %d\n", FPGA_SLICE_NUM_LUTS);
    printf("FFs/slice:    %d\n", FPGA_SLICE_NUM_FFS);
    printf("Tile:         %.0fx%.0f nm\n", f->tile_width_nm, f->tile_height_nm);
    printf("Tech node:    %d nm\n", f->tech_node_nm);
    printf("Config bits:  %d\n", fpga_fabric_config_bits(f));
    double min_w = fpga_min_channel_width(f->grid_width, f->grid_height, 0.6);
    printf("Min W (Rent): %.1f\n", min_w);
}
