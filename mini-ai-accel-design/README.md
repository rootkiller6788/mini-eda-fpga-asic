# mini-ai-accel-design - AI Accelerator Design Space Exploration (C99)
AI accelerator architecture: tensor cores, dataflow DSE, sparse computation engines, memory hierarchy modeling, and roofline analysis.
## Features
- **Tensor Core**: MMA operations, warp scheduling, mixed precision (FP16/FP32/INT8/BF16)
- **Dataflow DSE**: Weight/Output/Input/Row stationary energy models, Pareto frontier
- **Sparse Engine**: EIE-like weight sparsity, block sparsity, balanced sparsity
- **Memory Hierarchy**: Multi-level (RF/L1/GBuf/DRAM) energy and bandwidth modeling
- **Roofline**: Compute/memory bound analysis, optimization suggestions
## Build
`make && make test`
## Course Alignment
Stanford CS217, MIT 6.5930, UC Berkeley CS294
## License
MIT
