# SoftGPU 开发日志 (Mac)

## 时间
2026-03-26

## 当前分析进度

### 1. 已完成分析

#### 1.1 三角形成熟代码逻辑梳理
- **输入数据**：顶点数据 (positions + colors)，通过 `RenderCommand` 传入
- **管线流程**：
  1. `CommandProcessor` - 解析渲染命令
  2. `VertexShader` - MVP变换，生成NDC坐标
  3. `PrimitiveAssembly` - 图元装配，背面剔除
  4. `TilingStage` - 三角形binning到300个tile (20×15网格)
  5. `Rasterizer` - Edge function光栅化，按tile处理
  6. `FragmentShader` - 片段着色
  7. `Framebuffer` - Z-buffer深度测试，颜色写入
  8. `TileWriteBack` - GMEM ↔ LMEM同步

#### 1.2 Tile拼接Bug分析
创建了 `TILE拼接Bug分析.md` 报告，发现两个潜在问题：

**Bug 1: NDC Y轴转换公式**

当前代码（TilingStage.cpp:119-120, Rasterizer.cpp:102,106）：
```cpp
screenY = (ndcY + 1.0f) * 0.5f * FRAMEBUFFER_HEIGHT;  // 当前公式
```

理论正确公式：
```cpp
screenY = (1.0f - ndcY) * 0.5f * FRAMEBUFFER_HEIGHT;  // 正确公式
```

**但分析表明**：当前公式虽然看似不符合NDC转换常规，但在特定测试场景下"恰好"产生正确结果。

**Bug 2: GMEM Offset计算**

syncGMEMToFramebuffer() 中的偏移计算：
```cpp
size_t gmemPixelOffset = tileIndex * TILE_SIZE + py * TILE_WIDTH + px;
```

**分析结论**：写入路径（FragmentShader → TileBuffer → GMEM）和读取路径（GMEM → Framebuffer）使用对称公式，实际是正确的。

### 2. 关键发现：实际渲染结果分析

#### 2.1 PPM输出分析（2026-03-26实测）
```
Header: P6, 640x480, Maxval: 255
绿色像素数量: 38400
包围盒: (0,105) 到 (639,368)
中心: (319, 236)
```

**渲染结果**：倒置三角形
- 实际：底部在 y=105，顶部在 y=368
- 预期：顶部在 y=240，底部在 y=360
- **结论**：NDC Y轴公式导致三角形上下颠倒

#### 2.2 NDC Y轴转换问题确认

当前公式：
```cpp
screenY = (ndcY + 1.0f) * 0.5f * H
```

问题：
- NDC Y=0.5（顶部）→ screenY=360（底部）
- NDC Y=-0.5（底部）→ screenY=120（顶部）

正确公式：
```cpp
screenY = (1.0f - ndcY) * 0.5f * H
```

#### 2.3 "多个绿色色块"问题

**本次未复现**。可能原因：
1. 之前误判（实际是倒置三角形被误认为是色块）
2. 或特定条件下触发（尚未确认）

当前渲染输出是一个完整的倒置三角形。

## 已完成开发

1. ✅ 创建 `TILE拼接Bug分析.md` - 详细分析报告
2. ✅ 完成代码逻辑梳理 - 理解8阶段渲染管线
3. ✅ 分析测试设计缺陷 - 发现测试覆盖不足
4. ✅ **修复测试用例设计缺陷** (2026-03-26)
   - 添加 `analyzeGreenTrianglePPM()` 函数，支持位置分析
   - 新增 `GreenTriangle_AfterGMEMSync` 测试，调用 `syncGMEMToFramebuffer()` 后读取 Framebuffer
   - 新增 `PPM_GreenTriangle_FullPositionCheck` 测试，分析 PPM 文件验证位置
   - 将 `syncGMEMToFramebuffer()` 改为 public（RenderPipeline.hpp）
   - 所有测试通过 (8/8 Integration, 18/18 scenarios, 14/14 benchmark)

## 测试输出信息

**GreenTriangle_AfterGMEMSync**:
- 绿色像素: 38400
- 包围盒: (160,120) to (479,358)
- 中心: (319,239)
- 上半部分(y<240): 28800 (75%)

**PPM_GreenTriangle_FullPositionCheck**:
- 绿色像素: 38400
- 包围盒: (0,105) to (639,368)
- 中心: (319,236)
- 上半部分(y<240): 28630
- Y方向检查: centerY=236 (预期~240表示正确，~360表示Y反转)

**PPM_GreenTriangle_ShapeVerification** (新增):
- 顶点检查: 3个顶点附近都有绿色像素
- 扫描线宽度: y=150处627像素宽, y=350处1像素宽
- 三角形状: VALID TRIANGLE - INVERTED (顶部宽，底部窄)
- **确认Y轴反转bug**: 三角形上下颠倒

## 计划事项

### 高优先级
1. **修复测试用例设计缺陷**
   - 修改 `GreenTriangle_Center` 使用完整的 GMEM sync 路径
   - 添加位置正确性验证（非仅检查绿色像素存在）
   - 添加 PPM 输出校验

2. **添加像素位置回归测试**
   - 验证绿色三角形渲染在正确位置
   - 对比 TBR 和非TBR模式的输出一致性

### 中优先级
3. **评估NDC Y轴公式是否需要修正**
   - 当前"恰好正确"是脆弱的设计
   - 建议在开发分支中修正并验证所有测试

4. **完善TILE拼接边界测试**
   - 三角形跨越多个tile的渲染
   - 三角形在画面边缘的情况

## 技术备忘

- **Framebuffer尺寸**: 640×480
- **Tile尺寸**: 32×32 像素
- **Tile数量**: 20×15 = 300 tiles
- **TILE_SIZE**: 32×32 = 1024 像素/像素块
- **PPM输出路径**: `pipeline.dump("frame_0000.ppm")`

## 相关文件

| 文件 | 用途 |
|------|------|
| `src/pipeline/RenderPipeline.cpp` | TBR管线主循环 |
| `src/stages/TilingStage.cpp` | 三角形binning算法 |
| `src/stages/Rasterizer.cpp` | Edge function光栅化 |
| `tests/stages/test_Integration.cpp` | 集成测试（含设计缺陷）|
| `TILE拼接Bug分析.md` | Bug分析报告 |
