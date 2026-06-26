#include "fpga_arch.h"
#include <stdio.h>
#include <string.h>

void fpga_arch_init(FpgaArch *arch, const char *name, int w, int h) {
    strncpy(arch->name, name, sizeof(arch->name) - 1);
    arch->grid_width = w;
    arch->grid_height = h;
    arch->clb_count = w * h;
    arch->lut_count = w * h * 2;
    arch->dsp_count = 0;
    arch->bram_count = 0;
    arch->io_count = (w + h) * 2;
    arch->routing_channel_width = 10.0;
}

void fpga_arch_set_resources(FpgaArch *arch, int clbs, int dsps, int brams, int ios) {
    arch->clb_count = clbs;
    arch->dsp_count = dsps;
    arch->bram_count = brams;
    arch->io_count = ios;
}

void fpga_print_arch(FpgaArch *arch) {
    printf("FPGA Architecture: %s\n", arch->name);
    printf("  Grid: %dx%d\n", arch->grid_width, arch->grid_height);
    printf("  CLBs: %d (LUTs: %d)\n", arch->clb_count, arch->lut_count);
    printf("  DSPs: %d, BRAMs: %d\n", arch->dsp_count, arch->bram_count);
    printf("  IOs: %d\n", arch->io_count);
    printf("  Routing channels: %.1f\n", arch->routing_channel_width);
}

int fpga_total_luts(FpgaArch *arch) { return arch->lut_count; }
int fpga_total_capacity(FpgaArch *arch) { return arch->clb_count; }
