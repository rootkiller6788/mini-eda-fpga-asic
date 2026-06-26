# AI Accelerator Architecture

## System Overview

The mini-ai-accel-design module models a complete DNN accelerator
inspired by Google TPU v1, Eyeriss, and Simba architectures.

### Components

1. **DNN ISA** - Custom instruction set with 60+ opcodes
2. **Systolic Array** - 2D MAC grid with configurable dataflow
3. **PE Microarchitecture** - 7-stage pipeline with activation functions
4. **Buffer Hierarchy** - L1/L2/DRAM with DMA and double buffering
5. **Roofline Model** - Performance analysis and DSE

### Dataflow

- Weight Stationary (TPU): weights preloaded, inputs/psum flow
- Output Stationary: psum stays, weights/inputs flow
- Row Stationary (Eyeriss): each PE holds one filter row
- Input Stationary: inputs stay, weights/psum flow

### Memory Hierarchy


