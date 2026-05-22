#ifndef BITSTREAM_GEN_H
#define BITSTREAM_GEN_H

#include <stdint.h>
#include <stddef.h>
#include "fpga_arch.h"

#define BITSTREAM_VERSION "1.0.0"

/* Configuration Frame Parameters */
#define FRAME_DATA_WORDS      101        /* 101 32-bit words per frame */
#define FRAME_SIZE_BYTES      (FRAME_DATA_WORDS * 4)
#define MAX_FRAMES_PER_COLUMN 256
#define MAX_CONFIG_COLUMNS    512
#define FAR_WIDTH              32        /* Frame Address Register width */

/* Bitstream Types */
#define BIT_TYPE_FULL          0
#define BIT_TYPE_PARTIAL       1
#define BIT_TYPE_COMPRESSED    2
#define BIT_TYPE_ENCRYPTED     3
#define BIT_TYPE_AUTHENTICATED 4

/* Compression Constants */
#define COMPRESSION_MAGIC      0xA5A55A5A
#define COMPRESSION_ALGO_RLE   0         /* Run-Length Encoding */
#define COMPRESSION_ALGO_LZSS  1         /* LZSS-based */
#define COMPRESSION_MIN_RUN    3         /* minimum run for RLE */

/* Bitstream File Format Header */
#define BIN_FILE_MAGIC \
    ((uint32_t)0x0009 << 16 | 0x0FF0)   /* .bit file marker */
#define BIN_FILE_HEADER_WORDS 13
#define BIN_FILE_KEY_LEN      32

/* Partial Reconfiguration */
#define PR_REGION_MAX          16
#define PR_MODULE_MAX          64

/* -------------------------------------------------------
 * Configuration Frame — smallest addressable config unit
 * ------------------------------------------------------- */
typedef struct {
    uint32_t far_address;               /* Frame Address Register value */
    uint32_t data[FRAME_DATA_WORDS];    /* frame payload */
    uint8_t  block_type;               /* CLB/BRAM/DSP/CLK/IO */
    uint8_t  major_addr;               /* column address */
    uint8_t  minor_addr;               /* frame within column */
    uint8_t  is_last_frame;            /* end-of-config marker */
    uint32_t crc;                       /* frame CRC */
} ConfigFrame;

void  config_frame_init(ConfigFrame *cf, uint8_t block_type,
                         uint8_t major, uint8_t minor);
void  config_frame_set_data(ConfigFrame *cf, const uint32_t *data, int words);
void  config_frame_write_far(ConfigFrame *cf, uint32_t far);
int   config_frame_verify_crc(const ConfigFrame *cf);
void  config_frame_compute_crc(ConfigFrame *cf);

/* -------------------------------------------------------
 * LUT Mask Encoder — encodes truth table into frame bits
 * ------------------------------------------------------- */
typedef struct {
    uint64_t truth_table;               /* full truth table */
    uint8_t  frame_start_word;         /* starting word within frame */
    uint8_t  bit_offset;               /* bit offset within word */
    uint8_t  num_frames_occupied;      /* frames spanned for this LUT */
    uint32_t frame_data[4];            /* encoded config bits */
} LutMaskEncoder;

void  lut_mask_init(LutMaskEncoder *lme, uint64_t truth_table);
int   lut_mask_encode(LutMaskEncoder *lme);
void  lut_mask_decode(const LutMaskEncoder *lme, uint64_t *truth_table_out);
int   lut_mask_write_to_frame(LutMaskEncoder *lme, ConfigFrame *cf,
                               int frame_idx);

/* -------------------------------------------------------
 * Routing Mux Bits — encodes mux select into frame data
 * ------------------------------------------------------- */
typedef struct {
    uint8_t  num_inputs;           /* mux input count */
    uint8_t  selected_input;       /* chosen input (0 to num_inputs-1) */
    uint8_t  select_width;         /* ceil(log2(num_inputs)) */
    uint32_t encoded_bits;         /* config bits for the mux */
    uint8_t  frame_word;           /* word within frame */
    uint8_t  bit_pos;              /* bit position within word */
} RoutingMuxBits;

void  routing_mux_init(RoutingMuxBits *rmb, uint8_t num_inputs);
int   routing_mux_select(RoutingMuxBits *rmb, uint8_t input);
void  routing_mux_encode(RoutingMuxBits *rmb);
uint8_t routing_mux_get_selected(const RoutingMuxBits *rmb);

/* -------------------------------------------------------
 * Bitstream — full device configuration image
 * ------------------------------------------------------- */
typedef struct {
    ConfigFrame frames[MAX_FRAMES_PER_COLUMN * MAX_CONFIG_COLUMNS];
    int         num_frames;
    int         num_columns;
    char        design_name[128];
    char        part_name[64];
    char        date_str[32];
    char        time_str[32];
    uint32_t    file_marker;
    uint8_t     key[BIN_FILE_KEY_LEN];
    int         has_encryption;
    int         bitstream_type;        /* BIT_TYPE_* */
} FpgaBitstream;

void  bitstream_init(FpgaBitstream *bs, const char *design, const char *part);
int   bitstream_add_frame(FpgaBitstream *bs, const ConfigFrame *frame);
int   bitstream_write_file(const FpgaBitstream *bs, const char *filename);
int   bitstream_read_file(FpgaBitstream *bs, const char *filename);
int   bitstream_verify(const FpgaBitstream *bs);
void  bitstream_get_size(const FpgaBitstream *bs, size_t *bytes_out);

/* -------------------------------------------------------
 * Bitstream Compression
 * ------------------------------------------------------- */
typedef struct {
    uint32_t    magic;
    uint32_t    uncompressed_size;
    uint32_t    compressed_size;
    uint8_t    *compressed_data;
    size_t      data_max;
} BitstreamCompressor;

void  bitstream_comp_init(BitstreamCompressor *bc, size_t max_size);
int   bitstream_compress_rle(BitstreamCompressor *bc,
                              const uint8_t *data, size_t len);
int   bitstream_decompress_rle(const BitstreamCompressor *bc,
                                uint8_t *out, size_t out_len);
void  bitstream_comp_free(BitstreamCompressor *bc);

/* -------------------------------------------------------
 * Partial Reconfiguration
 * ------------------------------------------------------- */
typedef struct {
    char     module_name[64];
    uint32_t region_start_far;         /* first frame address */
    uint32_t region_end_far;           /* last frame address */
    int      num_partial_frames;
    ConfigFrame partial_frames[MAX_FRAMES_PER_COLUMN];
    uint8_t  module_id;
    int      is_active;
} PartialRegion;

typedef struct {
    PartialRegion regions[PR_REGION_MAX];
    int           num_regions;
    FpgaBitstream  golden_bitstream;    /* initial full config */
    int            pr_enabled;
} PrController;

void  pr_ctrl_init(PrController *pr);
int   pr_ctrl_add_region(PrController *pr, uint32_t start_far,
                          uint32_t end_far, const char *name);
int   pr_ctrl_configure_region(PrController *pr, int region_idx,
                                const ConfigFrame *frames, int count);
int   pr_ctrl_generate_partial_bitstream(const PrController *pr,
                                          int region_idx,
                                          FpgaBitstream *out_bs);

#endif /* BITSTREAM_GEN_H */
