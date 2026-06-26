#ifndef FPGA_ARCH_H
#define FPGA_ARCH_H

#include <stdint.h>
#include <stddef.h>

#define FPGA_ARCH_VERSION "1.0.0"

/* LUT Configuration Constants */
#define LUT_INPUT_COUNT     6
#define LUT_CONFIG_BITS     64      /* 2^6 = 64 truth table bits */
#define LUT_OUTPUT_WIDTH    2       /* dual-output capable */

/* Flip-Flop Types */
#define FF_TYPE_DFF         0
#define FF_TYPE_DFFE        1       /* DFF with enable */
#define FF_TYPE_DFFSR       2       /* DFF with set/reset */

/* BRAM Configuration */
#define BRAM_DEPTH          512
#define BRAM_WIDTH          36
#define BRAM_PORT_COUNT     2       /* true dual-port */
#define BRAM_BYTE_WRITE     4       /* byte-write enable bits */

/* DSP Slice Configuration */
#define DSP_A_WIDTH         25
#define DSP_B_WIDTH         18
#define DSP_P_WIDTH         48      /* product width */
#define DSP_ACCUM_WIDTH     48
#define DSP_NUM_REG_STAGES  3

/* Routing Architecture */
#define CHAN_W_MIN           8
#define CHAN_W_MAX         256
#define CONN_BOX_FLEX       0.6    /* Fc: fraction of tracks connected */
#define SWITCH_BOX_FLEX     0.5    /* Fs: fraction of other sides */

/* Maximum Architecture Dimensions */
#define MAX_CLB_ROWS       256
#define MAX_CLB_COLS       256

/* -------------------------------------------------------
 * LUT (Look-Up Table) — 6-input LUT with configurable mask
 * ------------------------------------------------------- */
typedef struct {
    uint16_t config;                    /* 16-bit truth table entry */
    uint8_t  input_select[LUT_INPUT_COUNT]; /* input signal routing */
    uint8_t  output_select;             /* which of 2 outputs */
    uint8_t  mode;                      /* LUT5/LUT6/ROM mode */
} FpgaLut;

void  fpga_lut_init(FpgaLut *lut);
void  fpga_lut_set_mask(FpgaLut *lut, uint64_t mask);
int   fpga_lut_eval(const FpgaLut *lut, uint8_t inputs);
void  fpga_lut_set_input(FpgaLut *lut, int idx, uint8_t signal_id);

/* -------------------------------------------------------
 * Flip-Flop — D-type with optional enable, set, reset
 * ------------------------------------------------------- */
typedef struct {
    int     type;               /* FF_TYPE_* */
    uint8_t state;              /* current stored value (0 or 1) */
    uint8_t data_input;         /* D pin routing id */
    uint8_t enable_input;       /* CE pin routing id */
    uint8_t set_input;          /* set pin routing id */
    uint8_t reset_input;        /* reset pin routing id */
    uint8_t clock_input;        /* clock pin routing id */
    uint8_t init_value;         /* initial value for config */
    uint8_t sr_priority;        /* 0: reset first; 1: set first */
} FpgaFlipFlop;

void  fpga_ff_init(FpgaFlipFlop *ff, int type);
void  fpga_ff_reset(FpgaFlipFlop *ff);
void  fpga_ff_clock(FpgaFlipFlop *ff, uint8_t data, uint8_t enable,
                    uint8_t set, uint8_t reset);
uint8_t fpga_ff_get_q(const FpgaFlipFlop *ff);

/* -------------------------------------------------------
 * Block RAM (BRAM) — dual-port, configurable width/depth
 * ------------------------------------------------------- */
typedef struct {
    uint8_t  memory[BRAM_DEPTH * BRAM_WIDTH / 8];
    uint16_t port_a_addr;
    uint16_t port_b_addr;
    uint32_t port_a_data_in;
    uint32_t port_b_data_in;
    uint32_t port_a_data_out;
    uint32_t port_b_data_out;
    uint8_t  port_a_we;          /* write enable port A */
    uint8_t  port_b_we;          /* write enable port B */
    uint8_t  port_a_en;          /* port A enable */
    uint8_t  port_b_en;          /* port B enable */
    int      width_mode;         /* x1, x2, x4, x9, x18, x36 */
    uint8_t  byte_we_a[BRAM_BYTE_WRITE];
    uint8_t  byte_we_b[BRAM_BYTE_WRITE];
} FpgaBram;

void  fpga_bram_init(FpgaBram *bram, int width_mode);
void  fpga_bram_write(FpgaBram *bram, int port, uint16_t addr, uint32_t data);
uint32_t fpga_bram_read(const FpgaBram *bram, int port, uint16_t addr);
void  fpga_bram_clock(FpgaBram *bram);

/* -------------------------------------------------------
 * DSP Slice — multiply + accumulate + pipeline registers
 * ------------------------------------------------------- */
typedef struct {
    int32_t  a_reg[DSP_NUM_REG_STAGES];   /* A pipeline regs */
    int32_t  b_reg[DSP_NUM_REG_STAGES];   /* B pipeline regs */
    int64_t  m_reg;                       /* multiplier output reg */
    int64_t  p_reg;                       /* product/accumulate output */
    int32_t  a_input;
    int32_t  b_input;
    int64_t  c_input;                     /* cascade/accumulate input */
    uint8_t  opmode;                      /* multiply, mac, etc */
    uint8_t  a_pipe_stages;
    uint8_t  b_pipe_stages;
    uint8_t  m_pipe_en;
    uint8_t  p_pipe_en;
} FpgaDsp;

void  fpga_dsp_init(FpgaDsp *dsp);
void  fpga_dsp_set_inputs(FpgaDsp *dsp, int32_t a, int32_t b, int64_t c);
int64_t fpga_dsp_compute(FpgaDsp *dsp, uint8_t opmode);
void  fpga_dsp_clock(FpgaDsp *dsp);

/* -------------------------------------------------------
 * Switch Box — connects routing wires between channels
 * ------------------------------------------------------- */
typedef struct {
    uint16_t config_bits;          /* programmable switch matrix */
    uint8_t  num_tracks;           /* tracks per channel side */
    uint8_t  connections[4][32];   /* up to 32 connections per side */
} FpgaSwitchBox;

void  fpga_switchbox_init(FpgaSwitchBox *sb, uint8_t num_tracks);
int   fpga_switchbox_connect(FpgaSwitchBox *sb,
                              int from_side, int from_track,
                              int to_side, int to_track);
int   fpga_switchbox_is_connected(const FpgaSwitchBox *sb,
                                   int from_side, int from_track,
                                   int to_side, int to_track);

/* -------------------------------------------------------
 * Connection Box — connects CLB pins to routing tracks
 * ------------------------------------------------------- */
typedef struct {
    double  flex;                    /* Fc: fraction of tracks per pin */
    uint8_t num_tracks;              /* routing tracks per channel */
    uint16_t pin_to_track_map[32];   /* which tracks each pin connects to */
    uint8_t num_pins;
} FpgaConnectionBox;

void  fpga_connbox_init(FpgaConnectionBox *cb, uint8_t num_tracks,
                         double flex, uint8_t num_pins);
int   fpga_connbox_bind(FpgaConnectionBox *cb, uint8_t pin, uint8_t track);
int   fpga_connbox_get_track(const FpgaConnectionBox *cb, uint8_t pin);

/* -------------------------------------------------------
 * Routing Channel — W wires spanning a tile boundary
 * ------------------------------------------------------- */
typedef struct {
    uint8_t  width;                 /* number of tracks */
    uint8_t  track_occupied[CHAN_W_MAX];
    double   track_delay_ps[CHAN_W_MAX];  /* per-track delay */
    uint32_t usage_count;
} FpgaRoutingChannel;

void  fpga_channel_init(FpgaRoutingChannel *ch, uint8_t width);
int   fpga_channel_allocate(FpgaRoutingChannel *ch, uint8_t *track_out);
void  fpga_channel_release(FpgaRoutingChannel *ch, uint8_t track);
double fpga_channel_get_delay(const FpgaRoutingChannel *ch, uint8_t track);

#endif /* FPGA_ARCH_H */
