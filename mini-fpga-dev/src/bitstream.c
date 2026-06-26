#include "bitstream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc32_calc(const uint8_t *data, int len) {
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

void bitstream_init(Bitstream *bs, const char *design, const char *device) {
    memset(bs->bits, 0, sizeof(bs->bits));
    bs->bit_count = 0;
    bs->frame_count = 0;
    bs->checksum = 0;
    strncpy(bs->design_name, design, sizeof(bs->design_name) - 1);
    strncpy(bs->device_name, device, sizeof(bs->device_name) - 1);
}

bool bitstream_generate(Bitstream *bs) {
    for (int i = 0; i < bs->frame_count; i++) {
        ConfigFrame *f = &bs->frames[i];
        int byte_count = (f->bit_count + 7) / 8;
        f->crc = (uint16_t)crc32_calc(f->data, byte_count);
        for (int b = 0; b < f->bit_count; b++) {
            if (bs->bit_count < (int)(MAX_BITSTREAM_BITS / 8 * 8)) {
                int byte_idx = bs->bit_count / 8;
                int bit_idx = bs->bit_count % 8;
                if (f->data[b / 8] & (1 << (b % 8)))
                    bs->bits[byte_idx] |= (1 << bit_idx);
                bs->bit_count++;
            }
        }
    }
    int byte_count = (bs->bit_count + 7) / 8;
    bs->checksum = crc32_calc(bs->bits, byte_count);
    return true;
}

bool bitstream_add_frame(Bitstream *bs, FrameType type, int addr, uint8_t *data, int bits) {
    if (bs->frame_count >= MAX_FRAMES) return false;
    ConfigFrame *f = &bs->frames[bs->frame_count++];
    f->type = type;
    f->address = addr;
    f->bit_count = bits;
    if (data) memcpy(f->data, data, (bits + 7) / 8);
    else memset(f->data, 0, sizeof(f->data));
    return true;
}

bool bitstream_verify(Bitstream *bs) {
    for (int i = 0; i < bs->frame_count; i++) {
        ConfigFrame *f = &bs->frames[i];
        int byte_count = (f->bit_count + 7) / 8;
        uint16_t crc = (uint16_t)crc32_calc(f->data, byte_count);
        if (crc != f->crc) {
            printf("Frame %d CRC mismatch: expected 0x%04X, got 0x%04X\n", i, f->crc, crc);
            return false;
        }
    }
    printf("Bitstream verification: PASS\n");
    return true;
}

void bitstream_program(Bitstream *bs) {
    printf("Programming device '%s' with design '%s'...\n", bs->device_name, bs->design_name);
    printf("%d bits, %d frames\n", bs->bit_count, bs->frame_count);
    bitstream_verify(bs);
}

void bitstream_save(Bitstream *bs, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) { printf("Cannot open %s for writing\n", filename); return; }
    fwrite(&bs->bit_count, sizeof(int), 1, fp);
    fwrite(bs->bits, sizeof(uint8_t), (bs->bit_count + 7) / 8, fp);
    fwrite(&bs->checksum, sizeof(uint32_t), 1, fp);
    fclose(fp);
    printf("Bitstream saved to %s (%d bits)\n", filename, bs->bit_count);
}

void bitstream_load(Bitstream *bs, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) { printf("Cannot open %s\n", filename); return; }
    fread(&bs->bit_count, sizeof(int), 1, fp);
    fread(bs->bits, sizeof(uint8_t), (bs->bit_count + 7) / 8, fp);
    fread(&bs->checksum, sizeof(uint32_t), 1, fp);
    fclose(fp);
}

void bitstream_print(Bitstream *bs) {
    printf("=== Bitstream ===\n");
    printf("Design: %s, Device: %s\n", bs->design_name, bs->device_name);
    printf("Total bits: %d, Frames: %d\n", bs->bit_count, bs->frame_count);
    printf("Checksum: 0x%08X\n", bs->checksum);
    for (int i = 0; i < bs->frame_count; i++) {
        ConfigFrame *f = &bs->frames[i];
        static const char *tname[] = {"CLB_CFG","ROUTE_CFG","IO_CFG","BRAM_CFG","DSP_CFG","CRC"};
        printf("  Frame %d: %s addr=0x%04X bits=%d crc=0x%04X\n",
               i, tname[f->type], f->address, f->bit_count, f->crc);
    }
}

void bitstream_print_summary(Bitstream *bs) {
    printf("Bitstream: %d bits, %d frames, checksum 0x%08X [%s on %s]\n",
           bs->bit_count, bs->frame_count, bs->checksum,
           bs->design_name, bs->device_name);
}
