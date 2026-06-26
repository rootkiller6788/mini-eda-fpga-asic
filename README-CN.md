# Mini EDA FPGA ASIC（迷你电子设计自动化 FPGA 专用集成电路）

**从零开始、零依赖的 C 语言实现**，涵盖 EDA 工具、FPGA 开发、ASIC 设计流程和硬件描述语言。每个模块覆盖芯片设计全流程 — 从 RTL 设计与综合到布局布线、静态时序分析、形式化验证和 AI 加速器架构设计。

## 模块总览

| 模块 | 主题 | 参考标准 |
|--------|--------|----------------|
| [mini-hdl-lang](mini-hdl-lang/) | Verilog 仿真（Module/Assign/Always）、VHDL 仿真、SystemVerilog 仿真、Testbench、波形生成 | IEEE 1364, IEEE 1800 |
| [mini-eda-tools](mini-eda-tools/) | 综合（RTL→门级网表）、布局布线（P&R）、静态时序分析 STA（Setup/Hold）、功耗分析、可制造性设计 DFM | Synopsys DC, Cadence Innovus |
| [mini-fpga-dev](mini-fpga-dev/) | LUT/FF/BRAM/DSP 资源、Vivado/Quartus 流程、比特流生成、时序收敛、I/O 规划 | Xilinx Vivado, Intel Quartus |
| [mini-asic-flow](mini-asic-flow/) | RTL→GDSII 全流程、标准单元库、布图规划、时钟树综合 CTS、ECO 工程变更、DRC/LVS 物理验证、流片检查清单 | Cadence/Synopsys Flow |
| [mini-hardware-verification](mini-hardware-verification/) | UVM 方法学（Driver/Monitor/Scoreboard）、断言（SVA/PSL）、形式化验证（SymbiYosys）、覆盖率驱动 | IEEE 1800.2 UVM, SVA |
| [mini-hls-compiler](mini-hls-compiler/) | C→RTL 高层次综合（Vivado HLS）、循环展开/流水线、数据流优化、数组划分、接口 Pragma | Vivado HLS, Catapult HLS |
| [mini-riscv-arch](mini-riscv-arch/) | RISC-V RV32I 处理器核设计、五级流水线、数据/控制冒险处理、CSR 控制状态寄存器、特权级别 | RISC-V ISA Manual Vol 1-2 |
| [mini-noc-design](mini-noc-design/) | 片上网络 NoC：拓扑（Mesh/Torus）、XY 路由、虚通道 VC、虫洞交换、AXI 总线、延迟/吞吐量分析 | Dally "Principles of NoC" |
| [mini-chiplet-design](mini-chiplet-design/) | Chiplet 芯粒：UCIe/D2D 互连、Interposer 中介层、MCM 多芯片模块、HBM 集成、Die-to-Die PHY、热分析 | UCIe Spec, OCP ODSA |
| [mini-ai-accel-design](mini-ai-accel-design/) | DNN 加速器 RTL 设计、脉动阵列 RTL、PE 处理单元微架构、缓冲层次、专用指令集 ISA | Google TPU, Eyeriss, Simba |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **用户态 EDA 仿真** — 对芯片设计工具链、验证方法学和硬件架构的教学级建模
- **理论到代码的映射** — 每个模块包含 `docs/` 目录，内有标准/规范对齐说明
- **实用演示程序** — Verilog 仿真器、RISC-V 核心仿真器、STA 引擎、HLS 调度器、NoC 仿真器等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-hdl-lang
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-eda-fpga-asic/
├── mini-hdl-lang/               # HDL 硬件描述语言
├── mini-eda-tools/              # EDA 工具
├── mini-fpga-dev/               # FPGA 开发
├── mini-asic-flow/              # ASIC 设计流程
├── mini-hardware-verification/  # 硬件验证
├── mini-hls-compiler/           # 高层次综合编译器
├── mini-riscv-arch/             # RISC-V 处理器架构
├── mini-noc-design/             # 片上网络设计
├── mini-chiplet-design/         # Chiplet 芯粒设计
└── mini-ai-accel-design/        # AI 加速器设计
```

## 许可证

MIT
