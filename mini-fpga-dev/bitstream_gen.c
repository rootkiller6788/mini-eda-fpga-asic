#include "bitstream_gen.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* =======================================================
 * Configuration Frame Implementation
 * ======================================================= */

static uint32_t crc32_ieee(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++) {
            uint32_t mask = (crc & 1) ? 0xEDB88320 : 0;
            crc = (crc >> 1) ^ mask;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

void config_frame_init(ConfigFrame *cf, uint8_t block_type,
                        uint8_t major, uint8_t minor)
{
    memset(cf, 0, sizeof(ConfigFrame));
    cf->block_type = block_type;
    cf->major_addr = major;
    cf->minor_addr = minor;
    cf->is_last_frame = 0;

    /* Construct FAR: block_type[23:21] | major[20:12] | minor[11:0] */
    cf->far_address = ((uint32_t)(block_type & 0x07) << 21) |
                      ((uint32_t)(major & 0x1FF) << 12) |
                      ((uint32_t)(minor & 0xFFF));
}

void config_frame_set_data(ConfigFrame *cf, const uint32_t *data, int words)
{
    int n = (words < FRAME_DATA_WORDS) ? words : FRAME_DATA_WORDS;
    for (int i = 0; i < n; i++) {
        cf->data[i] = data[i];
    }
}

void config_frame_write_far(ConfigFrame *cf, uint32_t far)
{
    cf->far_address = far;
    cf->block_type = (uint8_t)((far >> 21) & 0x07);
    cf->major_addr = (uint8_t)((far >> 12) & 0x1FF);
    cf->minor_addr = (uint8_t)(far & 0xFFF);
}

int config_frame_verify_crc(const ConfigFrame *cf)
{
    uint32_t computed = crc32_ieee((const uint8_t *)cf->data,
                                    sizeof(cf->data));
    return (computed == cf->crc) ? 0 : -1;
}

void config_frame_compute_crc(ConfigFrame *cf)
{
    cf->crc = crc32_ieee((const uint8_t *)cf->data, sizeof(cf->data));
}

/* =======================================================
 * LUT Mask Encoder Implementation
 * ======================================================= */

void lut_mask_init(LutMaskEncoder *lme, uint64_t truth_table)
{
    memset(lme, 0, sizeof(LutMaskEncoder));
    lme->truth_table = truth_table;
    lme->num_frames_occupied = 1;
}

int lut_mask_encode(LutMaskEncoder *lme)
{
    /* Encode 64-bit truth table into 2 x 32-bit frame words */
    lme->frame_data[0] = (uint32_t)(lme->truth_table & 0xFFFFFFFFULL);
    lme->frame_data[1] = (uint32_t)((lme->truth_table >> 32) & 0xFFFFFFFFULL);
    lme->num_frames_occupied = 1;
    return 0;
}

void lut_mask_decode(const LutMaskEncoder *lme, uint64_t *truth_table_out)
{
    if (truth_table_out) {
        *truth_table_out = (uint64_t)lme->frame_data[0] |
                          ((uint64_t)lme->frame_data[1] << 32);
    }
}

int lut_mask_write_to_frame(LutMaskEncoder *lme, ConfigFrame *cf,
                             int frame_idx)
{
    (void)frame_idx;
    cf->data[lme->frame_start_word]     = lme->frame_data[0];
    cf->data[lme->frame_start_word + 1] = lme->frame_data[1];
    return 0;
}

/* =======================================================
 * Routing Mux Bits Implementation
 * ======================================================= */

void routing_mux_init(RoutingMuxBits *rmb, uint8_t num_inputs)
{
    memset(rmb, 0, sizeof(RoutingMuxBits));
    rmb->num_inputs = num_inputs;
    /* select_width = ceil(log2(num_inputs)) */
    rmb->select_width = 0;
    while ((1U << rmb->select_width) < num_inputs) {
        rmb->select_width++;
    }
}

int routing_mux_select(RoutingMuxBits *rmb, uint8_t input)
{
    if (input >= rmb->num_inputs) return -1;
    rmb->selected_input = input;
    return 0;
}

void routing_mux_encode(RoutingMuxBits *rmb)
{
    /* Encode selected_input into encoded_bits */
    rmb->encoded_bits = rmb->selected_input;
}

uint8_t routing_mux_get_selected(const RoutingMuxBits *rmb)
{
    return rmb->selected_input;
}

/* =======================================================
 * Bitstream Implementation
 * ======================================================= */

void bitstream_init(FpgaBitstream *bs, const char *design, const char *part)
{
    memset(bs, 0, sizeof(FpgaBitstream));
    strncpy(bs->design_name, design, sizeof(bs->design_name) - 1);
    strncpy(bs->part_name, part, sizeof(bs->part_name) - 1);
    bs->file_marker = BIN_FILE_MAGIC;
    bs->bitstream_type = BIT_TYPE_FULL;

    /* Auto-fill date/time */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(bs->date_str, sizeof(bs->date_str), "%Y-%m-%d", tm_info);
        strftime(bs->time_str, sizeof(bs->time_str), "%H:%M:%S", tm_info);
    }
}

int bitstream_add_frame(FpgaBitstream *bs, const ConfigFrame *frame)
{
    int max_frames = MAX_FRAMES_PER_COLUMN * MAX_CONFIG_COLUMNS;
    if (bs->num_frames >= max_frames) return -1;
    bs->frames[bs->num_frames] = *frame;
    bs->num_frames++;
    return 0;
}

static void write_uint32_be(FILE *f, uint32_t val)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val);
    fwrite(buf, 1, 4, f);
}

int bitstream_write_file(const FpgaBitstream *bs, const char *filename)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    /* File header */
    uint16_t ver_major = 0x0001, ver_minor = 0x0000;
    fwrite(&ver_major, sizeof(uint16_t), 1, f);
    fwrite(&ver_minor, sizeof(uint16_t), 1, f);

    /* Marker */
    write_uint32_be(f, bs->file_marker);

    /* Design name (padded) */
    char dname[128] = {0};
    strncpy(dname, bs->design_name, sizeof(dname) - 1);
    fwrite(dname, 1, 128, f);

    /* Part name (padded) */
    char pname[64] = {0};
    strncpy(pname, bs->part_name, sizeof(pname) - 1);
    fwrite(pname, 1, 64, f);

    /* Date / Time */
    char dt[64] = {0};
    snprintf(dt, sizeof(dt), "%s %s", bs->date_str, bs->time_str);
    fwrite(dt, 1, 64, f);

    /* Number of frames */
    write_uint32_be(f, (uint32_t)bs->num_frames);

    /* Type */
    write_uint32_be(f, (uint32_t)bs->bitstream_type);

    /* Optional key */
    if (bs->has_encryption) {
        fwrite(bs->key, 1, BIN_FILE_KEY_LEN, f);
    }

    /* Config frames */
    for (int i = 0; i < bs->num_frames; i++) {
        config_frame_compute_crc((ConfigFrame *)&bs->frames[i]);
        write_uint32_be(f, bs->frames[i].far_address);
        for (int j = 0; j < FRAME_DATA_WORDS; j++) {
            write_uint32_be(f, bs->frames[i].data[j]);
        }
        write_uint32_be(f, bs->frames[i].crc);
    }

    fclose(f);
    return 0;
}

static uint32_t read_uint32_be(FILE *f)
{
    uint8_t buf[4];
    if (fread(buf, 1, 4, f) != 4) return 0;
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  | (uint32_t)buf[3];
}

int bitstream_read_file(FpgaBitstream *bs, const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    memset(bs, 0, sizeof(FpgaBitstream));

    /* Skip version */
    fseek(f, 4, SEEK_SET);

    bs->file_marker = read_uint32_be(f);

    /* Design name */
    fread(bs->design_name, 1, 128, f);
    bs->design_name[127] = '\0';

    /* Part name */
    fread(bs->part_name, 1, 64, f);
    bs->part_name[63] = '\0';

    /* Date/time */
    char dt[64];
    fread(dt, 1, 64, f);
    strncpy(bs->date_str, dt, 10);
    bs->date_str[10] = '\0';

    bs->num_frames = (int)read_uint32_be(f);
    bs->bitstream_type = (int)read_uint32_be(f);

    /* Read frames */
    int max_read = bs->num_frames;
    if (max_read > MAX_FRAMES_PER_COLUMN * MAX_CONFIG_COLUMNS) {
        max_read = MAX_FRAMES_PER_COLUMN * MAX_CONFIG_COLUMNS;
    }
    for (int i = 0; i < max_read; i++) {
        bs->frames[i].far_address = read_uint32_be(f);
        for (int j = 0; j < FRAME_DATA_WORDS; j++) {
            bs->frames[i].data[j] = read_uint32_be(f);
        }
        bs->frames[i].crc = read_uint32_be(f);
    }

    fclose(f);
    return 0;
}

int bitstream_verify(const FpgaBitstream *bs)
{
    int frame_errors = 0;
    for (int i = 0; i < bs->num_frames; i++) {
        if (config_frame_verify_crc(&bs->frames[i]) != 0) {
            frame_errors++;
        }
    }
    return frame_errors;
}

void bitstream_get_size(const FpgaBitstream *bs, size_t *bytes_out)
{
    if (bytes_out) {
        *bytes_out = BIN_FILE_HEADER_WORDS * 4 +
                     (size_t)bs->num_frames * FRAME_SIZE_BYTES;
    }
}

/* =======================================================
 * Bitstream Compression (RLE) Implementation
 * ======================================================= */

void bitstream_comp_init(BitstreamCompressor *bc, size_t max_size)
{
    memset(bc, 0, sizeof(BitstreamCompressor));
    bc->magic = COMPRESSION_MAGIC;
    bc->compressed_data = (uint8_t *)calloc(1, max_size);
    bc->data_max = max_size;
}

int bitstream_compress_rle(BitstreamCompressor *bc,
                            const uint8_t *data, size_t len)
{
    if (!bc->compressed_data) return -1;
    size_t out_idx = 0;
    size_t i = 0;

    while (i < len && out_idx + 4 < bc->data_max) {
        /* Count run of identical bytes */
        size_t run_count = 1;
        uint8_t val = data[i];
        while (i + run_count < len && data[i + run_count] == val &&
               run_count < 255) {
            run_count++;
        }

        if (run_count >= COMPRESSION_MIN_RUN) {
            /* Encode (run_count - 3) as count, followed by value */
            bc->compressed_data[out_idx++] = val;
            bc->compressed_data[out_idx++] = (uint8_t)(run_count - COMPRESSION_MIN_RUN);
            i += run_count;
        } else {
            /* Literal byte */
            bc->compressed_data[out_idx++] = val;
            i++;
        }
    }

    bc->uncompressed_size = (uint32_t)len;
    bc->compressed_size = (uint32_t)out_idx;
    return 0;
}

int bitstream_decompress_rle(const BitstreamCompressor *bc,
                              uint8_t *out, size_t out_len)
{
    if (!bc->compressed_data || !out) return -1;
    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < bc->compressed_size && out_idx < out_len) {
        uint8_t val = bc->compressed_data[in_idx];
        uint8_t count;

        /* Check next byte: if zero, it's a literal; otherwise a run */
        if (in_idx + 1 >= bc->compressed_size) {
            out[out_idx++] = val;
            break;
        }

        count = bc->compressed_data[in_idx + 1];
        if (count == 0) {
            /* Reserved / edge case: treat as literal */
            out[out_idx++] = val;
            in_idx++;
        } else if (count <= 252) {
            /* Run of (count + 3) bytes */
            size_t run_len = (size_t)count + COMPRESSION_MIN_RUN;
            for (size_t r = 0; r < run_len && out_idx < out_len; r++) {
                out[out_idx++] = val;
            }
            in_idx += 2;
        } else {
            /* Literal */
            out[out_idx++] = val;
            in_idx++;
        }
    }
    return 0;
}

void bitstream_comp_free(BitstreamCompressor *bc)
{
    free(bc->compressed_data);
    bc->compressed_data = NULL;
}

/* =======================================================
 * Partial Reconfiguration Controller Implementation
 * ======================================================= */

void pr_ctrl_init(PrController *pr)
{
    memset(pr, 0, sizeof(PrController));
    pr->pr_enabled = 0;
}

int pr_ctrl_add_region(PrController *pr, uint32_t start_far,
                        uint32_t end_far, const char *name)
{
    if (pr->num_regions >= PR_REGION_MAX) return -1;
    PartialRegion *r = &pr->regions[pr->num_regions];
    r->region_start_far = start_far;
    r->region_end_far = end_far;
    strncpy(r->module_name, name, sizeof(r->module_name) - 1);
    r->is_active = 0;
    r->module_id = (uint8_t)pr->num_regions;
    pr->num_regions++;
    pr->pr_enabled = 1;
    return 0;
}

int pr_ctrl_configure_region(PrController *pr, int region_idx,
                              const ConfigFrame *frames, int count)
{
    if (region_idx < 0 || region_idx >= pr->num_regions) return -1;
    PartialRegion *r = &pr->regions[region_idx];
    int n = (count < MAX_FRAMES_PER_COLUMN) ? count : MAX_FRAMES_PER_COLUMN;
    for (int i = 0; i < n; i++) {
        r->partial_frames[i] = frames[i];
    }
    r->num_partial_frames = n;
    r->is_active = 1;
    return 0;
}

int pr_ctrl_generate_partial_bitstream(const PrController *pr,
                                        int region_idx,
                                        FpgaBitstream *out_bs)
{
    if (region_idx < 0 || region_idx >= pr->num_regions) return -1;
    const PartialRegion *r = &pr->regions[region_idx];
    if (!r->is_active) return -1;

    bitstream_init(out_bs, r->module_name, "partial");
    out_bs->bitstream_type = BIT_TYPE_PARTIAL;

    for (int i = 0; i < r->num_partial_frames; i++) {
        bitstream_add_frame(out_bs, &r->partial_frames[i]);
    }
    return 0;
}
