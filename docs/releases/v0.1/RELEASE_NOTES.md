# SoftGPU v0.1 Release Notes

**版本：** v0.1  
**日期：** 2026-03-26  
**状态：** Released ✅

---

## 概览

SoftGPU 是一个软件 Tile-Based GPU 模拟器，包含性能分析和可视化功能。

---

## 主要功能

### PHASE1-5 完成清单

| Phase | 功能 | 状态 |
|-------|------|------|
| P1 | 最小可运行GPU（7 Stage管线）| ✅ |
| P2 | TBR核心（Tiling + TileBuffer + LMEM）| ✅ |
| P3 | GMEM带宽模型 + L2 Cache | ✅ |
| P4 | 测试场景集 + Benchmark自动化 + PPM测试 | ✅ |
| P5 | Profiler + 瓶颈判定 + ImGui可视化 | ✅ |

---

## 技术规格

| 指标 | 数值 |
|------|------|
| 代码量 | ~5,430行 |
| Git commits | 13+ |
| 测试覆盖 | 30+ tests |
| P0/Critical bug | 0 |
| 代码评分 | 4.2/5 |

---

## 架构

```
RenderPipeline (8 Stage)
├── CommandProcessor
├── VertexShader
├── PrimitiveAssembly
├── TilingStage (Binning)
├── Rasterizer
├── FragmentShader
├── Framebuffer (Z-buffer)
└── TileWriteBack

支持模块：
├── MemorySubsystem (令牌桶+L2 Cache)
├── FrameProfiler (性能采集)
├── BottleneckDetector (瓶颈判定)
└── ProfilerUI (ImGui可视化)
```

---

## 测试场景

- Triangle-1Tri (1三角形)
- Triangle-Cube (12三角形)
- Triangle-Cubes-100 (1200三角形)
- Triangle-SponzaStyle (~80三角形)
- PBR-Material (~180三角形)

---

## 已知问题（待下版本修复）

| 严重度 | 问题 | 计划 |
|--------|------|------|
| 🔴 | Benchmark阶段时间硬编码估算 | v0.2 |
| 🔴 | TileBuffer raw loop拷贝 | v0.2 |
| 🔴 | render() 180行过长 | v0.2 |
| 🟡 | 代码重复（双输入源模式）| v0.2 |
| 🟡 | 魔法数字无命名 | v0.2 |

完整坏味道报告：`docs/CODE_SMELL_REPORT.md`

---

## 团队

| 角色 | 成员 |
|------|------|
| 协调 | 白小东 |
| 架构 | 陈二虎 |
| 开发 | 白小西 |
| 审查 | 王刚 |

---

## 下一步

- v0.2: 基于代码坏味道的重构计划（详见 `docs/REFACTORING_PLAN.md`）

---

_Release by 白小东_
