# ASIC 物理设计流程

## 概述

标准ASIC设计流程从RTL到GDSII，涵盖10个主要阶段。每个阶段都有设计里程碑和检查点。

## 流程阶段

| 阶段 | 名称 | 输入 | 输出 | 关键工具 |
|------|------|------|------|----------|
| 1 | RTL Design | 规格 | RTL (Verilog/VHDL) | 仿真器 |
| 2 | Synthesis | RTL + 约束 | 门级网表 | Design Compiler / Genus |
| 3 | DFT | 网表 | DFT插入网表 | Tessent / DFT Compiler |
| 4 | Floorplan | 网表 + .lib/.lef | 布图规划 | Innovus / ICC2 |
| 5 | Placement | Floorplan + 网表 | 布局后网表 | Innovus / ICC2 |
| 6 | CTS | 布局后网表 | 时钟树网表 | Innovus / ICC2 |
| 7 | Routing | CTS网表 | 布线后网表 | Innovus / ICC2 |
| 8 | RC Extraction | 布线后网表 | SPEF文件 | StarRC / QRC |
| 9 | STA | SPEF + 网表 | 时序报告 | PrimeTime / Tempus |
| 10 | SignOff | 所有签核数据 | GDSII | Calibre / ICV |

## 设计里程碑

每个阶段完成后记录以下里程碑数据：
- 单元数和网线数
- 芯片面积 (um²)
- 功耗 (mW)
- 时钟周期 (ns)
- Setup/Hold slack (ns)
- DRC/LVS 错误数

## 检查点

检查点用于跟踪每个阶段内的项目进展：
- **PASS**: 指标达标
- **FAIL**: 指标未达标，需要返工
- **WARN**: 边缘值，需要关注
- **SKIP**: 跳过此检查点

## 签核标准

最终签核需要满足：
- Setup/Hold 时序无违规
- DRC 错误为零
- LVS 匹配通过
- 功耗在预算内
- 面积满足要求
- IR Drop < 目标值 (通常 < 5% VDD)
