# PHASE1 Design Document — SoftGPU 软件 Tile-Based GPU 模拟器

**项目名称：** SoftGPU Pixel Workshop  
**阶段：** PHASE1 — 最小可运行 GPU，渲染三角形  
**架构师：** 艾拉（@ayla）  
**日期：** 2026-03-26  
**版本：** v1.0

---

## 1. 阶段目标

### 1.1 核心目标

实现一个最小可运行的软件 GPU 模拟器，能够：
- 接收 DrawCall 命令（VB / IB / 渲染参数）
- 执行完整的渲染管线（VS → PA → Rasterizer → FS → Framebuffer）
- 输出正确的三角形渲染结果到 CPU 端 Framebuffer

### 1.2 功能范围

| 模块 | 功能 | 边界 |
|------|------|------|
| CommandProcessor | 解析 DrawCall，提取 VB/IB | 仅支持三角形列表（Triangle List） |
| VertexShader | MVP 变换，并行处理顶点 | MVP 矩阵外部传入，Phase1 固定功能 |
| PrimitiveAssembly | 组装三角形 + AABB 视锥剔除 | 简单 bounding box 剔除 |
| Rasterizer | DDA 光栅化 | 输出 Edge Function fragment 列表 |
| FragmentShader | Flat color 或纹理采样 | Phase1 仅支持 flat color |
| Framebuffer | Color buffer + Depth buffer | 640×480 float，Z-test |
| TileWriteBack | Tile 写回 GMEM | 模拟，无实际 GMEM |

### 1.3 技术约束

- **语言：** C++17
- **构建：** CMake 3.15+
- **依赖：** 仅标准库 + GLSL（可选）
- **线程：** 单线程顺序执行（Phase2 再引入 MTL）
- **Framebuffer：** CPU 端 640×480 RGBA float

---

## 2. 渲染管线架构

### 2.1 管线流程图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              Render Pipeline                                  │
│                                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌────────────┐  │
│  │   Host CPU   │───▶│  Command     │───▶│  Vertex      │───▶│ Primitive  │  │
│  │  (App/Game)  │    │  Processor   │    │  Shader      │    │  Assembly  │  │
│  └──────────────┘    └──────────────┘    └──────────────┘    └────────────┘  │
│                                                                      │        │
│                                                                      ▼        │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌────────────┐  │
│  │   GMEM       │◀───│   Tile       │◀───│   Fragment   │◀───│ Rasterizer │  │
│  │  (Simulated) │    │  WriteBack   │    │   Shader     │    │            │  │
│  └──────────────┘    └──────────────┘    └──────────────┘    └────────────┘  │
│                                            ▲                                   │
│                                            │                                   │
│  ┌──────────────┐                         │                                   │
│  │   Depth      │─────────────────────────┘                                   │
│  │   Test       │                                                               │
│  └──────────────┘                                                               │
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐     │
│  │                        Framebuffer (640×480 RGBA float)               │     │
│  └──────────────────────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Stage 数据流

```
DrawCommand
    │
    ▼
CommandProcessor
    │  VertexBuffer + IndexBuffer + Uniforms
    ▼
VertexShader Stage  [计时开始]
    │  Transformed vertices (clip space)
    ▼
PrimitiveAssembly
    │  Assembled triangles + culling
    ▼
Rasterizer  [计时开始]
    │  Fragment list
    ▼
FragmentShader  [计时开始]
    │  Shaded fragments
    ▼
DepthTest
    │  Passed fragments
    ▼
TileWriteBack  [计时开始]
    │  Written to GMEM
    ▼
Present (Frame done)
```

### 2.3 执行顺序说明

Phase1 为**单线程顺序执行**，每个 Stage 按以下顺序执行：
1. CommandProcessor 解析命令
2. VertexShader 处理所有顶点（输出 clip-space vertices）
3. PrimitiveAssembly 组装三角形 + 剔除
4. Rasterizer 光栅化（输出覆盖的 fragment 列表）
5. FragmentShader 逐 fragment 执行
6. DepthTest + Framebuffer Write
7. TileWriteBack 写回 GMEM

---

## 3. 模块接口定义

### 3.1 抽象基类：IStage

所有 Stage 继承自 `IStage`，提供统一接口：

```cpp
// shared/include/SoftGPU/stages/IStage.hpp
#pragma once
#include <cstdint>
#include <string>

namespace SoftGPU {

struct PerformanceCounters {
    uint64_t cycle_count = 0;      // 周期计数
    uint64_t invocation_count = 0; // 调用次数
    double   elapsed_ms = 0.0;     // 耗时(ms)
};

class IStage {
public:
    virtual ~IStage() = default;
    
    // 阶段名称
    virtual const char* getName() const = 0;
    
    // 执行阶段（子类实现）
    virtual void execute() = 0;
    
    // 获取性能计数器
    virtual const PerformanceCounters& getCounters() const = 0;
    
    // 重置计数器
    virtual void resetCounters() = 0;
};

} // namespace SoftGPU
```

### 3.2 CommandProcessor

**职责：** 接收 DrawCall 参数，解析 VB / IB，准备渲染数据。

```cpp
// shared/include/SoftGPU/stages/CommandProcessor.hpp
#pragma once
#include "IStage.hpp"
#include "RenderCommand.hpp"
#include <memory>
#include <vector>

namespace SoftGPU {

class VertexShader;
class IRenderBackend;

class CommandProcessor : public IStage {
public:
    CommandProcessor(IRenderBackend* backend);
    
    // 设置当前命令（由 App 层调用）
    void setCommand(const RenderCommand& cmd);
    
    // IStage 实现
    const char* getName() const override { return "CommandProcessor"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 供后续 Stage 获取数据
    const std::vector<float>& getVertexBuffer() const;
    const std::vector<uint32_t>& getIndexBuffer() const;
    const DrawParams& getDrawParams() const;

private:
    IRenderBackend* m_backend;
    RenderCommand m_currentCommand;
    PerformanceCounters m_counters;
    
    std::vector<float> m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    DrawParams m_drawParams;
};

} // namespace SoftGPU
```

### 3.3 VertexShader

**职责：** MVP 变换，处理顶点。

```cpp
// shared/include/SoftGPU/stages/VertexShader.hpp
#pragma once
#include "IStage.hpp"
#include <vector>
#include <array>

namespace SoftGPU {

struct Vertex {
    float x, y, z, w;   // Position (clip space after transform)
    float r, g, b, a;   // Color
};

struct Uniforms {
    std::array<float, 16> modelMatrix;      // 4x4 column-major
    std::array<float, 16> viewMatrix;
    std::array<float, 16> projectionMatrix;
    float viewportWidth = 640.0f;
    float viewportHeight = 480.0f;
};

class VertexShader : public IStage {
public:
    VertexShader();
    
    // 设置输入数据（来自 CommandProcessor）
    void setInput(const std::vector<float>& vb,
                  const std::vector<uint32_t>& ib,
                  const Uniforms& uniforms);
    
    // 设置要处理的顶点数量
    void setVertexCount(size_t count);
    
    // IStage 实现
    const char* getName() const override { return "VertexShader"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 供后续 Stage 获取变换后的顶点
    const std::vector<Vertex>& getOutput() const;

private:
    std::vector<float> m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    Uniforms m_uniforms;
    size_t m_vertexCount = 0;
    
    std::vector<Vertex> m_outputVertices;
    PerformanceCounters m_counters;
    
    // 内部：执行 MVP 变换
    Vertex transformVertex(const float* rawVertex) const;
    Vertex applyMVP(const Vertex& v, const Uniforms& u) const;
};

} // namespace SoftGPU
```

### 3.4 PrimitiveAssembly

**职责：** 组装三角形，执行 AABB 视锥剔除。

```cpp
// shared/include/SoftGPU/stages/PrimitiveAssembly.hpp
#pragma once
#include "IStage.hpp"
#include "VertexShader.hpp"
#include <vector>

namespace SoftGPU {

struct Triangle {
    Vertex v[3];  // 三个顶点（NDC space）
    bool culled = false;
};

class PrimitiveAssembly : public IStage {
public:
    PrimitiveAssembly();
    
    // 设置输入（来自 VertexShader 输出）
    void setInput(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices);
    
    // IStage 实现
    const char* getName() const override { return "PrimitiveAssembly"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 供后续 Stage 获取三角形列表
    const std::vector<Triangle>& getOutput() const;

private:
    std::vector<Vertex> m_inputVertices;
    std::vector<uint32_t> m_inputIndices;
    std::vector<Triangle> m_outputTriangles;
    PerformanceCounters m_counters;
    
    // 内部：视锥剔除（AABB）
    bool shouldCull(const Triangle& tri) const;
    
    // 内部：计算 AABB
    void computeAABB(const Triangle& tri, float& minX, float& maxX,
                     float& minY, float& maxY, float& minZ, float& maxZ) const;
};

} // namespace SoftGPU
```

### 3.5 Rasterizer

**职责：** DDA 光栅化，输出 fragment 列表。

```cpp
// shared/include/SoftGPU/stages/Rasterizer.hpp
#pragma once
#include "IStage.hpp"
#include "PrimitiveAssembly.hpp"
#include <vector>

namespace SoftGPU {

struct Fragment {
    uint32_t x, y;       // 屏幕坐标（像素）
    float z;             // 深度值
    float r, g, b, a;    // 插值颜色
    float u, v;         // 纹理坐标（Phase1 可能不用）
};

class Rasterizer : public IStage {
public:
    Rasterizer();
    
    // 设置视口尺寸
    void setViewport(uint32_t width, uint32_t height);
    
    // 设置输入三角形（来自 PrimitiveAssembly）
    void setInput(const std::vector<Triangle>& triangles);
    
    // IStage 实现
    const char* getName() const override { return "Rasterizer"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 供后续 Stage 获取 fragment 列表
    const std::vector<Fragment>& getOutput() const;

private:
    uint32_t m_viewportWidth = 640;
    uint32_t m_viewportHeight = 480;
    std::vector<Triangle> m_inputTriangles;
    std::vector<Fragment> m_outputFragments;
    PerformanceCounters m_counters;
    
    // 内部：光栅化单个三角形（Edge Function DDA）
    void rasterizeTriangle(const Triangle& tri);
    
    // 内部：计算 edge function
    float edgeFunction(const Vertex& a, const Vertex& b, const Vertex& c) const;
    
    // 内部：重心坐标插值
    void interpolateAttributes(const Triangle& tri, float baryX, float baryY, Fragment& frag) const;
};

} // namespace SoftGPU
```

### 3.6 FragmentShader

**职责：** 逐 fragment 执行着色（Phase1：flat color）。

```cpp
// shared/include/SoftGPU/stages/FragmentShader.hpp
#pragma once
#include "IStage.hpp"
#include "Rasterizer.hpp"
#include <vector>

namespace SoftGPU {

class FragmentShader : public IStage {
public:
    FragmentShader();
    
    // 设置输入 fragments
    void setInput(const std::vector<Fragment>& fragments);
    
    // IStage 实现
    const char* getName() const override { return "FragmentShader"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 供后续 Stage 获取着色后的 fragments
    const std::vector<Fragment>& getOutput() const;

private:
    std::vector<Fragment> m_inputFragments;
    std::vector<Fragment> m_outputFragments;
    PerformanceCounters m_counters;
    
    // 内部：逐 fragment 着色（Phase1 直接输出输入颜色）
    Fragment shade(const Fragment& input) const;
};

} // namespace SoftGPU
```

### 3.7 Framebuffer

**职责：** 管理 color buffer 和 depth buffer，执行 Z-test。

```cpp
// shared/include/SoftGPU/stages/Framebuffer.hpp
#pragma once
#include "IStage.hpp"
#include "FragmentShader.hpp"
#include <vector>
#include <cstring>

namespace SoftGPU {

class Framebuffer : public IStage {
public:
    static constexpr uint32_t WIDTH = 640;
    static constexpr uint32_t HEIGHT = 480;
    static constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;
    
    Framebuffer();
    ~Framebuffer();
    
    // 设置输入（来自 FragmentShader）
    void setInput(const std::vector<Fragment>& fragments);
    
    // 清空 framebuffer
    void clear(const float* clearColor = nullptr);
    
    // IStage 实现
    const char* getName() const override { return "Framebuffer"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 获取 framebuffer 指针（供渲染/导出）
    const float* getColorBuffer() const { return m_colorBuffer.data(); }
    const float* getDepthBuffer() const { return m_depthBuffer.data(); }

private:
    std::vector<Fragment> m_inputFragments;
    std::vector<float> m_colorBuffer;   // RGBA float, WIDTH*HEIGHT*4
    std::vector<float> m_depthBuffer;   // Depth float, WIDTH*HEIGHT
    PerformanceCounters m_counters;
    
    // 内部：Z-test
    bool depthTest(uint32_t x, uint32_t y, float z) const;
    
    // 内部：写入 pixel
    void writePixel(uint32_t x, uint32_t y, float z, const float color[4]);
};

} // namespace SoftGPU
```

### 3.8 TileWriteBack

**职责：** 将完成渲染的 tile 写回模拟的 GMEM。

```cpp
// shared/include/SoftGPU/stages/TileWriteBack.hpp
#pragma once
#include "IStage.hpp"
#include "Framebuffer.hpp"
#include <cstdint>

namespace SoftGPU {

// Tile 配置（Phase1 固定）
static constexpr uint32_t TILE_WIDTH = 32;
static constexpr uint32_t TILE_HEIGHT = 32;

class TileWriteBack : public IStage {
public:
    TileWriteBack();
    
    // 注入 framebuffer 引用
    void setFramebuffer(Framebuffer* fb);
    
    // IStage 实现
    const char* getName() const override { return "TileWriteBack"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;
    
    // 模拟 GMEM 访问接口
    void writeTileToGMEM(uint32_t tileX, uint32_t tileY);
    void readTileFromGMEM(uint32_t tileX, uint32_t tileY, float* outColor, float* outDepth);

private:
    Framebuffer* m_framebuffer;
    PerformanceCounters m_counters;
    
    // 模拟 GMEM（CPU 端）
    std::vector<float> m_gmemColor;  // Tiles in GMEM layout
    std::vector<float> m_gmemDepth;
    
    // 内部：计算 tile 在 GMEM 中的偏移
    size_t getTileOffset(uint32_t tileX, uint32_t tileY) const;
};

} // namespace SoftGPU
```

### 3.9 RenderPipeline（管线组装器）

```cpp
// shared/include/SoftGPU/pipeline/RenderPipeline.hpp
#pragma once
#include "CommandProcessor.hpp"
#include "VertexShader.hpp"
#include "PrimitiveAssembly.hpp"
#include "Rasterizer.hpp"
#include "FragmentShader.hpp"
#include "Framebuffer.hpp"
#include "TileWriteBack.hpp"
#include "RenderCommand.hpp"

namespace SoftGPU {

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();
    
    // 执行一次完整的渲染
    void render(const RenderCommand& command);
    
    // 获取当前 framebuffer（用于显示/导出）
    const Framebuffer* getFramebuffer() const { return &m_framebuffer; }
    
    // 获取所有阶段的性能数据
    void printPerformanceReport() const;

private:
    CommandProcessor m_commandProcessor;
    VertexShader     m_vertexShader;
    PrimitiveAssembly m_primitiveAssembly;
    Rasterizer       m_rasterizer;
    FragmentShader   m_fragmentShader;
    Framebuffer      m_framebuffer;
    TileWriteBack    m_tileWriteBack;
    
    // 内部：连接各阶段的数据流
    void connectStages();
};

} // namespace SoftGPU
```

---

## 4. 数据结构设计

### 4.1 RenderCommand（渲染命令）

```cpp
// shared/include/SoftGPU/RenderCommand.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <array>

namespace SoftGPU {

struct DrawParams {
    uint32_t vertexCount = 0;      // 顶点数
    uint32_t firstVertex = 0;     // 起始顶点偏移
    uint32_t indexCount = 0;       // 索引数（0=无索引）
    uint32_t firstIndex = 0;       // 起始索引偏移
    bool indexed = false;          // 是否使用索引
    
    // 渲染状态
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    bool cullEnabled = false;      // Phase1 关闭 cull
};

struct RenderCommand {
    // Vertex Buffer
    const float* vertexBufferData = nullptr;  // 指针（外部数据）
    size_t       vertexBufferSize = 0;         // float 数量
    
    // Index Buffer
    const uint32_t* indexBufferData = nullptr;
    size_t          indexBufferSize = 0;
    
    // Uniforms
    std::array<float, 16> modelMatrix{};
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};
    
    // Draw params
    DrawParams drawParams;
    
    // Clear color
    std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace SoftGPU
```

### 4.2 Vertex 内存布局

```
Vertex Buffer 格式（每顶点 12 floats = 48 bytes）：

Offset  Field   Type    Description
------  -----   ----    -----------
+0     x       float   位置 X
+1     y       float   位置 Y
+2     z       float   位置 Z
+3     w       float   位置 W（齐次坐标）
+4     r       float   颜色 R
+5     g       float   颜色 G
+6     b       float   颜色 B
+7     a       float   颜色 A
+8-11  (unused/reserved for u,v,...) Phase1 不使用

示例（三个顶点组成三角形）：
float vertices[] = {
    // v0
    0.0f, 0.5f, 0.0f, 1.0f,   // position
    1.0f, 0.0f, 0.0f, 1.0f,   // color (red)
    // v1
    -0.5f, -0.5f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,   // color (green)
    // v2
    0.5f, -0.5f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f    // color (blue)
};
```

### 4.3 Framebuffer 内存布局

```
Color Buffer: 640 × 480 × 4 floats (RGBA)
Index = y * WIDTH * 4 + x * 4
Value order: [R, G, B, A]

Depth Buffer: 640 × 480 × 1 float
Index = y * WIDTH + x
Value: 深度值（0.0 = 近平面，1.0 = 远平面）

初始状态：
- Color Buffer = clearColor
- Depth Buffer = 1.0（远平面）
```

### 4.4 Tile GMEM 布局

```
Tile 大小: 32×32 pixels

GMEM Layout (tiled):
每个 Tile 在 GMEM 中连续存储
Tile (tx, ty) 的起始偏移 = (ty * numTilesX + tx) * TILE_WIDTH * TILE_HEIGHT * 4

numTilesX = ceil(640 / 32) = 20
numTilesY = ceil(480 / 32) = 15
GMEM Total = 20 * 15 * 32 * 32 * 4 floats
           = 300 * 4096 * 4 = 4,915,200 floats ≈ 19.7 MB
```

---

## 5. 算法描述

### 5.1 VertexShader MVP 变换

**输入：** 模型空间顶点 `V_model = (x, y, z, w)`  
**输出：** 裁剪空间顶点 `V_clip`

**公式：**
```
V_view = ViewMatrix × V_model
V_clip = ProjectionMatrix × V_view
```

**齐次裁剪：**
变换后检查 `w > 0`（近平面剔除）。

**透视除法（不在 VS 中做，移到 PA 阶段）：**
```
V_ndc = (x/w, y/w, z/w)  // NDC 范围 [-1, 1]
```

### 5.2 PrimitiveAssembly 视锥剔除

**AABB 剔除算法：**

1. 对三角形三个顶点分别做透视除法得到 NDC
2. 计算 AABB：`[min(x), max(x)]`, `[min(y), max(y)]`, `[min(z), max(z)]`
3. 与视锥体比较：
   - `maxX < -1 || minX > 1` → 完全在左/右 → Cull
   - `maxY < -1 || minY > 1` → 完全在上/下 → Cull
   - `maxZ < -1 || minZ > 1` → 完全在近/远 → Cull
4. 否则保留

### 5.3 Rasterizer — Edge Function DDA 光栅化

**原理：** 使用重心坐标边缘函数判断像素是否在三角形内。

**Edge Function：**
```
E(P, A, B) = (P.x - A.x) * (B.y - A.y) - (P.y - A.y) * (B.x - A.x)
```

**性质：**
- E > 0：P 在边左侧（逆时针）
- E < 0：P 在边右侧（顺时针）
- E = 0：P 在边上

**光栅化步骤：**

1. **准备：** 将三角形顶点从 NDC 转换到屏幕坐标：
   ```
   screenX = (ndcX + 1) * 0.5 * viewportWidth
   screenY = (ndcY + 1) * 0.5 * viewportHeight
   ```

2. **计算边界框：** 找到覆盖三角形的最小整数矩形 `[xmin, xmax] × [ymin, ymax]`

3. **DDA 扫描：** 对边界框内每个像素 `(px, py)`：
   ```
   e1 = E(P, V0, V1)
   e2 = E(P, V1, V2)
   e3 = E(P, V2, V0)
   
   if (e1 >= 0 && e2 >= 0 && e3 >= 0)  // 所有边左侧 = 内部
       // 或使用 signed 面积保持一致性
       if ((e1 | e2 | e3) >= 0)        // 全部非负
   ```

4. **重心坐标插值属性：**
   ```
   area = E(V0, V1, V2)  // 有符号面积
   w0 = E(P, V1, V2) / area  // V0 的权重
   w1 = E(P, V2, V0) / area  // V1 的权重
   w2 = E(P, V0, V1) / area  // V2 的权重
   
   z  = w0*V0.z + w1*V1.z + w2*V2.z
   color.r = w0*V0.r + w1*V1.r + w2*V2.r
   // ... 其他属性
   ```

5. **深度值：** 使用重心坐标线性插值 `z`

### 5.4 Depth Test

```
function depthTest(x, y, newZ):
    oldZ = depthBuffer[y * WIDTH + x]
    if newZ < oldZ:          // Z smaller = closer to camera
        depthBuffer[...] = newZ
        return true         // 通过测试，写入 color
    return false            // 未通过，丢弃
```

### 5.5 Tile WriteBack

按 tile 分块将 framebuffer 写入模拟的 GMEM：

```
for ty in 0..numTilesY:
    for tx in 0..numTilesX:
        writeTileToGMEM(tx, ty)
```

每个 tile 的写入：
```
for py in 0..TILE_HEIGHT:
    for px in 0..TILE_WIDTH:
        screenX = tx * TILE_WIDTH + px
        screenY = ty * TILE_HEIGHT + py
        gmemOffset = tileOffset + (py * TILE_WIDTH + px) * 4
        
        gmemColor[gmemOffset + 0] = colorBuffer[screenY * WIDTH * 4 + screenX * 4 + 0]
        gmemColor[gmemOffset + 1] = colorBuffer[...]
        gmemColor[gmemOffset + 2] = colorBuffer[...]
        gmemColor[gmemOffset + 3] = colorBuffer[...]
        gmemDepth[gmemOffset] = depthBuffer[...]
```

---

## 6. 性能指标设计

### 6.1 计数器埋点位置

每个 Stage 独立维护 `PerformanceCounters`，在 `execute()` 入口记录 `start_time`，出口记录 `end_time`。

### 6.2 需要埋的计数器

| Stage | 计数器 | 单位 | 说明 |
|-------|--------|------|------|
| CommandProcessor | `invocation_count` | 次 | DrawCall 调用次数 |
| | `elapsed_ms` | ms | 命令解析耗时 |
| VertexShader | `invocation_count` | 次 | 处理的顶点总数 |
| | `cycle_count` | 周期 | 估算周期数（CPU cycles） |
| | `elapsed_ms` | ms | VS 总耗时 |
| PrimitiveAssembly | `invocation_count` | 次 | 组装的三角形数 |
| | `culled_count` | 次 | 剔除的三角形数 |
| | `elapsed_ms` | ms | PA 总耗时 |
| Rasterizer | `fragment_count` | 个 | 输出的 fragment 总数 |
| | `pixel_tests` | 次 | 像素覆盖测试次数 |
| | `elapsed_ms` | ms | 光栅化总耗时 |
| FragmentShader | `invocation_count` | 次 | 处理的 fragment 数 |
| | `elapsed_ms` | ms | FS 总耗时 |
| Framebuffer | `writes` | 次 | 实际写入的 pixel 数 |
| | `depth_tests` | 次 | Z-test 次数 |
| | `depth_rejects` | 次 | Z-test 拒绝数 |
| | `elapsed_ms` | ms | FB 总耗时 |
| TileWriteBack | `tiles_written` | 个 | 写入的 tile 数 |
| | `elapsed_ms` | ms | 写回总耗时 |

### 6.3 PerformanceCounters 结构

```cpp
struct PerformanceCounters {
    uint64_t cycle_count = 0;       // 周期计数（可用 rdtsc）
    uint64_t invocation_count = 0;   // 调用/处理次数
    double   elapsed_ms = 0.0;       // 墙上时间
};
```

### 6.4 计时实现建议

```cpp
// 使用 std::chrono 高精度计时
#include <chrono>

class StageTimer {
    std::chrono::high_resolution_clock::time_point m_start;
public:
    void start() { m_start = std::chrono::high_resolution_clock::now(); }
    double stop() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - m_start).count();
    }
};

// 周期计数（x86）
#if defined(__x86_64__)
#include <x86intrin.h>
inline uint64_t get_cycles() { return __rdtsc(); }
#else
inline uint64_t get_cycles() { return 0; }
#endif
```

---

## 7. 验收标准（可量化）

### 7.1 功能验收

| ID | 测试项 | 验收条件 | 量化标准 |
|----|--------|----------|----------|
| F1 | 渲染三角形 | 屏幕中心显示彩色三角形 | 三顶点坐标正确 → 三角形覆盖预期像素 |
| F2 | 颜色插值 | 渐变三角形 | 三个顶点不同颜色 → 三角形内颜色线性渐变 |
| F3 | Z-buffer | 两个重叠三角形 | 正确的前后关系，前面的可见 |
| F4 | 视锥剔除 | 三角形完全在视锥体外 | 该三角形不产生任何 fragment |
| F5 | 索引绘制 | 使用 IB 渲染 | 与不使用 IB 的结果一致 |

### 7.2 性能验收

| ID | 测试项 | 目标 | 说明 |
|----|--------|------|------|
| P1 | 三角形渲染帧率 | ≥ 100 FPS | 单三角形，640×480 |
| P2 | 千三角形渲染帧率 | ≥ 10 FPS | 1000 三角形 |
| P3 | 计数器精度 | 误差 < 5% | 计时器与实际耗时对比 |

### 7.3 架构验收

| ID | 测试项 | 验收条件 |
|----|--------|----------|
| A1 | 模块可替换 | 替换 FragmentShader 不影响其他模块 |
| A2 | 接口一致性 | 所有 Stage 继承 IStage |
| A3 | 单线程 | 无 mutex/atomic（Phase1） |

---

## 8. 测试用例设计

### 8.1 单元测试

**VertexShader Test:**
```cpp
TEST_CASE("VertexShader_MVP_Transform") {
    // 输入：标准立方体顶点
    // MVP = Identity
    // 期望：输出 = 输入（world space = view space）
    
    // MVP = 透视投影
    // 期望：远处的顶点 x/y 被压缩
}
```

**PrimitiveAssembly Test:**
```cpp
TEST_CASE("PrimitiveAssembly_Cull_Outside") {
    // 三角形顶点在 NDC [-2, 2] 外
    // 期望：culled = true
}

TEST_CASE("PrimitiveAssembly_Pass_Inside") {
    // 三角形顶点在 NDC [-1, 1] 内
    // 期望：culled = false，输出 1 个三角形
}
```

**Rasterizer Test:**
```cpp
TEST_CASE("Rasterizer_Triangle_Area") {
    // 单位三角形覆盖预期像素
    Vertex v0{0, 0.5f, 0, 1, 1,0,0,1};
    Vertex v1{-0.5f, -0.5f, 0, 1, 0,1,0,1};
    Vertex v2{0.5f, -0.5f, 0, 1, 0,0,1,1};
    
    // 期望：覆盖约 0.5 * 1 * 1 = 0.5 平方单位 = 约 160x120 像素区域
}

TEST_CASE("Rasterizer_EdgeFunction") {
    // P 在边上 → 应该被包含
    // P 在外侧 → 应该被排除
}
```

**Framebuffer Test:**
```cpp
TEST_CASE("Framebuffer_DepthTest_Pass") {
    // 写入 z=0.5，buffer 中 z=1.0
    // 期望：通过，pixel 被更新
}

TEST_CASE("Framebuffer_DepthTest_Fail") {
    // 写入 z=0.8，buffer 中 z=0.5
    // 期望：未通过，pixel 不变
}
```

### 8.2 集成测试

**Test Case 1: 渲染绿色三角形**
```cpp
TEST_CASE("Integration_GreenTriangle") {
    RenderPipeline pipeline;
    
    // 创建简单的绿色三角形
    float vertices[] = {
        0.0f, 0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
    };
    
    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = 12;  // 3 vertices * 4 floats pos + 4 floats color
    cmd.drawParams.vertexCount = 3;
    
    // Identity matrices
    cmd.modelMatrix = identity();
    cmd.viewMatrix = identity();
    cmd.projectionMatrix = identity();
    
    pipeline.render(cmd);
    
    const auto* fb = pipeline.getFramebuffer();
    const float* color = fb->getColorBuffer();
    
    // 中心区域应该是绿色
    size_t centerIdx = (240 * 640 + 320) * 4;
    CHECK(color[centerIdx + 1] > 0.5f);  // G channel
}
```

**Test Case 2: 两个重叠三角形（Z-test）**
```cpp
TEST_CASE("Integration_OverlappingTriangles_ZBuffer") {
    // 红色三角形在前（z=0.1）
    // 蓝色三角形在后（z=0.5）
    // 期望：重叠区域显示红色
}
```

### 8.3 性能基准测试

```cpp
TEST_CASE("Benchmark_1000_Triangles") {
    auto start = std::chrono::high_resolution_clock::now();
    
    // 渲染 1000 个随机三角形
    for (int i = 0; i < 1000; ++i) {
        pipeline.render(singleTriangleCmd);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    double fps = 1000.0 / elapsed;
    CHECK(fps >= 10.0);  // ≥10 FPS
}
```

---

## 9. 风险点与应对

### 9.1 技术风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| R1 | Edge Function 符号约定导致三角形被错误剔除 | 中 | 高 | 明确约定逆时针为正面，统一使用 signed area |
| R2 | 透视除法精度丢失 | 低 | 中 | 使用 `double` 中间计算，或保持 `w` 传递到 rasterizer |
| R3 | 浮点精度导致 Z-fighting | 低 | 中 | Phase1 简化场景避免，Phase2 加 polygon offset |
| R4 | Framebuffer 大小硬编码导致耦合 | 中 | 低 | 用 constexpr 统一管理 WIDTH/HEIGHT |

### 9.2 架构风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| A1 | 后续扩展多线程时接口不兼容 | 中 | 高 | Phase1 接口设计时预留 thread-safe 扩展点（Phase2 再加锁） |
| A2 | 各 Stage 数据直接传递，耦合紧 | 低 | 中 | 保持 ABC 接口，未来可改用 Command Buffer 解耦 |

### 9.3 测试风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| T1 | 难以调试光栅化错误 | 高 | 中 | 加入 debug overlay：显示 wireframe triangle + fragment coverage |
| T2 | 性能计数精度不足 | 中 | 低 | 使用 `rdtsc` 而非 wall-clock 对小任务更准确 |

---

## 10. 文件结构

```
SoftGPU/
├── shared/
│   ├── design/
│   │   └── PHASE1_DESIGN.md          ← 本文档
│   ├── include/
│   │   └── SoftGPU/
│   │       ├── SoftGPU.hpp           ← 主头文件
│   │       ├── RenderCommand.hpp     ← 渲染命令结构
│   │       ├── Common.hpp            ← 公共类型（Vertex, Fragment, etc.）
│   │       ├── stages/
│   │       │   ├── IStage.hpp        ← 抽象基类
│   │       │   ├── CommandProcessor.hpp
│   │       │   ├── VertexShader.hpp
│   │       │   ├── PrimitiveAssembly.hpp
│   │       │   ├── Rasterizer.hpp
│   │       │   ├── FragmentShader.hpp
│   │       │   ├── Framebuffer.hpp
│   │       │   └── TileWriteBack.hpp
│   │       └── pipeline/
│   │           └── RenderPipeline.hpp
│   └── src/
│       ├── CommandProcessor.cpp
│       ├── VertexShader.cpp
│       ├── PrimitiveAssembly.cpp
│       ├── Rasterizer.cpp
│       ├── FragmentShader.cpp
│       ├── Framebuffer.cpp
│       ├── TileWriteBack.cpp
│       └── RenderPipeline.cpp
├── app/
│   ├── CMakeLists.txt
│   └── main.cpp                       ← 测试入口 + ImGui 可视化
├── tests/
│   ├── CMakeLists.txt
│   ├── test_VertexShader.cpp
│   ├── test_Rasterizer.cpp
│   ├── test_Framebuffer.cpp
│   └── test_Integration.cpp
└── CMakeLists.txt                     ← 根 CMake
```

---

## 附录 A：关键常量

```cpp
namespace SoftGPU {
    constexpr uint32_t FRAMEBUFFER_WIDTH  = 640;
    constexpr uint32_t FRAMEBUFFER_HEIGHT = 480;
    constexpr uint32_t TILE_WIDTH         = 32;
    constexpr uint32_t TILE_HEIGHT        = 32;
    constexpr uint32_t VERTEX_STRIDE      = 12;  // floats per vertex
    constexpr float    CLEAR_DEPTH        = 1.0f; // 远平面
}
```

## 附录 B：建议的 CMake 最小配置

```cmake
cmake_minimum_required(VERSION 3.15)
project(SoftGPU VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(SoftGPU STATIC
    shared/src/CommandProcessor.cpp
    shared/src/VertexShader.cpp
    shared/src/PrimitiveAssembly.cpp
    shared/src/Rasterizer.cpp
    shared/src/FragmentShader.cpp
    shared/src/Framebuffer.cpp
    shared/src/TileWriteBack.cpp
    shared/src/RenderPipeline.cpp
)

target_include_directories(SoftGPU PUBLIC
    shared/include
)

enable_testing()
add_subdirectory(tests)
```

---

## 10.5 FrameDumper 模块（补充）

### 问题背景
服务器无 GUI 环境，无法直接看到渲染结果。需要在渲染完成后 dump 结果到文件。

### 实现方案

#### PPM 格式输出（Phase1 优先实现）

```cpp
// shared/include/SoftGPU/utils/FrameDumper.hpp
#pragma once
#include <string>

namespace SoftGPU {

class FrameDumper {
public:
    FrameDumper() = default;
    
    // dump RGBA float buffer 为 PPM 格式
    static void dumpPPM(const float* colorBuffer,
                        uint32_t width, uint32_t height,
                        const std::string& filename);
    
    // dump 为 PNG 格式（后续扩展）
    static void dumpPNG(const float* colorBuffer,
                        uint32_t width, uint32_t height,
                        const std::string& filename);
};

} // namespace SoftGPU
```

```cpp
// shared/src/FrameDumper.cpp
#include "FrameDumper.hpp"
#include <cstdio>
#include <cmath>

namespace SoftGPU {

void FrameDumper::dumpPPM(const float* colorBuffer,
                          uint32_t width, uint32_t height,
                          const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "wb");
    if (!f) return;
    
    // PPM header: P6 = binary RGB
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    
    for (uint32_t i = 0; i < width * height; i++) {
        uint8_t r = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, colorBuffer[i*4 + 0])) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, colorBuffer[i*4 + 1])) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, colorBuffer[i*4 + 2])) * 255.0f);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}

} // namespace SoftGPU
```

### 文件格式说明

| 格式 | 后缀 | 优点 | 缺点 |
|------|------|------|------|
| PPM (P6) | .ppm | 无依赖，单函数实现 | 文件大，无压缩 |
| PNG | .png | 通用压缩 | 需 libpng 或 stb |
| Raw RGBA | .raw | 最简单 | 无法直接查看 |

### 使用方式

```cpp
// 渲染完成后
const float* colorData = framebuffer->getColorBuffer();
FrameDumper::dumpPPM(colorData, 640, 480, "frame_0000.ppm");

// 在有 GUI 的机器上用任意图片查看器打开 .ppm 文件
```

### 验收标准

- F6: 运行 `./SoftGPU --dump frame.ppm` 生成可查看的 PPM 文件
- F7: 文件可用 `feh`、`Photoshop`、`Preview` 等正常打开

---

**文档结束**

*Architect: 艾拉（@ayla）*  
*SoftGPU Project — Pixel Workshop*  
*Phase1: Minimal Runnable GPU — Triangle Renderer*
