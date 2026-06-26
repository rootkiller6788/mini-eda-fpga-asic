# mini-fpga-dev — FPGA开发 (C 语言实现)

纯 C99 实现的 FPGA 开发工具链核心模型。涵盖 FPGA 架构建模、Vivado 设计流程、
比特流生成、时序收敛和 I/O 规划等关键环节。

## 模块

| 文件 | 内容 |
|------|------|
| `fpga_arch.h/c` | FPGA 底层架构：LUT、Flip-Flop、BRAM、DSP、布线资源 |
| `vivado_flow.h/c` | Vivado 设计流程：综合→实现→比特流、XDC 约束 |
| `bitstream_gen.h/c` | 比特流生成：配置帧、LUT 掩码、压缩、部分重配置 |
| `timing_closure.h/c` | 时序收敛：建立/保持时间、流水线、重定时、逻辑复制 |
| `io_planning.h/c` | I/O 规划：引脚分配、Bank 电压、I/O 标准、SSN 分析 |

## 构建

```sh
make all
make test
make clean
```

## 要求

- C99 编译器 (gcc/clang)
- Make
