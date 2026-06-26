/**
 * mini-fpga-dev FPGA Flow Demo
 * 演示 FPGA 开发完整流程：架构 → LUT映射 → CLB打包 → 布线 → 比特流
 */
#include "fpga_arch.h"
#include "lut_synth.h"
#include "clb_pack.h"
#include "routing_fabric.h"
#include "bitstream.h"
#include <stdio.h>

int main(void) {
    printf("====== FPGA Development Flow Demo ======\n\n");

    printf("--- 1. FPGA Architecture ---\n");
    FpgaArch arch;
    fpga_arch_init(&arch, "Artix-7-mini", 4, 4);
    fpga_arch_set_resources(&arch, 16, 4, 8, 32);
    fpga_print_arch(&arch);

    printf("\n--- 2. LUT Synthesis ---\n");
    BooleanNetwork bn;
    bool_net_init(&bn, 6);
    int a = bool_net_add_input(&bn);
    int b = bool_net_add_input(&bn);
    int c = bool_net_add_input(&bn);
    int ab = bool_net_add_and(&bn, a, b);
    int notc = bool_net_add_not(&bn, c);
    int root = bool_net_add_or(&bn, ab, notc);
    printf("Built Boolean network: %d nodes, root=%d\n", bn.node_count, root);

    LutInstance luts[10]; int lut_count = 0;
    lut_map(&bn, luts, &lut_count, 10);
    for (int i = 0; i < lut_count; i++) lut_print(&luts[i]);

    printf("\n--- 3. CLB Packing ---\n");
    Packer packer;
    packer_init(&packer, luts, lut_count);
    pack_greedy(&packer);
    pack_print(&packer);

    printf("\n--- 4. Routing ---\n");
    RoutingFabric fab;
    fabric_init(&fab, 4, 4, 10);
    fabric_route_path(&fab, 0, 0, 2, 2, 1);
    fabric_print(&fab);

    printf("\n--- 5. Bitstream Generation ---\n");
    Bitstream bs;
    bitstream_init(&bs, "counter", "Artix-7-mini");
    uint8_t cfg_data[] = {0xAA, 0x55, 0x0F};
    bitstream_add_frame(&bs, FRAME_CLB_CFG, 0, cfg_data, 24);
    bitstream_add_frame(&bs, FRAME_ROUTE_CFG, 1, cfg_data, 24);
    bitstream_generate(&bs);
    bitstream_print_summary(&bs);
    bitstream_verify(&bs);

    return 0;
}
