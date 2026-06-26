#ifndef FPGA_ARCH_H
#define FPGA_ARCH_H

#include <stdbool.h>

#define MAX_CLBS       64
#define MAX_LUTS       256
#define MAX_DSP        16
#define MAX_BRAM       16
#define MAX_IOPADS     64
#define MAX_CBX        128
#define MAX_SBX        128

typedef enum { LUT4, LUT6, LUT8 } LutSize;
typedef enum { CB_NORTH, CB_SOUTH, CB_EAST, CB_WEST } CbDirection;

typedef struct {
    int      id;
    LutSize  size;
    int      truth_table[64];
    bool     has_ff;
    bool     ff_output;
    double   delay;
} Lut;

typedef struct {
    int      id;
    int      luts[2];
    bool     has_carry_chain;
    bool     has_ff;
    double   delay;
} Clb;

typedef struct {
    char     name[32];
    int      clb_count;
    int      lut_count;
    int      dsp_count;
    int      bram_count;
    int      io_count;
    int      grid_width;
    int      grid_height;
    double   routing_channel_width;
} FpgaArch;

void fpga_arch_init(FpgaArch *arch, const char *name, int w, int h);
void fpga_arch_set_resources(FpgaArch *arch, int clbs, int dsps, int brams, int ios);
void fpga_print_arch(FpgaArch *arch);
int  fpga_total_luts(FpgaArch *arch);
int  fpga_total_capacity(FpgaArch *arch);

#endif
