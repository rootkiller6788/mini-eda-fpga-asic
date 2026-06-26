#include "fpga_arch.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* =======================================================
 * LUT (Look-Up Table) Implementation
 * ======================================================= */

void fpga_lut_init(FpgaLut *lut)
{
    lut->config = 0;
    lut->output_select = 0;
    lut->mode = LUT_INPUT_COUNT; /* default 6-input LUT mode */
    for (int i = 0; i < LUT_INPUT_COUNT; i++) {
        lut->input_select[i] = i;
    }
}

void fpga_lut_set_mask(FpgaLut *lut, uint64_t mask)
{
    /* Store 16-bit slice; upper bits are for LUT6 mode split */
    lut->config = (uint16_t)(mask & 0xFFFF);
}

static uint8_t lut_fn6(uint64_t mask, uint8_t in)
{
    return (mask >> in) & 1;
}

int fpga_lut_eval(const FpgaLut *lut, uint8_t inputs)
{
    uint64_t full_mask = lut->config;
    if (lut->mode == 5) {
        /* 5-input LUT: use lower 32 bits */
        return lut_fn6(full_mask & 0xFFFFFFFFULL, inputs & 0x1F);
    }
    /* 6-input LUT */
    return lut_fn6(full_mask, inputs & 0x3F);
}

void fpga_lut_set_input(FpgaLut *lut, int idx, uint8_t signal_id)
{
    if (idx >= 0 && idx < LUT_INPUT_COUNT) {
        lut->input_select[idx] = signal_id;
    }
}

/* =======================================================
 * Flip-Flop Implementation
 * ======================================================= */

void fpga_ff_init(FpgaFlipFlop *ff, int type)
{
    ff->type = type;
    ff->state = 0;
    ff->data_input = 0;
    ff->enable_input = 0;
    ff->set_input = 0;
    ff->reset_input = 0;
    ff->clock_input = 0;
    ff->init_value = 0;
    ff->sr_priority = 0;
}

void fpga_ff_reset(FpgaFlipFlop *ff)
{
    ff->state = ff->init_value;
}

void fpga_ff_clock(FpgaFlipFlop *ff, uint8_t data, uint8_t enable,
                    uint8_t set, uint8_t reset)
{
    if (ff->sr_priority == 1) {
        /* set has priority */
        if (set) { ff->state = 1; return; }
        if (reset) { ff->state = 0; return; }
    } else {
        /* reset has priority */
        if (reset) { ff->state = 0; return; }
        if (set) { ff->state = 1; return; }
    }

    if (ff->type == FF_TYPE_DFF) {
        ff->state = data;
    } else if (ff->type == FF_TYPE_DFFE) {
        if (enable) ff->state = data;
    } else if (ff->type == FF_TYPE_DFFSR) {
        ff->state = data;
    }
}

uint8_t fpga_ff_get_q(const FpgaFlipFlop *ff)
{
    return ff->state;
}

/* =======================================================
 * Block RAM (BRAM) Implementation
 * ======================================================= */

void fpga_bram_init(FpgaBram *bram, int width_mode)
{
    bram->width_mode = width_mode;
    bram->port_a_addr = 0;
    bram->port_b_addr = 0;
    bram->port_a_data_in = 0;
    bram->port_b_data_in = 0;
    bram->port_a_data_out = 0;
    bram->port_b_data_out = 0;
    bram->port_a_we = 0;
    bram->port_b_we = 0;
    bram->port_a_en = 0;
    bram->port_b_en = 0;
    memset(bram->memory, 0, sizeof(bram->memory));
    memset(bram->byte_we_a, 0, BRAM_BYTE_WRITE);
    memset(bram->byte_we_b, 0, BRAM_BYTE_WRITE);
}

static uint32_t bram_read_word(const FpgaBram *bram, uint16_t addr)
{
    size_t byte_addr = (size_t)addr * 4;
    if (byte_addr + 3 >= sizeof(bram->memory)) return 0;
    uint8_t b0 = bram->memory[byte_addr];
    uint8_t b1 = bram->memory[byte_addr + 1];
    uint8_t b2 = bram->memory[byte_addr + 2];
    uint8_t b3 = bram->memory[byte_addr + 3];
    return ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) |
           ((uint32_t)b1 << 8)  | (uint32_t)b0;
}

static void bram_write_word(FpgaBram *bram, uint16_t addr, uint32_t data)
{
    size_t byte_addr = (size_t)addr * 4;
    if (byte_addr + 3 >= sizeof(bram->memory)) return;
    bram->memory[byte_addr]     = (uint8_t)(data & 0xFF);
    bram->memory[byte_addr + 1] = (uint8_t)((data >> 8) & 0xFF);
    bram->memory[byte_addr + 2] = (uint8_t)((data >> 16) & 0xFF);
    bram->memory[byte_addr + 3] = (uint8_t)((data >> 24) & 0xFF);
}

void fpga_bram_write(FpgaBram *bram, int port, uint16_t addr, uint32_t data)
{
    if (port == 0) {
        bram->port_a_addr = addr;
        bram->port_a_data_in = data;
    } else {
        bram->port_b_addr = addr;
        bram->port_b_data_in = data;
    }
}

uint32_t fpga_bram_read(const FpgaBram *bram, int port, uint16_t addr)
{
    if (port == 0) {
        return bram_read_word(bram, addr);
    }
    return bram_read_word(bram, addr);
}

void fpga_bram_clock(FpgaBram *bram)
{
    /* Port A */
    if (bram->port_a_en) {
        if (bram->port_a_we) {
            bram_write_word(bram, bram->port_a_addr, bram->port_a_data_in);
        } else {
            bram->port_a_data_out = bram_read_word(bram, bram->port_a_addr);
        }
    }
    /* Port B */
    if (bram->port_b_en) {
        if (bram->port_b_we) {
            bram_write_word(bram, bram->port_b_addr, bram->port_b_data_in);
        } else {
            bram->port_b_data_out = bram_read_word(bram, bram->port_b_addr);
        }
    }
}

/* =======================================================
 * DSP Slice Implementation
 * ======================================================= */

void fpga_dsp_init(FpgaDsp *dsp)
{
    for (int i = 0; i < DSP_NUM_REG_STAGES; i++) {
        dsp->a_reg[i] = 0;
        dsp->b_reg[i] = 0;
    }
    dsp->m_reg = 0;
    dsp->p_reg = 0;
    dsp->a_input = 0;
    dsp->b_input = 0;
    dsp->c_input = 0;
    dsp->opmode = 0;
    dsp->a_pipe_stages = 0;
    dsp->b_pipe_stages = 0;
    dsp->m_pipe_en = 0;
    dsp->p_pipe_en = 0;
}

void fpga_dsp_set_inputs(FpgaDsp *dsp, int32_t a, int32_t b, int64_t c)
{
    dsp->a_input = a;
    dsp->b_input = b;
    dsp->c_input = c;
}

int64_t fpga_dsp_compute(FpgaDsp *dsp, uint8_t opmode)
{
    int64_t result = 0;
    int32_t a = dsp->a_input;
    int32_t b = dsp->b_input;
    int64_t c = dsp->c_input;

    switch (opmode) {
    case 0: /* multiply only */
        result = (int64_t)a * (int64_t)b;
        break;
    case 1: /* multiply + accumulate */
        result = (int64_t)a * (int64_t)b + c;
        break;
    case 2: /* multiply + subtract */
        result = (int64_t)a * (int64_t)b - c;
        break;
    case 3: /* multiply accumulate pre-add */
        result = (int64_t)a * (int64_t)b + c;
        break;
    case 4: /* add only (A+B) */
        result = (int64_t)a + (int64_t)b;
        break;
    default:
        result = 0;
        break;
    }

    dsp->m_reg = (int64_t)a * (int64_t)b;
    dsp->p_reg = result;
    dsp->opmode = opmode;
    return result;
}

void fpga_dsp_clock(FpgaDsp *dsp)
{
    /* Shift pipeline registers */
    for (int i = DSP_NUM_REG_STAGES - 1; i > 0; i--) {
        dsp->a_reg[i] = dsp->a_reg[i - 1];
        dsp->b_reg[i] = dsp->b_reg[i - 1];
    }
    dsp->a_reg[0] = dsp->a_input;
    dsp->b_reg[0] = dsp->b_input;

    /* Recompute on clock edge */
    dsp->m_reg = (int64_t)dsp->a_reg[dsp->a_pipe_stages] *
                 (int64_t)dsp->b_reg[dsp->b_pipe_stages];
}

/* =======================================================
 * Switch Box Implementation
 * ======================================================= */

void fpga_switchbox_init(FpgaSwitchBox *sb, uint8_t num_tracks)
{
    sb->num_tracks = (num_tracks > 32) ? 32 : num_tracks;
    sb->config_bits = 0;
    memset(sb->connections, 0, sizeof(sb->connections));
}

int fpga_switchbox_connect(FpgaSwitchBox *sb,
                            int from_side, int from_track,
                            int to_side, int to_track)
{
    if (from_side < 0 || from_side > 3 || to_side < 0 || to_side > 3)
        return -1;
    if (from_track >= (int)sb->num_tracks || to_track >= (int)sb->num_tracks)
        return -1;
    sb->connections[from_side][from_track] = (uint8_t)(1 << to_side);
    sb->connections[to_side][to_track]     = (uint8_t)(1 << from_side);
    sb->config_bits |= (uint16_t)(1 << from_track);
    return 0;
}

int fpga_switchbox_is_connected(const FpgaSwitchBox *sb,
                                 int from_side, int from_track,
                                 int to_side, int to_track)
{
    if (from_side < 0 || from_side > 3) return 0;
    if (from_track >= (int)sb->num_tracks) return 0;
    return (sb->connections[from_side][from_track] & (1 << to_side)) != 0;
}

/* =======================================================
 * Connection Box Implementation
 * ======================================================= */

void fpga_connbox_init(FpgaConnectionBox *cb, uint8_t num_tracks,
                        double flex, uint8_t num_pins)
{
    cb->num_tracks = num_tracks;
    cb->flex = flex;
    cb->num_pins = (num_pins > 32) ? 32 : num_pins;
    memset(cb->pin_to_track_map, 0xFF, sizeof(cb->pin_to_track_map));
}

int fpga_connbox_bind(FpgaConnectionBox *cb, uint8_t pin, uint8_t track)
{
    if (pin >= cb->num_pins || track >= cb->num_tracks) return -1;
    cb->pin_to_track_map[pin] = track;
    return 0;
}

int fpga_connbox_get_track(const FpgaConnectionBox *cb, uint8_t pin)
{
    if (pin >= cb->num_pins) return -1;
    return (int)cb->pin_to_track_map[pin];
}

/* =======================================================
 * Routing Channel Implementation
 * ======================================================= */

void fpga_channel_init(FpgaRoutingChannel *ch, uint8_t width)
{
    ch->width = (width > CHAN_W_MAX) ? CHAN_W_MAX : width;
    ch->usage_count = 0;
    memset(ch->track_occupied, 0, sizeof(ch->track_occupied));
    for (int i = 0; i < CHAN_W_MAX; i++) {
        /* Model RC delay increasing with track length */
        ch->track_delay_ps[i] = 50.0 + (double)i * 2.5;
    }
}

int fpga_channel_allocate(FpgaRoutingChannel *ch, uint8_t *track_out)
{
    for (uint8_t i = 0; i < ch->width; i++) {
        if (!ch->track_occupied[i]) {
            ch->track_occupied[i] = 1;
            ch->usage_count++;
            if (track_out) *track_out = i;
            return 0;
        }
    }
    return -1; /* no free tracks */
}

void fpga_channel_release(FpgaRoutingChannel *ch, uint8_t track)
{
    if (track < ch->width && ch->track_occupied[track]) {
        ch->track_occupied[track] = 0;
        if (ch->usage_count > 0) ch->usage_count--;
    }
}

double fpga_channel_get_delay(const FpgaRoutingChannel *ch, uint8_t track)
{
    if (track >= ch->width) return 999.0;
    return ch->track_delay_ps[track];
}
