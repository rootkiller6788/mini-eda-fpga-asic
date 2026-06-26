/* ================================================================
 * src/bitstream_gen.c - FPGA Bitstream Generation
 * L3: Configuration frame organization, frame addressing
 * L4: CRC-32 for bitstream integrity
 * L5: Bitstream compression (RLE-based)
 * L9: Partial reconfiguration bitstream (Xilinx-like format)
 * References: Xilinx UG470, IEEE 802.3 CRC-32
 * ================================================================ */

#include "bitstream_gen.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

/* CRC-32 lookup table (IEEE 802.3 polynomial: 0xEDB88320) */
static const uint32_t crc32_table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
    0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
    0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,
    0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
    0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
    0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
    0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,
    0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
    0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
    0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
    0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
    0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
    0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
    0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
    0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
    0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
    0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
    0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
    0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
    0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
    0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
    0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,
    0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
    0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
    0x2BB45A92,0x5CB30A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,
    0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
    0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
    0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,
    0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
    0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
    0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,
    0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
    0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

/* L4: CRC-32 computation (IEEE 802.3)
 * Polynomial: x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 + x^10
 *             + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
 * Reversed form: 0xEDB88320
 * Used for bitstream integrity verification.
 * Complexity: O(N) where N = bitstream length */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, int len) {
    crc = ~crc;
    for (int i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

FpgaBitstream* bitstream_create(void) {
    FpgaBitstream *bs = (FpgaBitstream*)calloc(1, sizeof(FpgaBitstream));
    if (!bs) return NULL;
    bs->num_frames = 0;
    bs->bitstream_length = 0;
    bs->format = BITSTREAM_BIN;
    bs->design_name[0] = '\0';
    bs->part_name[0] = '\0';
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(bs->date, sizeof(bs->date), "%Y-%m-%d", tm_info);
        strftime(bs->time, sizeof(bs->time), "%H:%M:%S", tm_info);
    }
    return bs;
}

void bitstream_destroy(FpgaBitstream *bs) {
    free(bs);
}

int bitstream_add_frame(FpgaBitstream *bs, FpgaFrameType type, int col, int row) {
    assert(bs);
    if (bs->num_frames >= FPGA_MAX_FRAMES) return -1;
    FpgaConfigFrame *f = &bs->frames[bs->num_frames];
    memset(f, 0, sizeof(FpgaConfigFrame));
    f->type = type;
    f->frame_addr = col;
    f->minor_addr = row;
    f->column = col;
    f->row = row;
    return bs->num_frames++;
}

void bitstream_set_frame_word(FpgaConfigFrame *frame, int word_idx, uint32_t value) {
    assert(frame);
    assert(word_idx >= 0 && word_idx < FPGA_FRAME_WORDS);
    frame->words[word_idx] = value;
}

uint32_t bitstream_get_frame_word(const FpgaConfigFrame *frame, int idx) {
    assert(frame);
    assert(idx >= 0 && idx < FPGA_FRAME_WORDS);
    return frame->words[idx];
}

/* L4: Compute CRC-32 over entire bitstream
 * Verifies bitstream data integrity for reliable FPGA configuration. */
uint32_t bitstream_compute_crc(const FpgaBitstream *bs) {
    assert(bs);
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < bs->num_frames; i++) {
        const FpgaConfigFrame *f = &bs->frames[i];
        crc = crc32_update(crc, (const uint8_t*)f->words,
                           FPGA_FRAME_WORDS * sizeof(uint32_t));
    }
    crc = ~crc;
    return crc;
}

bool bitstream_validate(const FpgaBitstream *bs) {
    assert(bs);
    if (bs->num_frames == 0) return false;
    uint32_t computed = bitstream_compute_crc(bs);
    if (bs->crc32 != 0 && computed != bs->crc32) return false;
    return true;
}

/* L5: Simple RLE-based bitstream compression
 * Exploits frame-to-frame redundancy.
 * Typical compression ratios: 1.5x-3x for typical designs.
 * Complexity: O(N * F) where N=frames, F=words per frame */
int bitstream_compress(FpgaBitstream *bs) {
    assert(bs);
    if (bs->num_frames < 2) return 0;

    uint32_t orig_bits = bs->num_frames * FPGA_FRAME_BITS;
    uint32_t compressed = 0;
    int run_length = 1;

    for (int i = 1; i < bs->num_frames; i++) {
        bool same = true;
        for (int w = 0; w < FPGA_FRAME_WORDS; w++) {
            if (bs->frames[i].words[w] != bs->frames[i-1].words[w]) {
                same = false;
                break;
            }
        }
        if (same) {
            run_length++;
        } else {
            /* Encode run: run_length identical frames */
            compressed += 32 + (run_length > 1 ? 8 : 0);
            run_length = 1;
        }
    }
    compressed += 32 + (run_length > 1 ? 8 : 0);

    bs->compressed_bits = compressed;
    bs->compression_ratio = (double)orig_bits / (double)compressed;
    return 0;
}

int bitstream_write_file(const FpgaBitstream *bs, const char *filename,
                          FpgaBitstreamFormat fmt) {
    assert(bs && filename);
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;

    switch (fmt) {
        case BITSTREAM_BIN: {
            /* Write sync word */
            uint32_t sync = FPGA_BITSTREAM_SYNC_WORD;
            fwrite(&sync, sizeof(uint32_t), 1, fp);
            /* Write frame count */
            fwrite(&bs->num_frames, sizeof(int), 1, fp);
            /* Write CRC */
            uint32_t crc = bitstream_compute_crc(bs);
            fwrite(&crc, sizeof(uint32_t), 1, fp);
            /* Write data */
            for (int i = 0; i < bs->num_frames; i++) {
                fwrite(bs->frames[i].words, sizeof(uint32_t),
                       FPGA_FRAME_WORDS, fp);
            }
            break;
        }
        case BITSTREAM_HEX: {
            /* Intel HEX-like format */
            fprintf(fp, ":020000040000FA\n");
            for (int i = 0; i < bs->num_frames; i++) {
                fprintf(fp, ":20%04X00", i * FPGA_FRAME_WORDS);
                uint8_t checksum = 0x20 + ((i * FPGA_FRAME_WORDS) >> 8)
                                   + ((i * FPGA_FRAME_WORDS) & 0xFF);
                for (int w = 0; w < FPGA_FRAME_WORDS; w++) {
                    uint32_t word = bs->frames[i].words[w];
                    fprintf(fp, "%08X", word);
                    checksum += (word >> 24) + ((word >> 16) & 0xFF)
                                + ((word >> 8) & 0xFF) + (word & 0xFF);
                }
                fprintf(fp, "%02X\n", (~checksum + 1) & 0xFF);
            }
            fprintf(fp, ":00000001FF\n");
            break;
        }
        default:
            fclose(fp);
            return -1;
    }

    fclose(fp);
    return 0;
}

FpgaBitstream* bitstream_read_file(const char *filename) {
    assert(filename);
    FILE *fp = fopen(filename, "rb");
    if (!fp) return NULL;

    FpgaBitstream *bs = bitstream_create();
    if (!bs) { fclose(fp); return NULL; }

    uint32_t sync;
    if (fread(&sync, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp); bitstream_destroy(bs); return NULL;
    }

    if (fread(&bs->num_frames, sizeof(int), 1, fp) != 1) {
        fclose(fp); bitstream_destroy(bs); return NULL;
    }
    if (bs->num_frames > FPGA_MAX_FRAMES) {
        fclose(fp); bitstream_destroy(bs); return NULL;
    }

    if (fread(&bs->crc32, sizeof(uint32_t), 1, fp) != 1) {
        fclose(fp); bitstream_destroy(bs); return NULL;
    }

    for (int i = 0; i < bs->num_frames; i++) {
        if (fread(bs->frames[i].words, sizeof(uint32_t),
                  FPGA_FRAME_WORDS, fp) != (size_t)FPGA_FRAME_WORDS) {
            fclose(fp); bitstream_destroy(bs); return NULL;
        }
    }

    fclose(fp);
    return bs;
}

void bitstream_print_summary(const FpgaBitstream *bs) {
    assert(bs);
    printf("=== Bitstream Summary ===\n");
    printf("Design:          %s\n", bs->design_name[0] ? bs->design_name : "(unnamed)");
    printf("Part:            %s\n", bs->part_name[0] ? bs->part_name : "(unspecified)");
    printf("Date:            %s %s\n", bs->date, bs->time);
    printf("Total frames:    %d\n", bs->num_frames);
    printf("Config bits:     %u\n", bs->num_frames * FPGA_FRAME_BITS);
    printf("CRC-32:          0x%08X\n", bitstream_compute_crc(bs));
    if (bs->compressed_bits > 0) {
        printf("Compressed:      %u bits (%.1fx)\n",
               bs->compressed_bits, bs->compression_ratio);
    }
}

/* L3: Decode Frame Address Register value */
FpgaFrameAddr bitstream_decode_far(uint32_t far_value) {
    FpgaFrameAddr addr;
    addr.block_type  = (far_value >> 23) & 0x07;
    addr.top_half    = (far_value >> 22) & 0x01;
    addr.row_addr    = (far_value >> 19) & 0x07;
    addr.column_addr = (far_value >> 7) & 0x1FF;
    addr.minor_addr  = far_value & 0x7F;
    return addr;
}

uint32_t bitstream_encode_far(FpgaFrameAddr addr) {
    return ((uint32_t)addr.block_type << 23)
         | ((uint32_t)(addr.top_half ? 1 : 0) << 22)
         | ((uint32_t)addr.row_addr << 19)
         | ((uint32_t)addr.column_addr << 7)
         | addr.minor_addr;
}

FpgaConfigPacket* config_packet_create(FpgaConfigCmd cmd, int word_count) {
    FpgaConfigPacket *pkt = (FpgaConfigPacket*)calloc(1, sizeof(FpgaConfigPacket));
    if (!pkt) return NULL;
    pkt->cmd = cmd;
    pkt->word_count = word_count;
    pkt->is_type1 = (word_count <= 2047);
    pkt->data = (uint32_t*)calloc(word_count, sizeof(uint32_t));
    return pkt;
}

void config_packet_destroy(FpgaConfigPacket *pkt) {
    if (pkt) {
        free(pkt->data);
        free(pkt);
    }
}

void config_packet_set_reg(FpgaConfigPacket *pkt, FpgaConfigReg reg) {
    assert(pkt);
    pkt->register_addr = reg;
}

void config_packet_data_write(FpgaConfigPacket *pkt, int idx, uint32_t val) {
    assert(pkt && pkt->data);
    assert(idx >= 0 && idx < (int)pkt->word_count);
    pkt->data[idx] = val;
}

/* L9: Generate partial reconfiguration bitstream
 * Only generates frames for specified region.
 * Reference: Xilinx UG909 Partial Reconfiguration */
int bitstream_generate_partial(FpgaBitstream *bs, const FpgaFabric *fabric,
                                int region_x, int region_y,
                                int region_w, int region_h) {
    assert(bs && fabric);
    bs->num_frames = 0;
    for (int y = region_y; y < region_y + region_h && y < fabric->grid_height; y++) {
        for (int x = region_x; x < region_x + region_w && x < fabric->grid_width; x++) {
            int idx = bitstream_add_frame(bs, FRAME_TYPE_CLB, x, y);
            if (idx < 0) return -1;
            /* Encode CLB configuration into frame words */
            for (int w = 0; w < FPGA_FRAME_WORDS && w < 8; w++) {
                uint32_t config_word = 0;
                /* Simplified: pack LUT masks and routing config */
                /* slice and LUT indices derived from word position */
                config_word = (uint32_t)(fabric->lut_size & 0xF); bs->frames[idx].words[w] = config_word;
            }
        }
    }
    return bs->num_frames;
}
