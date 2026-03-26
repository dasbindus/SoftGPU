# Bug 分析报告

## 问题描述

运行 headless 模式渲染绿色三角形时，输出图像显示三角形**上下颠倒**。

## 根因分析

### Bug 1: NDC Y 轴转换公式错误

**问题代码** (TilingStage.cpp, Rasterizer.cpp):

```cpp
screenY = (ndcY + 1.0f) * 0.5f * FRAMEBUFFER_HEIGHT;  // 错误
```

**正确公式**:

```cpp
screenY = (1.0f - ndcY) * 0.5f * FRAMEBUFFER_HEIGHT;  // 正确
```

**影响**:
- NDC Y=-1 (底部) → 错误: screenY=0, 正确: screenY=H
- NDC Y=+1 (顶部) → 错误: screenY=H, 正确: screenY=0

导致三角形**上下颠倒**。

### Bug 2: TilingStage computeBbox 中 minY/maxY 赋值问题

当修正 Y 轴公式后，screenMinY 和 screenMaxY 的赋值需要**交换**：

**错误代码**:

```cpp
float screenMinY = (1.0f - minY) * 0.5f * H;
float screenMaxY = (1.0f - maxY) * 0.5f * H;
```

**正确代码**:

```cpp
float screenMinY = (1.0f - maxY) * 0.5f * H;  // NDC maxY (top) → screen top (small Y)
float screenMaxY = (1.0f - minY) * 0.5f * H;  // NDC minY (bottom) → screen bottom (large Y)
```

**原因**: Y 轴反转后，NDC 的 top (maxY) 对应 screen 的顶部（小 Y 值），NDC 的 bottom (minY) 对应 screen 的底部（大 Y 值）。

### Bug 3: RenderPipeline::dump() 未同步 GMEM

**问题代码**:

```cpp
void RenderPipeline::dump(const std::string& filename) const {
    if (m_tbrEnabled) {
        // 直接 dump GMEM，没有先同步到 framebuffer
        const float* colorData = m_tileWriteBack.getGMEMColor();
        m_dumper.dumpPPM(colorData, ...);
    }
    // ...
}
```

**正确代码**:

```cpp
void RenderPipeline::dump(const std::string& filename) const {
    if (m_tbrEnabled) {
        // 先同步 GMEM 到 framebuffer
        const_cast<RenderPipeline*>(this)->syncGMEMToFramebuffer();
        const float* colorData = m_framebuffer.getColorBuffer();
        m_dumper.dumpPPM(colorData, ...);
    }
    // ...
}
```

**原因**: TBR 模式下，渲染结果存储在 GMEM 中。需要先同步到 framebuffer 才能得到正确的屏幕空间图像。

## 修复内容

### 修复的文件

| 文件 | 修改内容 |
|------|----------|
| `src/stages/TilingStage.cpp` | Y 轴转换公式改为 `(1.0f - ndcY) * 0.5f * H`，并交换 minY/maxY 赋值 |
| `src/stages/Rasterizer.cpp` | Y 轴转换公式改为 `(1.0f - ndcY) * 0.5f * H` |
| `src/pipeline/RenderPipeline.cpp` | dump() 先调用 syncGMEMToFramebuffer() |

### 修复后的输出验证

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 绿色像素数 | 38400 | 38400 |
| 包围盒 | (0,105)-(639,368) | (160,121)-(479,359) |
| 中心 | (319,236) | (319,240) |
| 扫描线行数 | 264 (但176行空) | 239 (全部有像素) |
| 三角形朝向 | 倒置 | 正确 |

## 测试结果

所有测试通过:
- `test_Integration`: 11/11 测试通过
- `test_test_scenarios`: 18/18 测试通过
- `test_benchmark_runner`: 14/14 测试通过

## 关键代码位置

| 文件 | 函数 | 行号 | 说明 |
|------|------|------|------|
| TilingStage.cpp | computeBbox | 116-127 | Y轴转换和minY/maxY交换 |
| TilingStage.cpp | ndcToTile | 131-137 | Y轴转换 |
| Rasterizer.cpp | rasterizeTrianglePerTile | 100-106 | Y轴转换 |
| Rasterizer.cpp | interpolateAttributes | 185-191 | Y轴转换 |
| RenderPipeline.cpp | dump | 325-336 | 添加syncGMEMToFramebuffer调用 |

## 经验教训

1. **NDC Y 轴转换必须正确**: 这是 GPU 渲染的基础公式，错误会影响整个渲染管线
2. **Y 轴反转后 min/max 交换**: 当改变坐标转换公式时，必须同时考虑 min/max 的语义变化
3. **GMEM/framebuffer 同步**: TBR 模式下，dump 图像前必须先同步 GMEM 到 framebuffer
4. **测试覆盖的重要性**: 原有的测试只检查"有绿色像素"，没有验证位置正确性。新增的测试发现并验证了修复的正确性
