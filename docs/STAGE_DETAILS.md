# 管线阶段详细说明

本文档包含 SoftGPU 8 级渲染管线的详细技术说明和差距分析。

---

## 各阶段详情

| 阶段 | 状态 | 已实现 | 待实现 |
|------|------|--------|--------|
| **CommandProcessor** | ⚠️ | VB/IB 复制、uniform 设置 | 预取队列、并行解码 |
| **VertexShader** | ⚠️ | MVP 变换、**ISA 执行模式** | SIMD 矢量单元 |
| **PrimitiveAssembly** | ⚠️ | 索引/非索引三角形装配、透视除法、**AABB 视锥剔除** | **背面剔除**、**完整裁剪** |
| **TilingStage** | ⚠️ | 三角形 binning (300 tiles) | **深度排序** |
| **Rasterizer** | ⚠️ | 边缘函数 DDA、viewport 裁剪 | **MSAA 多样品采样** |
| **FragmentShader** | ⚠️ | ISA v2.5 解释器、50+ 指令、Warp 调度、PNG 纹理采样（NEAREST） | **Bilinear/mipmap 滤波** |
| **EarlyZ** | ✅ | 管线已集成，FragmentShader 前过滤 | 无 |
| **Framebuffer** | ⚠️ | Z-buffer、颜色缓冲 | **Stencil**、**Blend/Alpha** |
| **TileWriteBack** | ⚠️ | GMEM ↔ LMEM 同步 | **压缩回写** |

---

## 完整差距分析报告

### CommandProcessor (50%)

- ✅ 已实现: VB/IB 数据复制、uniform 设置 (CommandProcessor.cpp L19-L52)
- ❌ 待实现: 预取队列、并行解码
- ⚠️ 说明: viewport 处理在 Rasterizer 中，CommandProcessor 仅做数据搬运

### VertexShader (40% ISA)

- ✅ 已实现: MVP 变换、齐次除法、裁剪
- ✅ 已实现: **ISA 执行模式**（executeISA 使用 Interpreter，SetProgram 加载 VS 程序）
- ⚠️ 待实现: SIMD 矢量单元
- ⚠️ 说明: 有两种执行模式 CPP (C++ 参考实现) 和 ISA (Interpreter)，Auto 模式自动选择

### PrimitiveAssembly (60%)

- ✅ 已实现: 索引/非索引三角形装配、透视除法(clip→NDC)、**AABB 视锥剔除** (PrimitiveAssembly.cpp L106-L124)
- ❌ 待实现: 背面剔除、完整裁剪

| 待实现功能 | 优先级 | 性能/正确性影响 |
|-----------|--------|-----------------|
| 背面剔除 | P0 | 性能 +50%（消除背面带宽浪费）|
| 完整裁剪（近平面）| P0 | 正确性（部分在视锥内的三角形）|
| triangle_strip 支持 | P1 | 带宽节省 |
| 视口变换（移至 PA）| P1 | 架构优化 |
| primitive restart | P2 | 便利性 |

### TilingStage (90%)

- ✅ 已实现: 三角形 binning 到 300 tiles (20×15)
- ❌ 待实现: **深度排序**（Painter's algorithm 用于透明物体排序）

### Rasterizer (70%)

- ✅ 已实现: 边缘函数 DDA、亚像素精度、perspectively correct 插值、viewport 裁剪 (Rasterizer.cpp L124-L129)
- ❌ 待实现: **MSAA 2×/4×** 多样品抗锯齿

### FragmentShader (90%)

- ✅ 已实现: ISA v2.5 解释器、50+ 指令、4 种着色器类型、Warp 调度
- ✅ 已实现: PNG 纹理加载（NEAREST 采样），可通过 `--texture` 参数使用 (ShaderCore.cpp, TextureBuffer.cpp)
- ⚠️ 待实现: **Bilinear/mipmap 滤波**

### EarlyZ (100% ✅)

- ✅ 已实现: EarlyZ::filterOccluded() 已集成到管线中（FragmentShader 前）
- ✅ 已验证: test_Integration.cpp 中 16 个 EarlyZ 单元测试全部通过

### Framebuffer (60%)

- ✅ 已实现: Z-buffer 深度测试、颜色缓冲写入
- ❌ 待实现: **Stencil buffer**、**Blend/Alpha 混合**

### TileWriteBack (80%)

- ✅ 已实现: GMEM ↔ LMEM 同步、回写逻辑
- ❌ 待实现: **压缩回写**（DXT/BC 格式）
