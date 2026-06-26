#ifndef BITSTREAM_GEN_H
#define BITSTREAM_GEN_H

#include "fpga_arch.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* ================================================================
 * L1/L3: FPGA Configuration Bitstream
 * Reference: Xilinx UG470 (7-Series Configuration), VPR bitstream format
 * L4: CRC-32 for bitstream integrity (IEEE 802.3 polynomial)
 * ================================================================ */

/* --- Configuration Frame ---
 * An FPGA bitstream is organized into frames.
 * Each frame configures a column of CLBs.
 * Xilinx 7-series: 101 words/frame, 32 bits/word = 3232 bits/frame
 */
#define FPGA_FRAME_WORDS  101
#define FPGA_FRAME_BITS   (FPGA_FRAME_WORDS * 32)

typedef enum {
    FRAME_TYPE_CLB,     /* CLB configuration */
    FRAME_TYPE_BRAM,    /* Block RAM configuration */
    FRAME_TYPE_DSP,     /* DSP slice configuration */
    FRAME_TYPE_IOB,     /* I/O block configuration */
    FRAME_TYPE_CLK,     /* Clock management */
    FRAME_TYPE_NULL     /* Padding/filler frame */
} FpgaFrameType;

typedef struct {
    FpgaFrameType type;
    int           frame_addr;          /* major address (column) */
    int           minor_addr;          /* minor address (row within column) */
    uint32_t      words[FPGA_FRAME_WORDS];
    uint32_t      ecc;                 /* Error Correction Code */
    int           column;              /* physical column */
    int           row;                 /* physical row */
} FpgaConfigFrame;

/* --- Bitstream Format ---
 * Structured representation of a complete FPGA configuration.
 */
#define FPGA_MAX_FRAMES  4096
#define FPGA_BITSTREAM_SYNC_WORD  0xAA995566  /* Bus width auto-detect */

typedef enum {
    BITSTREAM_BIN,    /* Raw binary (.bit) */
    BITSTREAM_BIT,    /* ASCII bitstream (.bit) */
    BITSTREAM_RBT,    /* Rawbits format (.rbt) */
    BITSTREAM_HEX,    /* Intel HEX format */
    BITSTREAM_SVF     /* Serial Vector Format */
} FpgaBitstreamFormat;

typedef struct {
    /* Header */
    char          design_name[64];
    char          part_name[32];
    char          date[16];
    char          time[16];
    uint32_t      bitstream_length;     /* total bits */
    uint32_t      crc32;                /* CRC-32 checksum */

    /* Frames */
    FpgaConfigFrame frames[FPGA_MAX_FRAMES];
    int              num_frames;

    /* Package pins & configuration */
    int              num_config_pins;
    FpgaBitstreamFormat format;

    /* Statistics */
    uint32_t      total_config_bits;
    uint32_t      compressed_bits;      /* if compression enabled */
    double        compression_ratio;
} FpgaBitstream;

/* --- Frame Address Register (FAR) ---
 * Xilinx 7-series: 32-bit FAR
 * [31:26] reserved, [25:23] block type, [22:19] top/bottom, [18:16] row,
 * [15:7]  major (column), [6:0] minor (frame within column)
 */
typedef struct {
    uint8_t  block_type;     /* 000=CLB, 001=BRAM, 010=DSP, 011=IOB */
    bool     top_half;       /* 0=bottom, 1=top */
    uint8_t  row_addr;       /* 0-4 */
    uint16_t column_addr;    /* major address 0-511 */
    uint8_t  minor_addr;     /* frame within column 0-127 */
} FpgaFrameAddr;

/* --- Configuration Commands ---
 * Xilinx configuration packet types
 */
typedef enum {
    CMD_NULL    = 0x00,
    CMD_WRITE   = 0x01,
    CMD_READ    = 0x02,
    CMD_NOP     = 0x04,
    CMD_RESET   = 0x05,
    CMD_SHUTDOWN= 0x0B,
    CMD_DESYNC  = 0x0D,
    CMD_SYNC    = 0x0E,
    /* Extended commands for modern FPGAs */
    CMD_IPROG   = 0x0F,  /* Internal PROGRAM */
    CMD_WRITE_MASK = 0x10
} FpgaConfigCmd;

/* Configuration packet header */
typedef struct {
    FpgaConfigCmd cmd;
    uint32_t      word_count;
    bool          is_type1;   /* Type 1: register write, Type 2: long write */
    uint8_t       register_addr;
    uint32_t*     data;
    int           data_len;
} FpgaConfigPacket;

/* --- Configuration Register Map ---
 * Key registers in the FPGA configuration interface
 */
typedef enum {
    REG_CRC      = 0x00,
    REG_FAR      = 0x01,  /* Frame Address Register */
    REG_FDRI     = 0x02,  /* Frame Data Register, Input */
    REG_FDRO     = 0x03,  /* Frame Data Register, Output */
    REG_CMD      = 0x04,  /* Command Register */
    REG_CTL0     = 0x05,  /* Control Register 0 */
    REG_MASK     = 0x06,  /* Masking Register */
    REG_STAT     = 0x07,  /* Status Register */
    REG_LOUT     = 0x08,  /* Legacy Output */
    REG_COR0     = 0x09,  /* Configuration Options 0 */
    REG_MFWR     = 0x0A,  /* Multi-Frame Write */
    REG_CBC      = 0x0B,  /* Initial CBC Value */
    REG_IDCODE   = 0x0C,  /* Device ID */
    REG_AXSS     = 0x0D,  /* AXSS Register */
    REG_COR1     = 0x0E,  /* Configuration Options 1 */
    REG_WBSTAR   = 0x10,  /* Warm Boot Start Address */
    REG_TIMER    = 0x11,  /* Watchdog Timer */
    REG_BOOTSTS  = 0x16,  /* Boot History Status */
    REG_CTL1     = 0x18   /* Control Register 1 */
} FpgaConfigReg;

/* L1 API: Bitstream Operations */
FpgaBitstream* bitstream_create(void);
void           bitstream_destroy(FpgaBitstream *bs);
int            bitstream_add_frame(FpgaBitstream *bs, FpgaFrameType type, int col, int row);
void           bitstream_set_frame_word(FpgaConfigFrame *frame, int word_idx, uint32_t value);
uint32_t       bitstream_compute_crc(const FpgaBitstream *bs);
bool           bitstream_validate(const FpgaBitstream *bs);
int            bitstream_write_file(const FpgaBitstream *bs, const char *filename, FpgaBitstreamFormat fmt);
FpgaBitstream* bitstream_read_file(const char *filename);
void           bitstream_print_summary(const FpgaBitstream *bs);
int            bitstream_compress(FpgaBitstream *bs);
uint32_t       bitstream_get_frame_word(const FpgaConfigFrame *frame, int idx);
FpgaFrameAddr  bitstream_decode_far(uint32_t far_value);
uint32_t       bitstream_encode_far(FpgaFrameAddr addr);
FpgaConfigPacket* config_packet_create(FpgaConfigCmd cmd, int word_count);
void              config_packet_destroy(FpgaConfigPacket *pkt);
void              config_packet_set_reg(FpgaConfigPacket *pkt, FpgaConfigReg reg);
void              config_packet_data_write(FpgaConfigPacket *pkt, int idx, uint32_t val);
int               bitstream_generate_partial(FpgaBitstream *bs, const FpgaFabric *fabric,
                                              int region_x, int region_y, int region_w, int region_h);

#endif
