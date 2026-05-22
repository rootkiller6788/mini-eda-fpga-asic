#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fpga_arch.h"

static void demo_lut(void)
{
    printf("\n--- LUT Demo ---\n");
    FpgaLut lut;
    fpga_lut_init(&lut);

    /* AND gate: mask = 0x8000_0000_0000_0000 (only bit 63=1, i.e. all inputs=1) */
    uint64_t and_mask = (1ULL << 63);
    fpga_lut_set_mask(&lut, and_mask);

    printf("  AND gate test:\n");
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) {
            uint8_t in = (uint8_t)(a | (b << 1));
            int result = fpga_lut_eval(&lut, in);
            printf("    A=%d B=%d => %d\n", a & 1, b & 1, result);
        }
    }

    /* XOR gate: mask = 0x6996_0000_0000_0000 */
    uint64_t xor_mask = 0x6996000000000000ULL;
    fpga_lut_set_mask(&lut, xor_mask);
    printf("  XOR gate test:\n");
    for (int a = 0; a < 2; a++) {
        for (int b = 0; b < 2; b++) {
            uint8_t in = (uint8_t)(a | (b << 1));
            int result = fpga_lut_eval(&lut, in);
            printf("    A=%d B=%d => %d\n", a, b, result);
        }
    }
}

static void demo_flipflop(void)
{
    printf("\n--- Flip-Flop Demo ---\n");

    FpgaFlipFlop dff, dffe, dffsr;

    fpga_ff_init(&dff, FF_TYPE_DFF);
    fpga_ff_init(&dffe, FF_TYPE_DFFE);
    fpga_ff_init(&dffsr, FF_TYPE_DFFSR);
    dff.init_value = 0;
    dffe.init_value = 0;
    dffsr.init_value = 0;

    printf("  Basic DFF toggle:\n");
    for (int clk = 0; clk < 5; clk++) {
        uint8_t d = (uint8_t)(clk % 2);
        fpga_ff_clock(&dff, d, 1, 0, 0);
        printf("    clk=%d D=%d => Q=%d\n", clk, d, fpga_ff_get_q(&dff));
    }

    printf("  DFFE (enable=0):\n");
    fpga_ff_clock(&dffe, 1, 0, 0, 0);
    printf("    D=1 CE=0 => Q=%d (should hold)\n", fpga_ff_get_q(&dffe));
    fpga_ff_clock(&dffe, 0, 1, 0, 0);
    printf("    D=0 CE=1 => Q=%d\n", fpga_ff_get_q(&dffe));

    printf("  DFFSR (reset priority):\n");
    fpga_ff_clock(&dffsr, 1, 1, 0, 1);
    printf("    D=1, reset=1 => Q=%d (should be 0)\n", fpga_ff_get_q(&dffsr));
    dffsr.sr_priority = 1;
    fpga_ff_clock(&dffsr, 1, 1, 1, 1);
    printf("    D=1, set=1, reset=1 (set priority) => Q=%d\n",
           fpga_ff_get_q(&dffsr));
}

static void demo_bram(void)
{
    printf("\n--- BRAM Demo ---\n");

    FpgaBram bram;
    fpga_bram_init(&bram, 36);  /* 36-bit width mode */

    /* Write via port A */
    bram.port_a_en = 1;
    bram.port_a_we = 1;
    for (uint16_t addr = 0; addr < 8; addr++) {
        fpga_bram_write(&bram, 0, addr, (uint32_t)(addr * 0x11111111));
    }

    /* Clock BRAM to commit writes */
    fpga_bram_clock(&bram);

    /* Read via port B */
    bram.port_b_en = 1;
    bram.port_b_we = 0;
    printf("  BRAM contents:\n");
    for (uint16_t addr = 0; addr < 8; addr++) {
        uint32_t val = fpga_bram_read(&bram, 1, addr);
        printf("    addr=0x%04X => 0x%08X\n", addr, val);
    }
}

static void demo_dsp(void)
{
    printf("\n--- DSP Slice Demo ---\n");

    FpgaDsp dsp;
    fpga_dsp_init(&dsp);

    /* Multiply: 123 * 456 = 56088 */
    fpga_dsp_set_inputs(&dsp, 123, 456, 0);
    int64_t result = fpga_dsp_compute(&dsp, 0);
    printf("  Multiply:    123 * 456 = %lld\n", (long long)result);

    /* MAC: (25 * 18) + 100 = 550 */
    fpga_dsp_set_inputs(&dsp, 25, 18, 100);
    result = fpga_dsp_compute(&dsp, 1);
    printf("  MAC:         (25 * 18) + 100 = %lld\n", (long long)result);

    /* Multiply-Subtract: (10 * 5) - 3 = 47 */
    fpga_dsp_set_inputs(&dsp, 10, 5, 3);
    result = fpga_dsp_compute(&dsp, 2);
    printf("  MSub:        (10 * 5) - 3 = %lld\n", (long long)result);
}

static void demo_routing(void)
{
    printf("\n--- Routing Demo ---\n");

    /* Routing Channel */
    FpgaRoutingChannel ch_h, ch_v;
    fpga_channel_init(&ch_h, 16);
    fpga_channel_init(&ch_v, 8);

    uint8_t track;
    printf("  Allocating horizontal tracks:\n");
    for (int i = 0; i < 5; i++) {
        if (fpga_channel_allocate(&ch_h, &track) == 0) {
            printf("    Allocated track %d (delay=%.1f ps)\n",
                   track, fpga_channel_get_delay(&ch_h, track));
        }
    }
    printf("  Horizontal usage: %d/16\n", ch_h.usage_count);

    /* Switch Box */
    FpgaSwitchBox sb;
    fpga_switchbox_init(&sb, 8);
    fpga_switchbox_connect(&sb, 0, 0, 1, 0);
    fpga_switchbox_connect(&sb, 0, 1, 2, 1);
    fpga_switchbox_connect(&sb, 0, 2, 3, 2);

    printf("  Switch box connections:\n");
    printf("    Side0-Track0 -> Side1-Track0: %s\n",
           fpga_switchbox_is_connected(&sb, 0, 0, 1, 0) ? "YES" : "NO");
    printf("    Side0-Track0 -> Side2-Track0: %s\n",
           fpga_switchbox_is_connected(&sb, 0, 0, 2, 0) ? "YES" : "NO");

    /* Connection Box */
    FpgaConnectionBox cb;
    fpga_connbox_init(&cb, 16, CONN_BOX_FLEX, 8);
    fpga_connbox_bind(&cb, 0, 3);
    fpga_connbox_bind(&cb, 1, 7);
    fpga_connbox_bind(&cb, 2, 15);
    printf("  Connection box: pin0->track%d, pin1->track%d, pin2->track%d\n",
           fpga_connbox_get_track(&cb, 0),
           fpga_connbox_get_track(&cb, 1),
           fpga_connbox_get_track(&cb, 2));

    /* Release tracks */
    fpga_channel_release(&ch_h, 0);
    fpga_channel_release(&ch_h, 2);
    printf("  After release, horizontal usage: %d/16\n", ch_h.usage_count);
}

int main(void)
{
    printf("=== FPGA Architecture Example ===\n");
    printf("Version: %s\n\n", FPGA_ARCH_VERSION);

    demo_lut();
    demo_flipflop();
    demo_bram();
    demo_dsp();
    demo_routing();

    printf("\n=== All FPGA architecture tests passed ===\n");
    return 0;
}
