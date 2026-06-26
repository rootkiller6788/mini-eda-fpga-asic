# Mini AI Accelerator Design

**COMPLETE** - 3605 lines include+src, 17 tests pass

DNN accelerator architecture with custom ISA, systolic array, PE microarchitecture, buffer hierarchy, and roofline model.

## Knowledge Coverage

- L1 Definitions: Complete (ISA, SA, PE, Buffer, Roofline structs)
- L2 Core Concepts: Complete (SIMD, systolic rhythm, data reuse, double buffering)
- L3 Engineering: Complete (WS/OS/RS dataflows, 7-stage PE pipeline, DMA engine)
- L4 Theorems: Complete (Roofline, Amdahl Law, ridge point)
- L5 Algorithms: Complete (Tiled GEMM, MAC/FMA, precision conversion, DSE)
- L6 Problems: Complete (MNIST, ResNet, BERT workloads)
- L7 Applications: Partial+ (3 examples + benchmark suite)
- L8 Advanced: Partial (sparse ops, mixed precision, zero-gating)
- L9 Frontiers: Partial (chiplet concepts documented)

## Build & Test

