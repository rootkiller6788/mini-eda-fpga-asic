# mini-eda-tools — EDA 工具链 (C 实现)

> 参考 Stanford EE272 (EDA Tools), MIT 6.371 (VLSI Design), CMU 18-760 (Digital Design Automation)

## 模块-课程对应表 (Module-Course Mapping)

| 本模块组件 | Stanford EE272 | MIT 6.371 | CMU 18-760 |
|-----------|---------------|-----------|------------|
| `logic_synth.h/c` | L3-L4: Logic Synthesis | Ch4: Boolean Optimization | L5-L8: Synthesis |
| `tech_map.h/c` | L5-L6: Technology Mapping | Ch5: Cell Libraries | L9-L11: Tech Map |
| `place_route.h/c` | L7-L9: Physical Design | Ch6: Place & Route | L12-L16: P&R |
| `static_timing.h/c` | L10-L12: Timing Analysis | Ch7: Timing Closure | L17-L19: STA |
| `eda_flow.h/c` | L13-L15: Design Flow | Ch8: Full Flow | L20-L22: Integration |

## 核心能力 (Core Capabilities)

| 能力 | API 示例 |
|------|---------|
| 逻辑网络构建与优化 | `network_add_gate(&net, GATE_AND, inputs, 2); network_optimize(&net);` |
| Quine-McCluskey 逻辑简化 | `synth_quine_mccluskey(&mt, &result);` |
| 标准单元库与工艺映射 | `cell_lib_add(&lib, "NAND2", ...); techmap_match(&design);` |
| 模拟退火布局 | `place_simulated_annealing(&p, 1000.0, 0.95, 1000);` |
| Lee 迷宫布线 | `route_maze(&g, sx, sy, ex, ey, net_id);` |
| 静态时序分析 | `sta_compute_arrival(&g); sta_compute_slack(&g);` |
| 完整 EDA 流程 | `flow_run_all(&flow); flow_print_report(&flow);` |

## 编译与运行

```bash
make          # 编译所有示例
make test     # 运行所有测试
make clean    # 清理
```

## 依赖

- C99 compiler (gcc/clang)
- libc + libm only
