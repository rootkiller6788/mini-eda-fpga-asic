/* tests/test_bitstream.c - Test Bitstream Generation */
#include "bitstream_gen.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int passed = 0, failed = 0;
#define T(name) printf("  TEST: %s ... ", name)
#define P() do { printf("PASS\n"); passed++; } while(0)
#define F(m) do { printf("FAIL: %s\n", m); failed++; } while(0)

int main(void) {
    printf("=== Test: Bitstream Generation ===\n");

    /* L1: Bitstream create/destroy */
    T("Bitstream create");
    {
        FpgaBitstream *bs = bitstream_create();
        assert(bs != NULL);
        assert(bs->num_frames == 0);
        bitstream_destroy(bs);
        P();
    }

    /* L3: Frame addition */
    T("Add frames");
    {
        FpgaBitstream *bs = bitstream_create();
        int idx = bitstream_add_frame(bs, FRAME_TYPE_CLB, 3, 5);
        assert(idx == 0);
        assert(bs->num_frames == 1);
        assert(bs->frames[0].type == FRAME_TYPE_CLB);
        assert(bs->frames[0].column == 3);
        assert(bs->frames[0].row == 5);
        bitstream_destroy(bs);
        P();
    }

    /* L3: Frame words */
    T("Frame word read/write");
    {
        FpgaConfigFrame frame;
        memset(&frame, 0, sizeof(frame));
        bitstream_set_frame_word(&frame, 0, 0xDEADBEEF);
        assert(bitstream_get_frame_word(&frame, 0) == 0xDEADBEEF);
        bitstream_set_frame_word(&frame, 100, 0x12345678);
        assert(bitstream_get_frame_word(&frame, 100) == 0x12345678);
        P();
    }

    /* L4: CRC-32 computation */
    T("CRC-32 computation");
    {
        FpgaBitstream *bs = bitstream_create();
        bitstream_add_frame(bs, FRAME_TYPE_CLB, 0, 0);
        bitstream_set_frame_word(&bs->frames[0], 0, 0x12345678);
        uint32_t crc = bitstream_compute_crc(bs);
        assert(crc != 0);  /* non-zero CRC for non-empty data */
        bitstream_destroy(bs);
        P();
    }

    /* L4: CRC-32 consistency */
    T("CRC-32 consistency (same data -> same CRC)");
    {
        FpgaBitstream *bs1 = bitstream_create();
        FpgaBitstream *bs2 = bitstream_create();
        bitstream_add_frame(bs1, FRAME_TYPE_CLB, 0, 0);
        bitstream_add_frame(bs2, FRAME_TYPE_CLB, 0, 0);
        for (int w = 0; w < FPGA_FRAME_WORDS; w++) {
            bitstream_set_frame_word(&bs1->frames[0], w, (uint32_t)w);
            bitstream_set_frame_word(&bs2->frames[0], w, (uint32_t)w);
        }
        assert(bitstream_compute_crc(bs1) == bitstream_compute_crc(bs2));
        bitstream_destroy(bs1);
        bitstream_destroy(bs2);
        P();
    }

    /* L4: Bitstream validation */
    T("Bitstream validate empty");
    {
        FpgaBitstream *bs = bitstream_create();
        assert(!bitstream_validate(bs));  /* empty -> invalid */
        bitstream_destroy(bs);
        P();
    }

    /* L3: FAR encoding/decoding */
    T("Frame Address Register encode/decode");
    {
        FpgaFrameAddr addr = {0x02, true, 3, 42, 15};
        uint32_t encoded = bitstream_encode_far(addr);
        FpgaFrameAddr decoded = bitstream_decode_far(encoded);
        assert(decoded.block_type == addr.block_type);
        assert(decoded.top_half == addr.top_half);
        assert(decoded.row_addr == addr.row_addr);
        assert(decoded.column_addr == addr.column_addr);
        assert(decoded.minor_addr == addr.minor_addr);
        P();
    }

    /* L5: Bitstream compression */
    T("Bitstream compression");
    {
        FpgaBitstream *bs = bitstream_create();
        for (int i = 0; i < 10; i++) {
            bitstream_add_frame(bs, FRAME_TYPE_CLB, i, 0);
        }
        int ret = bitstream_compress(bs);
        assert(ret == 0);
        assert(bs->compression_ratio >= 1.0);
        bitstream_destroy(bs);
        P();
    }

    /* L1: Config packet creation */
    T("Config packet create");
    {
        FpgaConfigPacket *pkt = config_packet_create(CMD_WRITE, 4);
        assert(pkt != NULL);
        config_packet_set_reg(pkt, REG_FAR);
        assert(pkt->register_addr == REG_FAR);
        config_packet_data_write(pkt, 0, 0xAB);
        assert(pkt->data[0] == 0xAB);
        config_packet_destroy(pkt);
        P();
    }

    /* L7: Bitstream file I/O */
    T("Bitstream file write/read");
    {
        FpgaBitstream *bs = bitstream_create();
        strcpy(bs->design_name, "test_design");
        bitstream_add_frame(bs, FRAME_TYPE_CLB, 1, 2);
        bitstream_set_frame_word(&bs->frames[0], 0, 0xCAFEBABE);

        assert(bitstream_write_file(bs, "test_output.bin", BITSTREAM_BIN) == 0);

        FpgaBitstream *bs2 = bitstream_read_file("test_output.bin");
        assert(bs2 != NULL);
        assert(bs2->num_frames == 1);
        assert(bitstream_get_frame_word(&bs2->frames[0], 0) == 0xCAFEBABE);

        bitstream_destroy(bs);
        bitstream_destroy(bs2);
        remove("test_output.bin");
        P();
    }

    /* L9: Partial reconfiguration bitstream */
    T("Partial reconfig bitstream");
    {
        FpgaFabric *f = fpga_fabric_create(4, 4, 4, 4);
        FpgaBitstream *bs = bitstream_create();
        int n = bitstream_generate_partial(bs, f, 0, 0, 2, 2);
        assert(n == 4);  /* 2x2 = 4 frames */
        bitstream_destroy(bs);
        fpga_fabric_destroy(f);
        P();
    }

    /* Edge cases */
    T("Read nonexistent file");
    {
        FpgaBitstream *bs = bitstream_read_file("nonexistent.bin");
        assert(bs == NULL);
        P();
    }

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
