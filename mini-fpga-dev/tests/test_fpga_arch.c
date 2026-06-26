/* tests/test_fpga_arch.c - Test FPGA Architecture */
#include "fpga_arch.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;
#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)

int main(void) {
    printf("=== Test: FPGA Architecture ===\n");

    /* L1: LUT operations */
    TEST("LUT init and eval");
    {
        FpgaLut lut;
        fpga_lut_init(&lut, 2);
        assert(lut.k == 2);
        /* Set AND function: 0,0=0 0,1=0 1,0=0 1,1=1 */
        fpga_lut_set_bit(&lut, 0, false);
        fpga_lut_set_bit(&lut, 1, false);
        fpga_lut_set_bit(&lut, 2, false);
        fpga_lut_set_bit(&lut, 3, true);
        bool in0[] = {false, false};
        bool in1[] = {true, false};
        bool in2[] = {false, true};
        bool in3[] = {true, true};
        assert(fpga_lut_eval(&lut, in0) == false);
        assert(fpga_lut_eval(&lut, in1) == false);
        assert(fpga_lut_eval(&lut, in2) == false);
        assert(fpga_lut_eval(&lut, in3) == true);
        assert(fpga_lut_eval_packed(&lut, 3) == true);
        assert(fpga_lut_eval_packed(&lut, 0) == false);
        PASS();
    }

    /* L1: LUT SET function */
    TEST("LUT OR function");
    {
        FpgaLut lut;
        fpga_lut_init(&lut, 2);
        /* OR: 0=0, 1=1, 2=1, 3=1 */
        fpga_lut_set_bit(&lut, 0, false);
        fpga_lut_set_bit(&lut, 1, true);
        fpga_lut_set_bit(&lut, 2, true);
        fpga_lut_set_bit(&lut, 3, true);
        bool in[] = {true, false};
        assert(fpga_lut_eval(&lut, in) == true);
        PASS();
    }

    /* L2: Fabric creation */
    TEST("Fabric creation and destruction");
    {
        FpgaFabric *f = fpga_fabric_create(4, 4, 4, 4);
        assert(f != NULL);
        assert(f->grid_width == 4);
        assert(f->grid_height == 4);
        assert(f->num_clbs == 16);
        assert(fpga_fabric_config_bits(f) > 0);
        fpga_fabric_destroy(f);
        PASS();
    }

    /* L3: CLB initialization */
    TEST("CLB init");
    {
        FpgaClb clb;
        fpga_clb_init(&clb, 2, 3);
        assert(clb.x == 2);
        assert(clb.y == 3);
        assert(!clb.used);
        assert(clb.slices[0].num_luts_used == 0);
        assert(clb.slices[0].num_ffs_used == 0);
        PASS();
    }

    /* L3: Tile initialization */
    TEST("Tile init with switch box");
    {
        FpgaTile tile;
        fpga_tile_init(&tile, 1, 1, 8);
        assert(tile.x == 1);
        assert(tile.y == 1);
        assert(tile.sb.num_switches > 0);
        PASS();
    }

    /* L4: Rent's Rule */
    TEST("Rent's Rule I/O estimation");
    {
        double io = fpga_rent_io_estimate(100, 3.0, 0.5);
        assert(io > 0);
        /* 3 * sqrt(100) = 30 */
        assert(fabs(io - 30.0) < 1.0);
        PASS();
    }

    /* L4: Minimum channel width */
    TEST("Min channel width estimation");
    {
        int w = fpga_min_channel_width(8, 8, 0.6);
        assert(w >= 2);
        assert(w <= 64);
        PASS();
    }

    /* L4: Wirelength estimation */
    TEST("Total wirelength estimation");
    {
        double wl = fpga_estimate_total_wirelength(64, 0.6);
        assert(wl > 0);
        PASS();
    }

    /* L3: Switch box generation patterns */
    TEST("Switch box: Wilton pattern");
    {
        FpgaSwitchBox sb;
        fpga_switch_box_generate(&sb, 4, SW_PATTERN_WILTON);
        assert(sb.pattern == SW_PATTERN_WILTON);
        assert(sb.num_switches <= 4);
        /* Verify Wilton pattern: i -> W-1-i */
        assert(sb.switches[1].to_track == 2); /* 4-1-1=2 */
        PASS();
    }

    TEST("Switch box: Disjoint pattern");
    {
        FpgaSwitchBox sb;
        fpga_switch_box_generate(&sb, 4, SW_PATTERN_DISJOINT);
        assert(sb.switches[0].to_track == 0);
        PASS();
    }

    /* Edge cases */
    TEST("Fabric null-destroy safety");
    {
        fpga_fabric_destroy(NULL);
        PASS();
    }

    TEST("Fabric minimum grid validation");
    {
        FpgaFabric *f = fpga_fabric_create(2, 2, 2, 2);
        assert(f != NULL);
        assert(fpga_fabric_num_nets(f) == 0);
        fpga_fabric_destroy(f);
        PASS();
    }

    /* L1: FF initialization */
    TEST("Flip-flop init");
    {
        FpgaFlipFlop ff;
        fpga_ff_init(&ff, FF_TYPE_DFFE);
        assert(ff.type == FF_TYPE_DFFE);
        assert(ff.clk_net == -1);
        assert(ff.init_val == false);
        PASS();
    }

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
