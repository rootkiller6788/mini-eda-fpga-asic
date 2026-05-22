#include "chiplet_ucie.h"
#include "chiplet_d2d.h"
#include "chiplet_interposer.h"
#include "chiplet_thermal.h"
#include "chiplet_partition.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Chiplet Design Demo ===\n\n");
    /* UCIe link */ UcieLink link; ucie_link_init(&link, 8);
    ucie_link_train(&link); ucie_send_flit(&link, 0xDEADBEEF);
    ucie_print(&link);
    /* D2D interface */ D2DInterface d2d; memset(&d2d,0,sizeof(d2d)); d2d.pitch_um = 45.0;
    d2d_channel_model(&d2d, 2.0, 16.0); d2d_place_bump(&d2d, 0, 0);
    d2d_print(&d2d);
    /* Interposer */ Interposer ip; interposer_init(&ip, 20.0, 20.0);
    interposer_place_bumps(&ip, 0, 16, 2.0, 5.0); interposer_route(&ip, 0, 2.0, 5.0, 8.0, 5.0);
    interposer_print(&ip);
    /* Thermal */ ThermalModel tm; memset(&tm,0,sizeof(tm)); thermal_set_ambient(&tm, 25.0);
    thermal_add_node(&tm, "CPU", 10.0, 0, 0, 2.0); thermal_add_node(&tm, "GPU", 15.0, 5, 0, 2.0);
    thermal_add_node(&tm, "Memory", 3.0, 10, 0, 1.5);
    thermal_solve(&tm); thermal_report(&tm);
    /* Partition */ PartitionPlan pp; partition_init(&pp, 3);
    partition_add_block(&pp, BLOCK_LOGIC, "CPU_Cluster", 8.0, 10.0);
    partition_add_block(&pp, BLOCK_MEMORY, "HBM_Stack", 12.0, 5.0);
    partition_add_block(&pp, BLOCK_IO, "PCIe_PHY", 3.0, 2.0);
    partition_add_block(&pp, BLOCK_ACCEL, "NVDLA", 6.0, 8.0);
    partition_func(&pp); partition_print(&pp);
    return 0;
}
