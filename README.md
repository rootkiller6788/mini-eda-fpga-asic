# Mini EDA FPGA ASIC

**From-scratch, zero-dependency C implementations** of EDA tools, FPGA development, ASIC design flows, and hardware description languages. Each module covers the full chip design flow — from RTL design and synthesis to place-and-route, static timing analysis, formal verification, and AI accelerator architecture.

## Modules

| Module | Topics | Key References |
|--------|--------|----------------|
| [mini-hdl-lang](mini-hdl-lang/) | Verilog sim (module/assign/always), VHDL sim, SystemVerilog sim, testbench, waveform | IEEE 1364, IEEE 1800 |
| [mini-eda-tools](mini-eda-tools/) | Synthesis (RTL→gate netlist), P&R, STA (setup/hold), power analysis, DFM | Synopsys DC, Cadence Innovus |
| [mini-fpga-dev](mini-fpga-dev/) | LUT/FF/BRAM/DSP, Vivado/Quartus flow, bitstream gen, timing closure, I/O planning | Xilinx Vivado, Intel Quartus |
| [mini-asic-flow](mini-asic-flow/) | RTL→GDSII flow, standard cells, floorplan, CTS, ECO, DRC/LVS, tape-out checklist | Cadence/Synopsys Flow |
| [mini-hardware-verification](mini-hardware-verification/) | UVM methodology (driver/monitor/scoreboard), assertion (SVA/PSL), formal (SymbiYosys), coverage | IEEE 1800.2 UVM, SVA |
| [mini-hls-compiler](mini-hls-compiler/) | C→RTL (Vivado HLS), loop unroll/pipeline, dataflow, array partition, interface pragmas | Vivado HLS, Catapult HLS |
| [mini-riscv-arch](mini-riscv-arch/) | RISC-V RV32I core design, 5-stage pipeline, hazard handling, CSR, privilege levels | RISC-V ISA Manual Vol 1-2 |
| [mini-noc-design](mini-noc-design/) | Network-on-Chip: topology (mesh/torus), XY routing, VC, wormhole, AXI, latency/throughput | Dally "Principles of NoC" |
| [mini-chiplet-design](mini-chiplet-design/) | Chiplet: UCIe/D2D, interposer, MCM, HBM integration, die-to-die PHY, thermal analysis | UCIe Spec, OCP ODSA |
| [mini-ai-accel-design](mini-ai-accel-design/) | DNN accelerator RTL, systolic array RTL, PE microarchitecture, buffer hierarchy, ISA | Google TPU, Eyeriss, Simba |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **EDA simulation in user-space** — educational models of chip design toolchains, verification methodologies, and hardware architectures
- **Theory-to-code mapping** — every module includes `docs/` with standard/spec-alignment notes
- **Practical demos** — Verilog simulator, RISC-V core simulator, STA engine, HLS scheduler, NoC simulator, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-hdl-lang
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-eda-fpga-asic/
├── mini-hdl-lang/               # Hardware Description Languages
├── mini-eda-tools/              # EDA Tools
├── mini-fpga-dev/               # FPGA Development
├── mini-asic-flow/              # ASIC Design Flow
├── mini-hardware-verification/  # Hardware Verification
├── mini-hls-compiler/           # High-Level Synthesis Compiler
├── mini-riscv-arch/             # RISC-V Processor Architecture
├── mini-noc-design/             # Network-on-Chip Design
├── mini-chiplet-design/         # Chiplet Design
└── mini-ai-accel-design/        # AI Accelerator Design
```

## License

MIT
