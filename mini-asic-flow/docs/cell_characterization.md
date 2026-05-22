# 标准单元特化

## Liberty格式 (.lib)

标准单元特化的核心输出是Liberty格式文件，包含：
- **时序弧 (Timing Arcs)**: 每个输入到输出路径的延迟模型
- **功耗信息**: 内部功耗、漏电功耗、开关功耗
- **引脚电容**: 输入引脚负载电容
- **输出驱动**: 输出引脚驱动能力

## 时序弧类型

| 类型 | 描述 | 应用 |
|------|------|------|
| cell_rise / cell_fall | 组合逻辑单元延迟 | 所有组合单元 |
| rise_rise / rise_fall | 时序弧延迟 | 反相器、复杂门 |
| setup_rise / setup_fall | 建立时间约束 | 触发器 |
| hold_rise / hold_fall | 保持时间约束 | 触发器 |

## 阈值电压 (VT)

| VT类型 | 切换速度 | 漏电功耗 | 适用场景 |
|--------|----------|----------|----------|
| LVT | 最快 | 最高 | 关键路径 |
| SVT | 中等 | 中等 | 一般路径 |
| HVT | 最慢 | 最低 | 非关键路径 |
| ULVT | 极快 | 极高 | 极端高速路径 |

## FinFET 模型 (7nm/5nm)

### 关键参数

| 参数 | 7nm | 5nm |
|------|-----|-----|
| Fin Pitch | 30 nm | 27 nm |
| Fin Height | 32 nm | 30 nm |
| Fin Width | 6-8 nm | 5-7 nm |
| Gate Length | 7 nm | 5 nm |
| CPP | 84 nm | 60 nm |
| M1 Pitch | 40 nm | 28 nm |
| Tracks | 9T | 6T |

### 器件模型

FinFET驱动电流近似公式：
```
Idsat ≈ μ * Cox * (W/L) * (Vgs - Vth)² / 2
```

其中 μ 是载流子迁移率，Cox 是栅氧化层单位电容，W 是有效沟道宽度。

### Track Height

单元高度用"Tracks"表示，每个track约等于M2节距：
- 9T: ~216-240 nm (常见于7nm)
- 7.5T: ~180 nm
- 6T: ~150 nm (常见于5nm)

## LUT延迟模型

NLDM (Non-Linear Delay Model) 使用二维查找表：
- 第一维度：输入转换时间 (input slew)
- 第二维度：输出负载电容 (output load)
- 输出：单元延迟 / 输出转换时间
