#include "tensor_core.h"
#include "dataflow_dse.h"
#include "sparse_engine.h"
#include "memory_hierarchy.h"
#include "accel_roofline.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== AI Accelerator Design Demo ===\n\n");
    /* Tensor Core */ TensorCore tc; tensor_core_init(&tc, TC_FP16);
    tensor_core_warp_schedule(&tc, 8);
    tensor_core_print(&tc);
    /* Dataflow DSE */ DataflowConfig cfgs[3];
    dse_init_config(&cfgs[0], DF_WEIGHT_STATIONARY, 256);
    dse_init_config(&cfgs[1], DF_OUTPUT_STATIONARY, 256);
    dse_init_config(&cfgs[2], DF_ROW_STATIONARY, 256);
    printf("\n  Dataflow Energy (M=64,N=64,K=64):\n");
    printf("    WS: %.0f pJ\n", dse_eval_weight_stationary(&cfgs[0], 64, 64, 64));
    printf("    OS: %.0f pJ\n", dse_eval_output_stationary(&cfgs[1], 64, 64, 64));
    printf("    RS: %.0f pJ\n", dse_eval_row_stationary(&cfgs[2], 64, 64, 64));
    /* Sparse Engine */ SparseTensor st; sparse_init_tensor(&st, 4096, 0.8, SPARSE_UNSTRUCTURED);
    SparseEngine se; sparse_engine_init(&se, SPARSE_UNSTRUCTURED);
    sparse_engine_eie(&se, &st, NULL);
    sparse_print_stats(&se, &st);
    /* Memory Hierarchy */ MemHierarchy mh; mem_hier_init(&mh);
    mem_hier_add_level(&mh, MEM_RF, 64, 0.1, 1000, 1);
    mem_hier_add_level(&mh, MEM_L1, 256, 0.5, 500, 2);
    mem_hier_add_level(&mh, MEM_GBUF, 2048, 2.0, 200, 10);
    mem_hier_add_level(&mh, MEM_DRAM, 8388608, 20.0, 100, 100);
    mem_hier_dataflow(&mh, 1024, 2048, 512);
    mem_hier_print(&mh);
    /* Roofline */ RooflineModel rm; roofline_init(&rm, 10.0, 500.0);
    printf("\n");
    roofline_report(&rm, 15.0);
    return 0;
}
