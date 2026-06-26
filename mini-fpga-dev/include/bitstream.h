#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_BITSTREAM_BITS 65536
#define MAX_FRAME_SIZE    128
#define MAX_FRAMES        512

typedef enum {
    FRAME_CLB_CFG,
    FRAME_ROUTE_CFG,
    FRAME_IO_CFG,
    FRAME_BRAM_CFG,
    FRAME_DSP_CFG,
    FRAME_CRC
} FrameType;

typedef struct {
    FrameType type;
    int       address;
    uint8_t   data[MAX_FRAME_SIZE / 8];
    int       bit_count;
    uint16_t  crc;
} ConfigFrame;

typedef struct {
    uint8_t      bits[MAX_BITSTREAM_BITS / 8];
    int          bit_count;
    ConfigFrame  frames[MAX_FRAMES];
    int          frame_count;
    char         design_name[64];
    char         device_name[32];
    uint32_t     checksum;
} Bitstream;

void bitstream_init(Bitstream *bs, const char *design, const char *device);
bool bitstream_generate(Bitstream *bs);
bool bitstream_add_frame(Bitstream *bs, FrameType type, int addr, uint8_t *data, int bits);
bool bitstream_verify(Bitstream *bs);
void bitstream_program(Bitstream *bs);
void bitstream_save(Bitstream *bs, const char *filename);
void bitstream_load(Bitstream *bs, const char *filename);
void bitstream_print(Bitstream *bs);
void bitstream_print_summary(Bitstream *bs);

#endif
