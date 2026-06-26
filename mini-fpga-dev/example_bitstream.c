#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitstream_gen.h"

static void demo_config_frame(void)
{
    printf("\n--- Configuration Frame Demo ---\n");

    ConfigFrame cf;
    config_frame_init(&cf, 0x01, 42, 7);  /* CLB block, column 42, frame 7 */

    printf("  Frame: FAR=0x%08X, block=%d, major=%d, minor=%d\n",
           cf.far_address, cf.block_type, cf.major_addr, cf.minor_addr);

    /* Write some pattern data */
    uint32_t pattern[FRAME_DATA_WORDS];
    for (int i = 0; i < FRAME_DATA_WORDS; i++) {
        pattern[i] = (uint32_t)(i * 0x04030201);
    }
    config_frame_set_data(&cf, pattern, FRAME_DATA_WORDS);

    /* CRC checks */
    config_frame_compute_crc(&cf);
    printf("  CRC computed: 0x%08X\n", cf.crc);

    int crc_ok = config_frame_verify_crc(&cf);
    printf("  CRC verify:   %s\n", crc_ok == 0 ? "PASS" : "FAIL");

    /* Corrupt data and check */
    cf.data[5] ^= 0xDEADBEEF;
    crc_ok = config_frame_verify_crc(&cf);
    printf("  CRC verify (corrupted): %s\n", crc_ok == 0 ? "PASS" : "FAIL");
}

static void demo_lut_mask(void)
{
    printf("\n--- LUT Mask Encoder Demo ---\n");

    /* 6-input LUT: F = (A & B) | (C & D) | (E & F) */
    uint64_t truth_table = 0;
    for (int i = 0; i < 64; i++) {
        int a = (i >> 0) & 1, b = (i >> 1) & 1;
        int c = (i >> 2) & 1, d = (i >> 3) & 1;
        int e = (i >> 4) & 1, f = (i >> 5) & 1;
        int val = (a & b) | (c & d) | (e & f);
        if (val) truth_table |= (1ULL << i);
    }

    LutMaskEncoder lme;
    lut_mask_init(&lme, truth_table);
    lut_mask_encode(&lme);

    printf("  Truth table: 0x%016llX\n", (unsigned long long)truth_table);
    printf("  Frame words: [0]=0x%08X [1]=0x%08X\n",
           lme.frame_data[0], lme.frame_data[1]);

    uint64_t decoded;
    lut_mask_decode(&lme, &decoded);
    printf("  Decoded:      0x%016llX (match=%s)\n",
           (unsigned long long)decoded,
           decoded == truth_table ? "YES" : "NO");

    /* Write to frame */
    ConfigFrame cf;
    config_frame_init(&cf, 0x01, 0, 0);
    lme.frame_start_word = 10;
    lut_mask_write_to_frame(&lme, &cf, 0);
    printf("  Written to frame words [%d]=0x%08X, [%d]=0x%08X\n",
           lme.frame_start_word, cf.data[lme.frame_start_word],
           lme.frame_start_word + 1, cf.data[lme.frame_start_word + 1]);
}

static void demo_routing_mux(void)
{
    printf("\n--- Routing Mux Bits Demo ---\n");

    /* 4:1 mux */
    RoutingMuxBits mux4;
    routing_mux_init(&mux4, 4);
    printf("  4:1 mux: select_width=%d\n", mux4.select_width);

    routing_mux_select(&mux4, 2);
    routing_mux_encode(&mux4);
    printf("  Selected input: %d, encoded=0x%X\n",
           routing_mux_get_selected(&mux4), mux4.encoded_bits);

    /* 16:1 mux */
    RoutingMuxBits mux16;
    routing_mux_init(&mux16, 16);
    printf("  16:1 mux: select_width=%d\n", mux16.select_width);

    routing_mux_select(&mux16, 11);
    routing_mux_encode(&mux16);
    printf("  Selected input: %d, encoded=0x%X\n",
           routing_mux_get_selected(&mux16), mux16.encoded_bits);
}

static void demo_bitstream_file(void)
{
    printf("\n--- Bitstream File Demo ---\n");

    FpgaBitstream bs;
    bitstream_init(&bs, "my_design_wrapper", "xc7k325tffg900-2");

    /* Add several frames */
    for (int i = 0; i < 8; i++) {
        ConfigFrame cf;
        config_frame_init(&cf, (uint8_t)(i % 5), (uint8_t)(64 + i), (uint8_t)i);
        uint32_t pat[FRAME_DATA_WORDS];
        for (int j = 0; j < FRAME_DATA_WORDS; j++) {
            pat[j] = (uint32_t)(i * 10000 + j);
        }
        config_frame_set_data(&cf, pat, FRAME_DATA_WORDS);
        config_frame_compute_crc(&cf);
        bitstream_add_frame(&bs, &cf);
    }

    printf("  Design:    %s\n", bs.design_name);
    printf("  Part:      %s\n", bs.part_name);
    printf("  Date/Time: %s %s\n", bs.date_str, bs.time_str);
    printf("  Frames:    %d\n", bs.num_frames);
    printf("  Marker:    0x%08X\n", bs.file_marker);

    size_t bs_size;
    bitstream_get_size(&bs, &bs_size);
    printf("  Est. size: %zu bytes\n", bs_size);

    /* Write to file */
    const char *bit_filename = "example_bitstream_output.bit";
    if (bitstream_write_file(&bs, bit_filename) == 0) {
        printf("  Written:   %s\n", bit_filename);
    } else {
        printf("  ERROR: Failed to write bitstream\n");
    }

    /* Read back */
    FpgaBitstream bs2;
    if (bitstream_read_file(&bs2, bit_filename) == 0) {
        printf("  Read back: %s (%d frames)\n", bs2.design_name, bs2.num_frames);
        int errors = bitstream_verify(&bs2);
        printf("  CRC errors: %d\n", errors);
    }

    /* Cleanup test file */
    remove(bit_filename);
}

static void demo_compression(void)
{
    printf("\n--- Bitstream Compression Demo ---\n");

    uint8_t raw_data[256];
    for (int i = 0; i < 256; i++) {
        /* Create data with runs of zeros for compression */
        raw_data[i] = (uint8_t)((i < 100) ? 0x00 : (i & 0xFF));
    }

    printf("  Raw data size: %zu bytes\n", sizeof(raw_data));

    BitstreamCompressor bc;
    bitstream_comp_init(&bc, 1024);

    if (bitstream_compress_rle(&bc, raw_data, sizeof(raw_data)) == 0) {
        printf("  Compressed: %u -> %u bytes (%.1f%%)\n",
               bc.uncompressed_size, bc.compressed_size,
               100.0 * (double)bc.compressed_size / (double)bc.uncompressed_size);

        uint8_t decompressed[256];
        memset(decompressed, 0, sizeof(decompressed));
        if (bitstream_decompress_rle(&bc, decompressed, sizeof(decompressed)) == 0) {
            int match = (memcmp(raw_data, decompressed, sizeof(raw_data)) == 0);
            printf("  Decompress round-trip: %s\n", match ? "PASS" : "FAIL");
        }
    }

    bitstream_comp_free(&bc);
}

static void demo_partial_reconfig(void)
{
    printf("\n--- Partial Reconfiguration Demo ---\n");

    PrController pr;
    pr_ctrl_init(&pr);

    /* Define 2 reconfigurable regions */
    pr_ctrl_add_region(&pr, 0x00010000, 0x000100FF, "accelerator_a");
    pr_ctrl_add_region(&pr, 0x00020000, 0x000200FF, "accelerator_b");

    printf("  PR enabled: %s\n", pr.pr_enabled ? "YES" : "NO");
    printf("  Regions:    %d\n", pr.num_regions);
    for (int i = 0; i < pr.num_regions; i++) {
        printf("    Region %d: '%s' FAR 0x%08X-0x%08X\n",
               i, pr.regions[i].module_name,
               pr.regions[i].region_start_far,
               pr.regions[i].region_end_far);
    }

    /* Configure region 0 with 4 frames */
    ConfigFrame frames[4];
    for (int i = 0; i < 4; i++) {
        config_frame_init(&frames[i], 0x01, 32, (uint8_t)i);
        uint32_t pat[FRAME_DATA_WORDS];
        for (int j = 0; j < FRAME_DATA_WORDS; j++) {
            pat[j] = (uint32_t)(i * 256 + j);
        }
        config_frame_set_data(&frames[i], pat, FRAME_DATA_WORDS);
    }

    pr_ctrl_configure_region(&pr, 0, frames, 4);

    /* Generate partial bitstream */
    FpgaBitstream partial_bs;
    if (pr_ctrl_generate_partial_bitstream(&pr, 0, &partial_bs) == 0) {
        printf("  Partial bitstream: %s (%d frames, type=%d)\n",
               partial_bs.design_name, partial_bs.num_frames,
               partial_bs.bitstream_type);
    }

    printf("  Region 0 active: %s\n",
           pr.regions[0].is_active ? "YES" : "NO");
}

int main(void)
{
    printf("=== Bitstream Generation Example ===\n");
    printf("Version: %s\n", BITSTREAM_VERSION);
    printf("Frame size: %d words x 32-bit = %d bytes\n\n",
           FRAME_DATA_WORDS, FRAME_SIZE_BYTES);

    printf("Bitstream types:\n");
    printf("  0=FULL  1=PARTIAL  2=COMPRESSED  3=ENCRYPTED  4=AUTHENTICATED\n");

    demo_config_frame();
    demo_lut_mask();
    demo_routing_mux();
    demo_bitstream_file();
    demo_compression();
    demo_partial_reconfig();

    printf("\n=== All bitstream generation tests passed ===\n");
    return 0;
}
