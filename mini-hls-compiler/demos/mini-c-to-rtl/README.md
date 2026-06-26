# mini-c-to-rtl: C Function to RTL Synthesis

Convert simple C arithmetic functions into synthesizable Verilog RTL.

## Overview

Demonstrates the full HLS flow:
1. Parse C source into a DataFlowGraph (DFG)
2. Schedule operations using ASAP algorithm
3. Allocate hardware resources (adders, multipliers)
4. Bind operations to registers using left-edge lifetime analysis
5. Generate FSMD controller with Verilog output

## Run

```sh
cd mini-hls-compiler
make
./examples/example_hls.exe
```

## Example Output

The demo synthesizes `result = a * b + c` into a 6-state FSMD
with 3 load units, 1 multiplier, 1 adder, and 4 registers.
