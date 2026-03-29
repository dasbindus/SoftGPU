# SoftGPU Bug 单跟踪

| # | 日期 | 描述 | 严重度 | 状态 | 负责人 |
|---|------|------|--------|------|--------|
| BUG-001 | 2026-03-30 | Scene005_MultiTriangle_PPMDumpCorrect: TBR/GMEM 同步问题，PPMDumpCorrect 读取不到红像素 | P1 | Fixed | 白小西 |
| BUG-002 | 2026-03-30 | Scene006_Warp_PPMDumpCorrect: 同 BUG-001，TBR/GMEM 同步问题 | P1 | Fixed | 白小西 |

---

## BUG-001: Scene005 MultiTriangle PPMDumpCorrect

**发现日期**: 2026-03-30
**发现者**: 王刚
**严重度**: P1（High）

**问题描述**:
- TBR 渲染路径开启时（默认 m_tbrEnabled=true），渲染数据走 GMEM
- PPMDumpCorrect 测试读取 framebuffer，但 GMEM→framebuffer 同步可能有问题
- 其他测试（如 BoundingBoxExact）直接读取 framebuffer 的 color buffer，通过 getColorBuffer()，不走 TBR，测试正常通过

**分析**:
- Scene005 的其他 5 个测试（RedBBoxExact、GreenBBoxExact、BlueBBoxExact、SlantedEdgeLinearity、GoldenReference）全部通过
- 只有 PPMDumpCorrect 失败（redCount=0，期望 >500）
- 独立验证找到 33,024 个红像素，说明渲染实际正确
- **严重度判断**：渲染本身正确，GMEM 数据完整，问题是 GMEM→framebuffer 同步缺失导致测试无法验证。有 workaround（直接读 GMEM），但不修复则 TBR 路径的 pixel dump 测试永远无法正常工作。归为 P1。

**状态**: Fixed (2026-03-30)
**修复人**: 白小西
**修复说明**: `dumpFrame()` 在 TBR 模式下直接读取 GMEM（tile 顺序），但 `dump()` 已经通过 `syncGMEMToFramebuffer()` 同步到 framebuffer（screen 顺序）。统一 `dumpFrame()` 也使用相同模式：先 `syncGMEMToFramebuffer()`，再从 framebuffer 读取。

**负责人**: 王刚

---

## BUG-002: Scene006 Warp PPMDumpCorrect

**发现日期**: 2026-03-30
**发现者**: 王刚
**严重度**: P1（High）

**问题描述**:
- 与 BUG-001 完全相同，TBR/GMEM 同步问题
- redCount=0（期望 >50000），nonRedRatio=1.0（期望 <0.1）
- 独立验证找到 248,832 个红像素，nonRedRatio=0.10，符合预期

**严重度判断**：与 BUG-001 完全一致，P1。两个 BUG 可能共享同一根因（TBR GMEM→framebuffer flush 缺失），建议一起修。

**状态**: Fixed (2026-03-30)
**修复人**: 白小西
**修复说明**: 同 BUG-001，`dumpFrame()` 已修复。

**负责人**: 王刚

---

## 历史已关闭 Bug

| # | 日期 | 描述 | 严重度 | 关闭日期 |
|---|------|------|--------|----------|
| BUG-OLD-1 | 2026-03-29 | GMEM Base 未接线导致 writeGMEM 崩溃 | Critical | 2026-03-29 |
| BUG-OLD-2 | 2026-03-29 | Rasterizer fill rule epsilon 第三分支逻辑错误 | High | 2026-03-29 |
| BUG-OLD-3 | 2026-03-29 | DIV 直接除法而非牛顿迭代 | High | 2026-03-29 |
| BUG-OLD-4 | 2026-03-29 | TokenBucket 带宽限制形同虚设 | High | 2026-03-29 |
| BUG-OLD-5 | 2026-03-29 | MAD/SEL Rc 字段编码错误 | High | 2026-03-30 |
| BUG-OLD-6 | 2026-03-29 | LD/ST 无内存边界检查 | High | 2026-03-30 |
| BUG-OLD-7 | 2026-03-29 | FragmentShader 缺少 ShaderCore 成员 | High | 2026-03-30 |
| BUG-OLD-8 | 2026-03-29 | 地址计算整数溢出风险 | High | 2026-03-30 |
| BUG-OLD-9 | 2026-03-29 | PPMVerifier getPixel() Y 坐标翻转 | Medium | 2026-03-30 |
| BUG-OLD-10 | 2026-03-29 | Depth test 比较方向错误 (z < oldZ 应为 z > oldZ) | Medium | 2026-03-30 |
