# PHASE2 Design Document — SoftGPU TBR 核心实现

**项目名称：** SoftGPU Pixel Workshop  
**阶段：** PHASE2 — Tile-Based Rendering 核心  
**架构师：** 陈二虎（@erhu）  
**日期：** 2026-03-26  
**版本：** v1.0  
**依赖：** PHASE1（最小可运行 GPU）

---

## 1. 阶段目标

### 1.1 核心目标

将 PHASE1 的"全屏光栅化后写回"架构重构为 **TBR（Tile-Based Rendering）** 架构：

- **Tiling 阶段**：按 tile 对图元做 binning，产出 per-tile triangle list
- **Per-Tile 执行**：每个 tile 独立完成 VS → Rasterizer → FS → DepthTest，结果暂存在 tile-local memory（LMEM）
- **Tile 写回**：tile 执行完毕后，将 LMEM 数据批量写回 GMEM
- **GMEM Bandwidth 模型**：令牌桶模拟带宽约束
- **L2 Cache 模拟**（简化版）：模拟 GMEM 和 LMEM 之间的 cache line 行为

### 1.2 与 PHASE1 的关键差异

| 维度 | PHASE1 | PHASE2 |
|------|--------|--------|
| 光栅化范围 | 全屏扫描每个 triangle | 仅扫描 triangle 覆盖的 tile |
| Fragment 输出 | 全局 framebuffer | Tile-local LMEM |
| GMEM 访问 | 每 fragment 写回 | tile 完成时批量写回 |
| 带宽模型 | 无 | 令牌桶 + 带宽利用率统计 |
| L2 Cache | 无 | 简化模拟（命中率统计） |

### 1.3 技术约束

- **语言：** C++17
- **Tile Size：** 32×32 固定
- **Framebuffer：** 640×480（20×15 = 300 tiles）
- **内存布局：** GMEM tile 行优先（row-major in tile grid）
- **执行模型：** 单线程顺序（MTL 留给 PHASE3）

---

## 2. TBR 架构图

### 2.1 整体管线流程

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                                  TBR Render Pipeline                              │
│                                                                                  │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌─────────────┐  │
│  │   Host CPU   │───▶│  Command     │───▶│  Vertex      │───▶│  Primitive  │  │
│  │  (App/Game)  │    │  Processor   │    │  Shader      │    │  Assembly   │  │
│  └──────────────┘    └──────────────┘    └──────────────┘    └─────────────┘  │
│                                                                         │        │
│                                                                         ▼        │
│  ┌──────────────────────────────────────────────────────────────────────────┐   │
│  │                         TilingStage [NEW]                                 │   │
│  │   输入：transformation 后的 triangles（NDC space）                         │   │
│  │   输出：per-tile triangle list（Binned Triangles）                        │   │
│  │   算法：计算 bounding box，遍历覆盖的 tile，加入 bin                       │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
│                                                                         │        │
│                                                    ┌──────────────────────────┐ │
│                                                    │   Per-Tile Loop          │ │
│  ┌──────────────┐    ┌──────────────┐    ┌───────▼──────┐    ┌────────────┐ │ │
│  │   GMEM       │◀───│   Tile       │◀───│  Fragment    │◀───│ Rasterizer │ │ │
│  │  (Simulated) │    │  WriteBack   │    │   Shader    │    │ (per-tile) │ │ │
│  └──────┬───────┘    └──────────────┘    └─────────────┘    └────────────┘ │ │
│         │                                    ▲              ▲               │ │
│         │                    ┌────────────────┴──────────────┘               │ │
│         │                    │  TileBuffer (LMEM)                             │ │
│         │                    │  32×32 Color + 32×32 Depth                      │ │
│         │                    │  无 GMEM 访问（tile-local）                     │ │
│         │                    └───────────────────────────────────────────────┘ │
│         │                                                                    │ │
│         │         ┌────────────────────────────────────────────────────┐     │ │
│         └────────▶│              MemorySubsystem (GMEM)                  │     │ │
│                   │  令牌桶带宽模型 + L2 Cache 模拟                        │     │ │
│                   │  read_bytes / write_bytes / bandwidth_util            │     │ │
│                   └────────────────────────────────────────────────────┘     │ │
│                                                                              │ │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Tiling + Per-Tile 执行时序

```
Frame Start
    │
    ▼
CommandProcessor → VertexShader → PrimitiveAssembly
    │
    ▼
TilingStage          ←── 输入：全部 triangles（NDC）
    │                    输出：tileBins[300]（per-tile triangle list）
    ▼
For each tile (0 .. 299):
    │
    ├── loadTileFromGMEM()        ← 从 GMEM 加载 tile 历史数据（LMEM 初始化）
    │                               bandwidth: TILE_SIZE² × (color+depth) bytes
    │
    ├── Rasterizer (tile scope)   ← 输入：该 tile 覆盖的 triangle list
    │                               输出：tile-local fragments
    │
    ├── FragmentShader            ← 每个 fragment 执行一次
    │
    ├── DepthTest (in-Tile)       ← TileBuffer 内的 depth buffer
    │
    ├── TileBuffer Write          ← 写入 LMEM（无 GMEM 访问）
    │
    └── writeTileToGMEM()         ← Tile 执行完毕，批量写回 GMEM
                                    bandwidth: TILE_SIZE² × (color+depth) bytes
    │
    ▼
Frame Done
```

### 2.3 LMEM / GMEM 数据流

```
GMEM (off-chip, high latency, bandwidth-limited)
  │
  │  loadTile / writeTile (bandwidth-constrained)
  ▼
LMEM (on-chip, low latency, per-tile)
  │
  │  Rasterizer → FS → DepthTest → TileBuffer
  ▼
Color Buffer (32×32 RGBA float)
Depth Buffer (32×32 float)
```

---

## 3. 模块接口定义

### 3.1 新增/修改的模块列表

| 模块 | 类型 | 职责 |
|------|------|------|
| `TilingStage` | 新增 | Binning 算法，按 tile 分配图元 |
| `TileBuffer` | 新增 | LMEM：per-tile color + depth buffer |
| `MemorySubsystem` | 新增 | GMEM 带宽模型 + L2 Cache 模拟 |
| `Rasterizer` | 修改 | 输入从全量 triangles 改为 per-tile triangle list |
| `FragmentShader` | 修改 | 输出从全局 FB 改为 TileBuffer |
| `Framebuffer` | 移除 | PHASE1 的全局 framebuffer 被 TileBuffer + GMEM 替代 |
| `TileWriteBack` | 重构 | 改为 GMEM ↔ LMEM 交互 |
| `RenderPipeline` | 修改 | 新增 TilingStage、MemorySubsystem、TileBuffer，协调 per-tile loop |

### 3.2 TilingStage

```cpp
// shared/include/SoftGPU/stages/TilingStage.hpp
#pragma once
#include "IStage.hpp"
#include "PrimitiveAssembly.hpp"
#include <vector>
#include <array>
#include <cstdint>

namespace SoftGPU {

// Tile 配置
constexpr uint32_t TILE_WIDTH  = 32;
constexpr uint32_t TILE_HEIGHT = 32;
constexpr uint32_t TILE_SIZE   = TILE_WIDTH * TILE_HEIGHT;  // 1024

// Framebuffer 尺寸（须与 RenderPipeline 配置一致）
constexpr uint32_t FRAMEBUFFER_WIDTH  = 640;
constexpr uint32_t FRAMEBUFFER_HEIGHT = 480;

// Tile 网格尺寸
constexpr uint32_t NUM_TILES_X = (FRAMEBUFFER_WIDTH + TILE_WIDTH - 1) / TILE_WIDTH;   // 20
constexpr uint32_t NUM_TILES_Y = (FRAMEBUFFER_HEIGHT + TILE_HEIGHT - 1) / TILE_HEIGHT; // 15
constexpr uint32_t NUM_TILES   = NUM_TILES_X * NUM_TILES_Y;  // 300

// TileBin: 一个 tile 内所有图元的索引
struct TileBin {
    std::vector<uint32_t> triangleIndices;  // 指向原始 triangle 数组的索引
};

class TilingStage : public IStage {
public:
    TilingStage();

    // 设置输入（来自 PrimitiveAssembly 的 triangles）
    void setInput(const std::vector<Triangle>& triangles);

    // IStage 实现
    const char* getName() const override { return "TilingStage"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 获取某个 tile 的 bin（triangle index list）
    const TileBin& getTileBin(uint32_t tileIndex) const;
    const TileBin& getTileBin(uint32_t tileX, uint32_t tileY) const {
        return getTileBin(tileY * NUM_TILES_X + tileX);
    }

    // 获取受影响的 tile 数量
    uint32_t getNumAffectedTiles() const { return m_tilesAffected; }

private:
    std::vector<Triangle> m_inputTriangles;
    std::array<TileBin, NUM_TILES> m_tileBins;  // 固定 300 个 bin
    uint32_t m_tilesAffected = 0;
    PerformanceCounters m_counters;

    // 内部：计算三角形 bounding box（返回覆盖的 tile 范围）
    void computeBbox(const Triangle& tri,
                     int32_t& minTileX, int32_t& maxTileX,
                     int32_t& minTileY, int32_t& maxTileY) const;

    // 内部：NDC → tile grid 坐标
    bool ndcToTile(float ndcX, float ndcY,
                   int32_t& tileX, int32_t& tileY) const;
};

} // namespace SoftGPU
```

### 3.3 TileBuffer（LMEM）

```cpp
// shared/include/SoftGPU/stages/TileBuffer.hpp
#pragma once
#include "Rasterizer.hpp"
#include <array>
#include <cstdint>

namespace SoftGPU {

// 每个 tile 的 local memory 大小
// Color: 32×32 × 4 floats = 4096 floats = 16 KB
// Depth: 32×32 × 1 float  = 1024 floats =  4 KB
// Total per tile: ~20 KB
constexpr uint32_t TILE_BUFFER_COLOR_SIZE = TILE_SIZE * 4;  // RGBA
constexpr uint32_t TILE_BUFFER_DEPTH_SIZE = TILE_SIZE;       // Z

struct TileBuffer {
    std::array<float, TILE_BUFFER_COLOR_SIZE> color;  // RGBA
    std::array<float, TILE_BUFFER_DEPTH_SIZE> depth;  // Z

    TileBuffer() {
        color.fill(0.0f);
        depth.fill(1.0f);  // 初始化为远平面
    }

    void clear() {
        color.fill(0.0f);
        depth.fill(1.0f);
    }
};

class TileBufferManager {
public:
    TileBufferManager();

    // 获取指定 tile 的 buffer 引用
    TileBuffer& getTileBuffer(uint32_t tileIndex);
    TileBuffer& getTileBuffer(uint32_t tileX, uint32_t tileY) {
        return getTileBuffer(tileY * NUM_TILES_X + tileX);
    }

    // 初始化某个 tile 的 buffer（clear）
    void initTile(uint32_t tileIndex);
    void initTile(uint32_t tileX, uint32_t tileY) { initTile(tileY * NUM_TILES_X + tileX); }

    // Per-fragment 操作
    // Z-test 通过 → 写入 color + depth
    // Z-test 失败 → 丢弃
    bool depthTestAndWrite(uint32_t localX, uint32_t localY,
                           float z, const float color[4]);

    // GMEM 同步接口（由 MemorySubsystem 调用）
    void loadFromGMEM(uint32_t tileIndex, const float* gmemColor, const float* gmemDepth);
    void storeToGMEM(uint32_t tileIndex, float* outColor, float* outDepth) const;

    // 统计
    uint64_t getTileWriteCount() const { return m_tileWriteCount; }

private:
    std::array<TileBuffer, NUM_TILES> m_tileBuffers;
    uint64_t m_tileWriteCount = 0;
};

} // namespace SoftGPU
```

### 3.4 MemorySubsystem（GMEM Bandwidth Model）

```cpp
// shared/include/SoftGPU/memory/MemorySubsystem.hpp
#pragma once
#include <cstdint>
#include <chrono>

namespace SoftGPU {

// 带宽配置（可调）
constexpr double DEFAULT_BANDWIDTH_GBPS = 100.0;  // 100 GB/s
constexpr double NS_PER_SEC = 1e9;

// Cache 配置
constexpr uint32_t CACHE_LINE_SIZE = 64;   // bytes（模拟 L2 cache line）
constexpr uint32_t L2_CACHE_SETS   = 256;  // 简化：256 sets
constexpr uint32_t L2_CACHE_WAYS   = 8;    // 8-way set associative
constexpr size_t   L2_CACHE_SIZE   = CACHE_LINE_SIZE * L2_CACHE_SETS * L2_CACHE_WAYS; // 128 KB

// 访问类型
enum class MemoryAccessType {
    LoadTile,     // GMEM → LMEM（load tile 到 TileBuffer）
    StoreTile,    // LMEM → GMEM（tile 完成，写回）
    ReadVertex,   // VB/IB 读取（Phase2 不重点建模，简化为计数）
};

// 令牌桶状态
struct TokenBucket {
    double tokens;          // 当前令牌数
    double maxTokens;       // 桶容量
    double refillRate;      // 每秒补充令牌数（bytes/s）
    double lastRefillTime;  // 上次补充时间（模拟时间）

    void init(double bandwidthGBps, double capacityBytes = 0);
    bool tryConsume(size_t bytes);
    void refill(double currentTime);
};

// L2 Cache 行状态
struct CacheLine {
    uint64_t tag = 0;
    bool valid = false;
    bool dirty = false;
    uint32_t lastUsed = 0;  // 简化 LRU
};

// L2 Cache 模拟器
class L2CacheSim {
public:
    L2CacheSim();

    // 访问地址，返回是否命中（hit = true）
    bool access(uint64_t address, bool isWrite);

    // 统计
    double getHitRate() const;
    uint64_t getHits() const { return m_hits; }
    uint64_t getMisses() const { return m_misses; }
    void resetStats();

private:
    uint64_t m_hits = 0;
    uint64_t m_misses = 0;
    uint32_t m_currentTick = 0;
    std::array<CacheLine, L2_CACHE_SETS * L2_CACHE_WAYS> m_lines;

    uint32_t getSetIndex(uint64_t address) const;
    uint64_t getTag(uint64_t address) const;
};

class MemorySubsystem {
public:
    MemorySubsystem(double bandwidthGBps = DEFAULT_BANDWIDTH_GBPS);

    // 带宽记录接口
    // 每次 GMEM 访问调用 addAccess，消耗令牌
    // 返回值：true = 允许访问，false = 带宽耗尽（阻塞模拟）
    bool addAccess(size_t bytes, MemoryAccessType type);

    // 统计读取 / 写入字节数
    void recordRead(size_t bytes)  { m_totalReadBytes += bytes; }
    void recordWrite(size_t bytes) { m_totalWriteBytes += bytes; }

    // GMEM 模拟读写（带 L2 cache 模拟）
    void readGMEM(void* dst, uint64_t offset, size_t bytes);
    void writeGMEM(uint64_t offset, const void* src, size_t bytes);

    // L2 Cache 查询
    L2CacheSim& getL2Cache() { return m_l2Cache; }

    // 带宽利用率（0.0 ~ 1.0）
    double getBandwidthUtilization() const;
    double getConsumedBandwidthGBps() const;  // 实际消耗带宽

    // 重置统计
    void resetCounters();

    // 性能计数器
    uint64_t getReadBytes()  const { return m_totalReadBytes; }
    uint64_t getWriteBytes() const { return m_totalWriteBytes; }
    uint64_t getAccessCount() const { return m_accessCount; }

private:
    TokenBucket m_bucket;
    L2CacheSim  m_l2Cache;
    uint64_t    m_totalReadBytes = 0;
    uint64_t    m_totalWriteBytes = 0;
    uint64_t    m_accessCount = 0;

    double      m_currentSimTime = 0.0;  // 模拟时间（ns）
};

} // namespace SoftGPU
```

### 3.5 Rasterizer（修改：per-tile 输入）

```cpp
// shared/include/SoftGPU/stages/Rasterizer.hpp
// ── PHASE2 修改点 ──
// setInput() 不再接收全量 triangles
// 改为 setInputPerTile(triangleList, tileX, tileY)
// 输出仍然输出到 m_outputFragments（供 TileBuffer 消费）

#pragma once
#include "IStage.hpp"
#include "PrimitiveAssembly.hpp"
#include <vector>
#include <cstdint>

namespace SoftGPU {

struct Fragment {
    uint32_t localX, localY;  // Tile 内坐标（0..31）
    float    z;               // 深度值
    float    r, g, b, a;     // 插值颜色
};

class Rasterizer {
public:
    Rasterizer();

    // [PHASE2] 按 tile 设置输入
    void setInputPerTile(const std::vector<Triangle>& triangles,
                         uint32_t tileX, uint32_t tileY);

    // 清空输出
    void clearOutput();

    // 执行光栅化
    void executePerTile();

    // 获取输出 fragments
    const std::vector<Fragment>& getOutput() const { return m_outputFragments; }

    const char* getName() const { return "Rasterizer"; }
    const PerformanceCounters& getCounters() const { return m_counters; }

private:
    std::vector<Triangle>  m_inputTriangles;
    uint32_t m_tileX = 0, m_tileY = 0;  // 当前 tile 坐标
    std::vector<Fragment>  m_outputFragments;
    PerformanceCounters    m_counters;

    void rasterizeTriangle(const Triangle& tri);
    float edgeFunction(const Vertex& a, const Vertex& b, float px, float py) const;
};

} // namespace SoftGPU
```

### 3.6 FragmentShader（修改：输出到 TileBuffer）

```cpp
// shared/include/SoftGPU/stages/FragmentShader.hpp
// ── PHASE2 修改点 ──
// execute() 内部不再写入全局 Framebuffer
// 改为遍历 fragments，调用 TileBufferManager::depthTestAndWrite()

#pragma once
#include "IStage.hpp"
#include "Rasterizer.hpp"
#include "TileBuffer.hpp"

namespace SoftGPU {

class FragmentShader : public IStage {
public:
    FragmentShader();

    // 注入 TileBufferManager 引用
    void setTileBufferManager(TileBufferManager* manager) { m_tileBuffer = manager; }
    void setTileIndex(uint32_t idx) { m_tileIndex = idx; }

    void setInput(const std::vector<Fragment>& fragments);

    const char* getName() const override { return "FragmentShader"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

private:
    std::vector<Fragment>    m_inputFragments;
    TileBufferManager*       m_tileBuffer = nullptr;
    uint32_t                 m_tileIndex = 0;
    PerformanceCounters      m_counters;
};

} // namespace SoftGPU
```

### 3.7 RenderPipeline（PHASE2 主控）

```cpp
// shared/include/SoftGPU/pipeline/RenderPipeline.hpp
// ── PHASE2 重构 ──
// 1. 新增 TilingStage、MemorySubsystem、TileBufferManager
// 2. 移除全局 Framebuffer（或降级为 Present 用途）
// 3. 主循环改为 for tile: load → raster → fs → write

#pragma once
#include "CommandProcessor.hpp"
#include "VertexShader.hpp"
#include "PrimitiveAssembly.hpp"
#include "TilingStage.hpp"
#include "Rasterizer.hpp"
#include "FragmentShader.hpp"
#include "TileBuffer.hpp"
#include "MemorySubsystem.hpp"
#include "TileWriteBack.hpp"
#include "RenderCommand.hpp"

namespace SoftGPU {

class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    // 执行一次完整渲染
    void render(const RenderCommand& command);

    // 获取 GMEM 数据（供 Present / 导出）
    const float* getGMEMColor() const { return m_gmemColorBuffer.data(); }
    const float* getGMEMDepth() const { return m_gmemDepthBuffer.data(); }

    // 性能报告
    void printPerformanceReport() const;

    // 带宽 / Cache 统计
    const MemorySubsystem& getMemorySubsystem() const { return m_memory; }
    double getBandwidthUtilization() const { return m_memory.getBandwidthUtilization(); }

private:
    CommandProcessor    m_commandProcessor;
    VertexShader        m_vertexShader;
    PrimitiveAssembly   m_primitiveAssembly;
    TilingStage         m_tilingStage;
    Rasterizer          m_rasterizer;
    FragmentShader      m_fragmentShader;
    TileBufferManager   m_tileBuffer;
    MemorySubsystem     m_memory;
    TileWriteBack       m_tileWriteBack;

    // GMEM 模拟存储（CPU 端）
    std::vector<float>  m_gmemColorBuffer;  // 全部分 tile 存
    std::vector<float>  m_gmemDepthBuffer;

    // 内部：执行单个 tile
    void executeTile(uint32_t tileIndex, uint32_t tileX, uint32_t tileY);

    // 内部：初始化 GMEM
    void initGMEM();
};

} // namespace SoftGPU
```

---

## 4. 数据结构设计

### 4.1 TileBin 数据结构

```cpp
// TileBin = per-tile triangle list
// 内存布局：vector< TileBin >，固定 NUM_TILES = 300 个
// 每个 TileBin 内部是 vector<uint32_t> triangleIndices
//
// 优点：节省内存（只存被该 tile 覆盖的 triangle）
// 缺点：随机访问性能差（与 tile 分布有关）
//
// 替代方案（未来 PHASE3）：
//   - 固定大小 array<uint32_t, MAX_TRIS_PER_TILE> +
//     原子计数（避免 heap allocation）
```

### 4.2 TileBuffer 内存布局

```
LMEM Layout（per tile, 32×32）：

Color Buffer:   localIndex = localY * 32 + localX
                color[localIndex * 4 + 0] = R
                color[localIndex * 4 + 1] = G
                color[localIndex * 4 + 2] = B
                color[localIndex * 4 + 3] = A

Depth Buffer:   localIndex = localY * 32 + localX
                depth[localIndex] = Z (0.0 ~ 1.0)
```

### 4.3 GMEM Layout（tiled，行优先于 tile grid）

```
GMEM Color:  NUM_TILES_X * NUM_TILES_Y 个 tile 连续排列
             tile(tX, tY) 的起始 float 偏移：
               offset = (tY * NUM_TILES_X + tX) * TILE_SIZE * 4
             pixel(localX, localY) 在 tile 内的偏移：
               pixelOffset = (localY * TILE_WIDTH + localX) * 4
             最终 float 偏移 = offset + pixelOffset

GMEM Depth:   类似，每像素 1 个 float
             offset = (tY * NUM_TILES_X + tX) * TILE_SIZE
             depthOffset = offset + localY * TILE_WIDTH + localX

总 GMEM 大小：
  Color: 300 * 32 * 32 * 4 * 4 bytes = 300 * 16384 bytes = 4.9 MB
  Depth: 300 * 32 * 32 * 4 bytes     = 300 * 4096 bytes  = 1.2 MB
  Total: ~6.1 MB
```

### 4.4 MemoryAccess 记录结构

```cpp
struct MemoryAccess {
    MemoryAccessType type;   // LoadTile / StoreTile
    size_t bytes;            // 传输字节数
    uint64_t timestamp;      // 模拟时间戳（ns）
};
```

### 4.5 TilingStage 输入/输出

```
输入：PrimitiveAssembly 输出的 vector<Triangle>
      Triangle 包含 3 个 Vertex（clip space / NDC space）

输出：array<TileBin, 300> tileBins
      TileBin.triangleIndices 存储原始 triangles 数组的索引

特殊处理：
- 不在后处理视锥体内的 triangle → 直接跳过，不进入任何 bin
- triangle 覆盖 0 个 tile（完全在屏幕外）→ 不加入任何 bin
```

---

## 5. 算法描述

### 5.1 Tiling 算法（Binning）

**目标：** 将每个 triangle 分配到其覆盖的所有 tile。

**输入：** `vector<Triangle> triangles`（NDC space）  
**输出：** `array<TileBin, 300> tileBins`

**算法步骤：**

```
for each triangle tri in triangles:
    // Step 1: NDC 透视除法（如果尚未做）
    v0 = tri.v[0] / tri.v[0].w
    v1 = tri.v[1] / tri.v[1].w
    v2 = tri.v[2] / tri.v[2].w

    // Step 2: 计算 NDC bounding box
    minX = min(v0.x, v1.x, v2.x)
    maxX = max(v0.x, v1.x, v2.x)
    minY = min(v0.y, v1.y, v2.y)
    maxY = max(v0.y, v1.y, v2.y)

    // Step 3: NDC → Screen 坐标映射
    // screenX = (ndcX + 1) * 0.5 * viewportWidth
    // screenY = (ndcY + 1) * 0.5 * viewportHeight
    screenMinX = (minX + 1.0f) * 0.5f * FRAMEBUFFER_WIDTH
    screenMaxX = (maxX + 1.0f) * 0.5f * FRAMEBUFFER_WIDTH
    screenMinY = (minY + 1.0f) * 0.5f * FRAMEBUFFER_HEIGHT
    screenMaxY = (maxY + 1.0f) * 0.5f * FRAMEBUFFER_HEIGHT

    // Step 4: 计算覆盖的 tile 范围（整数向上/下取整）
    tileX0 = max(0, floor(screenMinX / TILE_WIDTH))
    tileX1 = min(NUM_TILES_X - 1, floor(screenMaxX / TILE_WIDTH))
    tileY0 = max(0, floor(screenMinY / TILE_HEIGHT))
    tileY1 = min(NUM_TILES_Y - 1, floor(screenMaxY / TILE_HEIGHT))

    // Step 5: 遍历覆盖范围内的每个 tile，加入 bin
    for tileY in [tileY0 .. tileY1]:
        for tileX in [tileX0 .. tileX1]:
            tileIndex = tileY * NUM_TILES_X + tileX
            tileBins[tileIndex].triangleIndices.push_back(triangleIndex)
            m_tilesAffected++  // 统计计数
```

**复杂度：**  
- 每个 triangle：`O(number_of_covered_tiles)`  
- 最好情况（小 triangle）：`O(1)`  
- 最坏情况（大 triangle，覆盖全屏）：`O(NUM_TILES) = O(300)`

**关键边界处理：**
- `screenMinX > screenMaxX` 或 `minY > maxY` → triangle 退化为线/点，跳过
- tile 坐标越界 → clamp 到 `[0, NUM_TILES_X/Y - 1]`
- `tileX0 > tileX1` 或 `tileY0 > tileY1` → triangle 不覆盖任何完整 tile（可能在边缘处退化）

### 5.2 Per-Tile 执行流程

```
executeTile(tileIndex, tileX, tileY):
    // 1. 初始化 TileBuffer（clear）
    m_tileBuffer.initTile(tileIndex)

    // 2. 从 GMEM 加载 tile 历史数据（如果需要）
    //    Phase2: 每次清空，无历史加载
    //    Phase3+: 支持 partial update，从 GMEM load
    // m_memory.addAccess(TILE_SIZE * (4+1) * 4, MemoryAccessType::LoadTile)
    // m_tileBuffer.loadFromGMEM(tileIndex, gmemColorPtr, gmemDepthPtr)

    // 3. 获取该 tile 的 triangle list
    const auto& triList = m_tilingStage.getTileBin(tileIndex)
    if (triList.empty()):
        goto writeback  // 无图元覆盖，直接写回

    // 4. Rasterizer（per-tile）
    m_rasterizer.setInputPerTile(triList_as_triangles, tileX, tileY)
    m_rasterizer.executePerTile()  // 输出 m_rasterizer.getOutput()

    // 5. FragmentShader
    m_fragmentShader.setInput(m_rasterizer.getOutput())
    m_fragmentShader.setTileBufferManager(&m_tileBuffer)
    m_fragmentShader.setTileIndex(tileIndex)
    m_fragmentShader.execute()

writeback:
    // 6. Tile 写回 GMEM
    m_tileBuffer.storeToGMEM(tileIndex, gmemColorPtr, gmemDepthPtr)
    m_memory.addAccess(TILE_SIZE * 4 * 4, MemoryAccessType::StoreTile)  // color
    m_memory.addAccess(TILE_SIZE * 4,     MemoryAccessType::StoreTile)  // depth
    m_tileWriteCount++
```

### 5.3 Depth Test（in-Tile）

```
depthTestAndWrite(localX, localY, z, color):
    idx = localY * TILE_WIDTH + localX
    if z < tileBuffer.depth[idx]:       // Z smaller = closer
        tileBuffer.depth[idx] = z
        tileBuffer.color[idx*4 + 0] = color[0]
        tileBuffer.color[idx*4 + 1] = color[1]
        tileBuffer.color[idx*4 + 2] = color[2]
        tileBuffer.color[idx*4 + 3] = color[3]
        return true
    return false
```

### 5.4 GMEM ↔ LMEM 数据传输

```
loadTileFromGMEM(tileIndex):
    gmemOffsetColor = tileIndex * TILE_SIZE * 4
    gmemOffsetDepth = tileIndex * TILE_SIZE
    memcpy(tileBuffer.color, gmemColorBuffer + gmemOffsetColor, TILE_SIZE * 4 * sizeof(float))
    memcpy(tileBuffer.depth, gmemDepthBuffer + gmemOffsetDepth, TILE_SIZE * sizeof(float))
    recordRead(TILE_SIZE * 4 * 4 + TILE_SIZE * 4)  // color + depth bytes

storeTileToGMEM(tileIndex):
    gmemOffsetColor = tileIndex * TILE_SIZE * 4
    gmemOffsetDepth = tileIndex * TILE_SIZE
    memcpy(gmemColorBuffer + gmemOffsetColor, tileBuffer.color, TILE_SIZE * 4 * sizeof(float))
    memcpy(gmemDepthBuffer + gmemOffsetDepth, tileBuffer.depth, TILE_SIZE * sizeof(float))
    recordWrite(TILE_SIZE * 4 * 4 + TILE_SIZE * 4)
```

---

## 6. 带宽模型设计

### 6.1 令牌桶模型

```
TokenBucket 算法：

初始化：
    maxTokens = bandwidth_GBps * 1024^3 * refillPeriodSec  # 桶容量 = 1 秒的带宽
    tokens = maxTokens
    refillRate = bandwidth_GBps * 1024^3  # bytes/s
    lastRefillTime = 0

每次访问 addAccess(bytes):
    now = currentSimTime()  # ns

    # 补充令牌（按时间线性补充）
    elapsed = now - lastRefillTime
    tokens += elapsed * refillRate / NS_PER_SEC
    if tokens > maxTokens:
        tokens = maxTokens
    lastRefillTime = now

    # 尝试消费
    if tokens >= bytes:
        tokens -= bytes
        return true   # 允许访问
    else:
        # 带宽不足（模拟阻塞）
        waitTime = (bytes - tokens) / refillRate * NS_PER_SEC  # ns
        tokens = 0
        lastRefillTime = now + waitTime
        return true   # 模拟等待后通过
```

**带宽上限配置（DEFAULT_BANDWIDTH_GBPS = 100.0）：**
- 每秒最多 100 GB 数据传输
- 每个 tile load/store：`32×32×4×4 = 16KB (color) + 32×32×4 = 4KB (depth) = 20KB`
- 300 tiles × 2（load + store）= 600 次访问 ≈ 12 MB × 300 = 12 GB（实际视图元覆盖度）

### 6.2 带宽统计

```cpp
// MemorySubsystem 维护：
double consumedBandwidthGBps;   // 实际消耗带宽（计算得出）
double peakBandwidthUtil;       // 峰值带宽利用率（0~1）

// 计算方式：
// 每帧结束时：
consumedBandwidth = (totalReadBytes + totalWriteBytes) / frameTimeSec
peakBandwidthUtil = max(peakBandwidthUtil, consumedBandwidth / DEFAULT_BANDWIDTH_GBPS)
```

### 6.3 L2 Cache 模拟（简化版）

```
L2 Cache 配置：
- 128 KB total
- 64B cache line
- 256 sets × 8 ways = 128 KB
- 简化 LRU 替换策略

访问流程（readGMEM / writeGMEM）：
    for each cacheLineAddr in addressRange:
        setIndex = (addr / CACHE_LINE_SIZE) % L2_CACHE_SETS
        tag = addr / (CACHE_LINE_SIZE * L2_CACHE_SETS)

        # 查找 set 内是否有匹配 tag 的有效行
        for way in 0..7:
            if lines[setIndex*8 + way].valid && lines[...].tag == tag:
                # HIT
                m_l2Cache.access(addr, isWrite)
                goto done
        # MISS
        # 选择 LRU 行（m_currentTick 最老的）替换
        victimWay = findLRU(setIndex)
        if lines[setIndex*8 + victimWay].dirty:
            # Write-back（简化：直接丢弃，不模拟写回带宽）
            pass
        lines[setIndex*8 + victimWay] = {tag, true, isWrite, m_currentTick}

统计输出：
    - L2 hit rate = hits / (hits + misses)
    - Phase2 仅做统计，不影响带宽计算（带宽模型与 cache 分开）
```

**说明：** L2 Cache 模拟是**教学目的**，Phase2 主要统计命中率，不实际改变行为。真实 GPU 中 cache miss 会导致额外的 latency 和 bandwidth 消耗，Phase3 会建模。

---

## 7. 性能指标

### 7.1 新增计数器总表

| Stage | 计数器 | 类型 | 说明 |
|-------|--------|------|------|
| TilingStage | `triangles_binned` | uint64 | 成功分配到 tile 的 triangle 数 |
| | `tiles_affected` | uint64 | 至少有一个 triangle 覆盖的 tile 数 |
| | `elapsed_ms` | double | Tiling 阶段耗时 |
| TileBuffer | `tile_write_count` | uint64 | 实际执行并写回的 tile 数 |
| | `depth_tests` | uint64 | Tile 内 Z-test 总次数 |
| | `depth_rejects` | uint64 | Z-test 失败次数 |
| | `fragments_shade` | uint64 | 实际执行 FS 的 fragment 数 |
| MemorySubsystem | `read_bytes` | uint64 | GMEM 读总字节数 |
| | `write_bytes` | uint64 | GMEM 写总字节数 |
| | `total_accesses` | uint64 | GMEM 访问次数 |
| | `bandwidth_util` | double | 带宽利用率（0.0~1.0） |
| | `l2_hit_rate` | double | L2 Cache 命中率 |
| Rasterizer | `tile_fragments` | uint64 | Per-tile 光栅化产生的 fragment 数 |

### 7.2 性能报告格式

```
=== PHASE2 TBR Performance Report ===
TilingStage:
  triangles_binned:  1500
  tiles_affected:    87
  elapsed_ms:        0.32

TileBuffer:
  tiles_processed:   87
  fragments_shade:   13456
  depth_tests:       13456
  depth_rejects:     234
  tile_write_count:  87

Rasterizer (per-tile):
  tile_fragments:    13456

MemorySubsystem:
  read_bytes:        1,742,400   (1.66 MB)
  write_bytes:       1,742,400   (1.66 MB)
  total_accesses:    174
  bandwidth_util:    0.05        (5%)
  l2_hit_rate:      0.78        (78%)
=========================================
```

---

## 8. 验收标准

### 8.1 功能验收

| ID | 测试项 | 验收条件 | 量化标准 |
|----|--------|----------|----------|
| F1 | Tiling 正确性 | 全屏三角形被分配到**所有**覆盖的 tile | 手动验证 bin 数量与 tile 覆盖范围一致 |
| F2 | Per-tile 光栅化正确性 | 与 PHASE1 全屏光栅化结果逐像素一致 | dump PPM，对比两版本输出，无像素差异 |
| F3 | Z-test 在 Tile 内正确 | 两个重叠三角形（不同 tile）的前后关系正确 | 屏幕内所有重叠区域显示正确的前景色 |
| F4 | Tile 写回 GMEM | 所有 tile 的 color + depth 正确写回 | GMEM 数据与 TileBuffer 输出一致 |
| F5 | 带宽计数器更新 | `addAccess` 调用后 `read_bytes` / `write_bytes` 正确增加 | 人工验证关键调用点 |
| F6 | L2 Cache 命中率非零 | 正常场景 L2 hit rate > 0（说明 cache 有作用） | 运行 1000 triangle 场景，hit rate > 0.5 |

### 8.2 性能验收

| ID | 测试项 | 目标 | 说明 |
|----|--------|------|------|
| P1 | TBR 开销可接受 | Tiling + Per-tile loop 总耗时 < 2× PHASE1 相同场景 | 确保 TBR 架构不引入过多开销 |
| P2 | 带宽利用率可测 | `bandwidth_util` 有非零值 | 说明带宽模型正常工作 |
| P3 | 小三角形场景带宽低 | 10 个小三角形覆盖 10 tiles，`bandwidth_util` < 0.01 | 验证按需加载 |
| P4 | 大三角形场景带宽高 | 1 个全屏三角形，`bandwidth_util` > 0.5 | 验证全量 load+store |

### 8.3 架构验收

| ID | 测试项 | 验收条件 |
|----|--------|----------|
| A1 | PHASE1 → PHASE2 平滑迁移 | 所有 PHASE1 的 stage 接口保持兼容（`IStage` 不变） |
| A2 | TilingStage 可单独禁用 | 设置环境变量 `SOFTGPU_TILING_DISABLED=1` 时行为退化为 PHASE1（全量 triangles 到全屏 rasterizer） |
| A3 | TileBuffer 独立于 GMEM | Tile 内操作不触发 `m_memory.addAccess` |
| A4 | RenderPipeline 支持 PHASE1 回退 | 单元测试中可单独测试 TilingStage、TileBuffer、MemorySubsystem |

---

## 9. 风险点与应对

### 9.1 技术风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| R1 | Tiling 精度问题（浮点边界） | 中 | 中 | triangle 边界 tile 使用 `ceil/floor` 混合，确保边缘 pixel 不漏计；加入 1-pixel 扩展边界 |
| R2 | TileBuffer 内存占用过高 | 低 | 中 | 当前 300×20KB = 6MB，可接受；超出时改为动态分配，按需创建 |
| R3 | 令牌桶模拟时间与真实时间不同步 | 中 | 低 | 使用墙上时间（`std::chrono`）而非模拟时间戳，带宽模型结果更真实 |
| R4 | L2 Cache 模拟复杂度高 | 高 | 中 | Phase2 仅实现简化版（统计命中率），不参与实际数据路径；行为由令牌桶主导 |

### 9.2 架构风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| A1 | TilingStage 产出数据结构（`vector<index>`）导致 `getTileBin` 返回引用而非拷贝，引发悬挂引用 | 中 | 高 | `TileBin` 存储在 `array` 中（非 `vector`），`getTileBin` 返回 `const TileBin&` 是安全的 |
| A2 | Per-tile loop 中 FragmentShader 需要持有 `TileBufferManager` 引用，耦合紧 | 中 | 低 | Phase2 接受此耦合，Phase3 引入 Command Buffer 解耦 |
| A3 | GMEM 数据布局与 TileBuffer 布局不一致导致 `storeTileToGMEM` 错位 | 中 | 高 | 严格按第 4.3 节公式实现，写单元测试对比 `gmemColor[px,py]` 与 `tileBuffer.color[px,py]` |

### 9.3 测试风险

| ID | 风险描述 | 概率 | 影响 | 应对策略 |
|----|----------|------|------|----------|
| T1 | Per-tile 结果与 PHASE1 全量光栅化结果对比困难 | 高 | 中 | 实现 `FrameDumper` dump 为 PPM，编写 `diff_ppm.py` 脚本逐像素对比 |
| T2 | Tiling 边界 case 多（细长三角形、点、三角形跨 tile 边界） | 高 | 低 | 专项测试用例覆盖：退化三角形、跨 4 tile 边界三角形、全屏三角形 |
| T3 | 带宽利用率不稳定（帧时间波动） | 中 | 低 | 取多帧平均，排除首帧（冷启动），报告 `avg / p50 / p95` |

---

## 附录 A：文件结构（PHASE2 新增/修改）

```
SoftGPU/
├── shared/
│   ├── design/
│   │   ├── PHASE1_DESIGN.md          [保留]
│   │   └── PHASE2_DESIGN.md          ← 本文档
│   ├── include/SoftGPU/
│   │   ├── stages/
│   │   │   ├── IStage.hpp            [保留]
│   │   │   ├── TilingStage.hpp       [NEW]
│   │   │   ├── TileBuffer.hpp        [NEW]
│   │   │   ├── Rasterizer.hpp        [MODIFY: setInputPerTile]
│   │   │   └── FragmentShader.hpp    [MODIFY: TileBuffer output]
│   │   ├── memory/
│   │   │   ├── MemorySubsystem.hpp   [NEW]
│   │   │   └── TokenBucket.hpp       [NEW: 内含于 MemorySubsystem]
│   │   └── pipeline/
│   │       └── RenderPipeline.hpp    [MODIFY: TBR main loop]
│   └── src/
│       ├── TilingStage.cpp           [NEW]
│       ├── TileBuffer.cpp            [NEW]
│       ├── MemorySubsystem.cpp       [NEW]
│       ├── Rasterizer.cpp            [MODIFY]
│       ├── FragmentShader.cpp        [MODIFY]
│       └── RenderPipeline.cpp        [MODIFY: TBR loop]
├── app/
│   └── main.cpp                       [MODIFY: 接入 TBR pipeline]
├── tests/
│   ├── test_TilingStage.cpp          [NEW]
│   ├── test_TileBuffer.cpp           [NEW]
│   ├── test_MemorySubsystem.cpp      [NEW]
│   ├── test_TBR_Integration.cpp      [NEW: 对比 PHASE1 与 TBR 输出]
│   └── test_Framebuffer.cpp          [保留]
└── CMakeLists.txt                    [MODIFY: 新增源文件]
```

---

## 附录 B：关键常量速查

```cpp
namespace SoftGPU {
    // Tile 尺寸
    constexpr uint32_t TILE_WIDTH       = 32;
    constexpr uint32_t TILE_HEIGHT      = 32;
    constexpr uint32_t TILE_SIZE         = 32 * 32;  // 1024

    // Framebuffer
    constexpr uint32_t FRAMEBUFFER_WIDTH  = 640;
    constexpr uint32_t FRAMEBUFFER_HEIGHT = 480;

    // Tile 网格
    constexpr uint32_t NUM_TILES_X      = (640 + 31) / 32;  // 20
    constexpr uint32_t NUM_TILES_Y      = (480 + 31) / 32;  // 15
    constexpr uint32_t NUM_TILES        = 20 * 15;           // 300

    // GMEM 大小（bytes）
    constexpr size_t GMEM_COLOR_SIZE    = NUM_TILES * TILE_SIZE * 4 * sizeof(float);  // ~4.9 MB
    constexpr size_t GMEM_DEPTH_SIZE    = NUM_TILES * TILE_SIZE * sizeof(float);        // ~1.2 MB
    constexpr size_t GMEM_TOTAL_SIZE    = GMEM_COLOR_SIZE + GMEM_DEPTH_SIZE;           // ~6.1 MB

    // LMEM per tile（bytes）
    constexpr size_t LMEM_COLOR_SIZE    = TILE_SIZE * 4 * sizeof(float);  // 16 KB
    constexpr size_t LMEM_DEPTH_SIZE    = TILE_SIZE * sizeof(float);       // 4 KB
    constexpr size_t LMEM_PER_TILE      = LMEM_COLOR_SIZE + LMEM_DEPTH_SIZE; // 20 KB

    // 带宽配置
    constexpr double DEFAULT_BANDWIDTH_GBPS = 100.0;  // 100 GB/s
}
```

---

**文档结束**

*Architect: 陈二虎（@erhu）*  
*SoftGPU Project — Pixel Workshop*  
*Phase2: Tile-Based Rendering Core*
