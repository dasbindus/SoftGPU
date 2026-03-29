# SoftGPU v0.2 Release Notes

**版本：** v0.2  
**日期：** 2026-03-26  
**状态：** Released ✅

---

## 概览

v0.2 是基于 v0.1 的代码质量重构版本，修复了 5 个🔴严重问题，提升了可维护性和运行时性能。

---

## 重构内容

| # | 重构 | 修复内容 |
|---|------|---------|
| R-P0-1 | Benchmark 真实计时 | 删除硬编码估算（×0.6/0.15/0.1），改用 FrameProfiler 真实数据 |
| R-P0-2 | memcpy 优化 | TileBuffer/TileWriteBack 逐像素循环改为 std::memcpy |
| R-P0-3 | render() 拆解 | 180行函数拆解为 executeCommonStages/renderTBRMode/renderPhase1Mode 等 |
| R-P0-4 | null check | FragmentShader::setTileBufferManager 添加 nullptr 校验 |
| R-P0-5 | 带宽模型 | MemorySubsystem tryConsume() 返回值正确处理 |

---

## 测试结果

**34/34 tests PASS** ✅

| 测试套件 | 结果 |
|---------|------|
| Framebuffer | ✅ 7/7 |
| PrimitiveAssembly | ✅ |
| Rasterizer | ✅ |
| VertexShader | ✅ |
| BenchmarkRunner | ✅ 14/14 |

---

## Git Tag

```
v0.1 - Initial Release
v0.2 - Refactoring complete
```

---

## 下一步

v0.3 可选方向：
- R-P1 中等问题修复（双输入源模式提取、魔法数字常量）
- R-P2 建议项（命名优化、注释补充）
- 新功能开发

---

_Release by 白小东_
