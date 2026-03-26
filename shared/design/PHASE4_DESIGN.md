# PHASE4 Design Document — SoftGPU 测试场景集 + 调优

**项目名称：** SoftGPU Pixel Workshop
**阶段：** PHASE4 — 测试场景集 + 性能调优
**架构师：** 陈二虎（@erhu）
**日期：** 2026-03-26
**版本：** v1.0
**依赖：** PHASE3（GMEM 带宽模型 + 性能计数器）

---

## 1. 阶段目标

### 1.1 核心目标

PHASE4 聚焦于 GPU 管线的**正确性验证**与**性能调优**，为后续 Real-Time 渲染打好基础：

1. **建立测试场景集**：覆盖各种几何复杂度、材质类型、绘制调用模式的场景
2. **Benchmark 自动化**：一键运行全场景集，输出 CSV 性能报告，跟踪 baseline 变化
3. **性能瓶颈定位与优化**：基于 PHASE3 带宽数据，找出热点并实施 SIMD / 多线程优化

### 1.2 与 PHASE3 的关键差异

| 维度 | PHASE3 | PHASE4 |
|------|--------|--------|
| GMEM 带宽统计 | 仅 print 输出 | CSV 文件 + 自动化对比 |
| 测试场景 | 仅有 1-2 个 hardcoded triangle | 5+ 标准化场景 |
| 测试覆盖 | 无 | 正确性 + 性能双重覆盖 |
| 性能优化 | 无 | SIMD 光栅化 + 可选多线程 |
| Benchmark | 手动运行 | `--benchmark` 自动模式 |

### 1.3 测试场景优先级

| 场景 | 目的 | 难度 |
|------|------|------|
| Triangle-1Tri | 最小正确性验证 | P0 |
| Triangle-Cube | 立方体（12 triangles），验证多边形渲染 | P0 |
| Triangle-Cubes-100 | 100 个小立方体，验证 draw call 批处理 | P1 |
| Triangle-SponzaStyle | Sponza 风格走廊，验证复杂场景 | P1 |
| PBR-Material | PBR 材质验证（albedo, roughness, metallic） | P2 |

---

## 2. 测试场景设计

### 2.1 场景列表管理

**新文件：** `src/test/TestScene.hpp` + `src/test/TestScene.cpp`

```cpp
// ============================================================================
// TestScene - 标准化测试场景
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include "core/RenderCommand.hpp"
#include "core/PipelineTypes.hpp"

namespace SoftGPU {

// 前向声明
struct RenderCommand;

// ============================================================================
// TestScene - 单个测试场景
// ============================================================================
struct TestScene {
    std::string name;               // 场景名称（唯一标识）
    std::string description;        // 场景描述
    std::vector<float> vertices;    // 顶点数据（float 数组）
    std::vector<uint32_t> indices;  // 索引数据（可选）
    uint32_t vertexCount = 0;       // 顶点数
    uint32_t indexCount = 0;        // 索引数
    bool indexed = false;            // 是否使用索引绘制
    std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    // 生成 RenderCommand（便捷方法）
    RenderCommand toRenderCommand() const;
};

// ============================================================================
// TestSceneRegistry - 场景注册表（单例）
// ============================================================================
class TestSceneRegistry {
public:
    static TestSceneRegistry& instance();

    // 注册场景（静态初始化时调用）
    void registerScene(const TestScene& scene);

    // 按名称查找
    const TestScene* findByName(const std::string& name) const;

    // 获取所有场景
    const std::vector<TestScene>& getAllScenes() const { return m_scenes; }

    // 获取场景数量
    size_t size() const { return m_scenes.size(); }

private:
    TestSceneRegistry() = default;
    std::vector<TestScene> m_scenes;
    std::unordered_map<std::string, size_t> m_nameIndex;
};

// ============================================================================
// 内置场景注册宏
// ============================================================================
#define REGISTER_SCENE(_name_, _desc_, _vertices_, _vertexCount_, ...) \
    static void __register_scene_##_name_() { \
        ::SoftGPU::TestSceneRegistry::instance().registerScene({ \
            .name = #_name_, \
            .description = _desc_, \
            .vertices = _vertices_, \
            .vertexCount = _vertexCount_, \
            ##__VA_ARGS__ \
        }); \
    }

}  // namespace SoftGPU
```

### 2.2 内置测试场景详细设计

#### 场景 1：Triangle-1Tri（单三角形）

**用途：** 最基础正确性验证，确保 TBR 管线路径畅通。

```cpp
// 简单绿色三角形（屏幕中心偏上）
TestScene scene_Triangle_1Tri = {
    .name = "Triangle-1Tri",
    .description = "单绿色三角形，验证最小 TBR 路径",
    .vertices = {
        // v0: 顶部
         0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        // v1: 左下
        -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        // v2: 右下
         0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    },
    .vertexCount = 3,
    .indexed = false,
    .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}
};
```

**验证点：**
- [ ] 绿色像素出现在预期位置
- [ ] `read_bytes > 0 && write_bytes > 0`
- [ ] L2 cache hit rate 可被查询

---

#### 场景 2：Triangle-Cube（立方体）

**用途：** 验证 12 triangles（2 per face × 6 faces）的背面剔除和多边形渲染。

```cpp
// 立方体顶点（8 corners × 8 floats = 64 floats）
// 每个面 2 triangles，合计 12 unique triangles
// 位置: 中心偏移 (0, 0, 0.5)，边长 0.8
//
//    v4 -------- v5
//   /|          /|
//  / |         / |
// v0 -------- v1  |
// |  v7 ------|-v6
// | /         |/
// |/          |
// v3 -------- v2

TestScene scene_Triangle_Cube = {
    .name = "Triangle-Cube",
    .description = "12-triangle 立方体，验证多边形渲染 + 背面剔除",
    .vertices = {
        // v0 (0): -0.4, +0.4, +0.5  white
        -0.4f,  0.4f,  0.5f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        // v1 (1): +0.4, +0.4, +0.5  white
         0.4f,  0.4f,  0.5f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        // v2 (2): +0.4, -0.4, +0.5  white
         0.4f, -0.4f,  0.5f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        // v3 (3): -0.4, -0.4, +0.5  white
        -0.4f, -0.4f,  0.5f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        // v4 (4): -0.4, +0.4, +0.9  light-gray
        -0.4f,  0.4f,  0.9f, 1.0f,   0.8f, 0.8f, 0.8f, 1.0f,
        // v5 (5): +0.4, +0.4, +0.9  light-gray
         0.4f,  0.4f,  0.9f, 1.0f,   0.8f, 0.8f, 0.8f, 1.0f,
        // v6 (6): +0.4, -0.4, +0.9  light-gray
         0.4f, -0.4f,  0.9f, 1.0f,   0.8f, 0.8f, 0.8f, 1.0f,
        // v7 (7): -0.4, -0.4, +0.9  light-gray
        -0.4f, -0.4f,  0.9f, 1.0f,   0.8f, 0.8f, 0.8f, 1.0f,
    },
    .vertexCount = 8,
    // 12 triangles: front(0,1,2,0,2,3), back(4,6,5,4,7,6), top(4,5,1,4,1,0),
    //               bottom(3,2,6,3,6,7), right(1,5,6,1,6,2), left(4,0,3,4,3,7)
    .indices = {
        0,1,2, 0,2,3,   // front
        4,6,5, 4,7,6,   // back
        4,5,1, 4,1,0,   // top
        3,2,6, 3,6,7,   // bottom
        1,5,6, 1,6,2,   // right
        4,0,3, 4,3,7,   // left
    },
    .indexCount = 36,
    .indexed = true,
    .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}
};
```

**验证点：**
- [ ] 6 个可见面颜色正确（前向面 white，侧面 light-gray）
- [ ] 背面剔除生效（背面不渲染）
- [ ] Z-buffer 正确处理面片遮挡

---

#### 场景 3：Triangle-Cubes-100（100 小立方体）

**用途：** 验证批量 draw call 性能，测试 TilingStage 对大量三角形的 binning 效率。

```cpp
TestScene scene_Triangle_Cubes_100 = {
    .name = "Triangle-Cubes-100",
    .description = "100 个小立方体（1200 triangles），验证批量渲染性能",
    .vertexCount = 0,    // 动态生成
    .indexCount = 0,    // 动态生成
    .indexed = true,
    .clearColor = {0.05f, 0.05f, 0.1f, 1.0f},
    // vertices / indices 由 TestSceneBuilder::buildCubes100() 动态生成
};
```

**动态生成规则：**
- 网格排列：10 × 10 = 100 个小立方体
- 每个立方体边长 0.08，间距 0.12
- 位置范围：X ∈ [-0.55, +0.55]，Y ∈ [-0.55, +0.55]，Z = 0.6（固定深度）
- 颜色：按位置 hash 成不同 RGB（马赛克效果）
- 总三角形数：100 × 12 = **1200 triangles**

**验证点：**
- [ ] 100 个独立立方体可见
- [ ] TilingStage binning 时间 < 5ms
- [ ] GMEM 带宽利用率可接受（< 80%）

---

#### 场景 4：Triangle-SponzaStyle（Sponza 风格场景）

**用途：** 模拟 Sponza 大厅的典型几何复杂度（拱门、柱子、走廊），为后续真实 Sponza 模型加载打基础。

```cpp
TestScene scene_Triangle_SponzaStyle = {
    .name = "Triangle-SponzaStyle",
    .description = "Sponza 风格走廊场景，验证复杂场景 binning",
    .vertexCount = 0,    // 动态生成
    .indexCount = 0,
    .indexed = true,
    .clearColor = {0.02f, 0.02f, 0.04f, 1.0f},
    // 由 TestSceneBuilder::buildSponzaStyle() 生成
};
```

**场景结构（简化版 Sponza）：**

| 物体 | 三角形数 | 颜色 |
|------|----------|------|
| Floor（地板） | 2 | 暖灰色 |
| Ceiling（天花板） | 2 | 深灰色 |
| Left wall（左侧墙） | 2 | 暖黄色 |
| Right wall（右侧墙） | 2 | 暖黄色 |
| Front wall with arch（拱门墙） | 10 | 暖黄色 |
| Back wall | 2 | 暖黄色 |
| Pillars × 4（柱子） | 8 × 4 = 32 | 金色 |
| Hanging banners × 6 | 4 × 6 = 24 | 深红色 |
| **总计** | **~80 triangles** | — |

**验证点：**
- [ ] 拱门形状正确（arch geometry）
- [ ] 柱子与墙面颜色区分明显
- [ ] TilingStage 正确处理跨 tile 三角形

---

#### 场景 5：PBR-Material（PBR 材质验证）

**用途：** 验证 PBR（Physically Based Rendering）材质参数的解析与插值，为后续 PBR 着色打好基础。

```cpp
// PBR 材质参数（扩展的 per-vertex 数据）
struct PBRVertex {
    float x, y, z, w;        // Position
    float r, g, b, a;        // Base color (albedo)
    float metallic;          // 金属度（per-vertex，简化）
    float roughness;          // 粗糙度（per-vertex，简化）
    float padding[2];        // 对齐到 8 floats stride
};

TestScene scene_PBR_Material = {
    .name = "PBR-Material",
    .description = "PBR 材质验证：metal sphere + rough sphere + plastic",
    // vertices: 3 spheres，每个 3 vertices (simplified)
    // Sphere 0: metallic=1.0, roughness=0.1 (gold ball)
    // Sphere 1: metallic=0.0, roughness=0.8 (plastic ball)
    // Sphere 2: metallic=0.5, roughness=0.5 (mixed)
    .vertexCount = 0,
    .indexCount = 0,
    .indexed = true,
    .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
};
```

**验证点：**
- [ ] Fragment shader 能读取 metallic / roughness 参数
- [ ] 三种材质视觉效果可区分（即使只是 flat shading）
- [ ] 材质数据正确从 vertex 传递到 fragment（插值验证）

---

### 2.3 TestSceneBuilder 动态场景生成器

**新文件：** `src/test/TestSceneBuilder.hpp` + `src/test/TestSceneBuilder.cpp`

负责动态生成复杂场景（Cube-100、SponzaStyle 等），避免 hardcoded 大量顶点数据。

```cpp
// ============================================================================
// TestSceneBuilder - 动态场景生成器
// ============================================================================
class TestSceneBuilder {
public:
    // 生成 100 个小立方体场景
    static TestScene buildCubes100();

    // 生成 Sponza 风格走廊场景
    static TestScene buildSponzaStyle();

    // 生成 PBR 材质测试场景
    static TestScene buildPBRMaterial();

    // 生成指定数量的立方体网格
    // count: 总立方体数（支持 1, 10, 100, 1000）
    static TestScene buildCubeGrid(uint32_t count);
};
```

---

## 3. Benchmark 设计

### 3.1 `--benchmark` 命令行模式

**修改文件：** `src/main.cpp`（或新增 `src/benchmark/BenchmarkMain.cpp`）

```bash
# 基础 benchmark（使用默认场景集）
./SoftGPU --benchmark

# 指定场景子集
./SoftGPU --benchmark --scenes=Triangle-1Tri,Triangle-Cube

# 指定输出 CSV 文件
./SoftGPU --benchmark --output=results.csv

# 指定运行次数（取平均值）
./SoftGPU --benchmark --runs=5

# 完整用法
./SoftGPU --benchmark \
    --scenes=ALL \
    --runs=3 \
    --output=build/benchmark_results.csv \
    --compare-to=baseline.csv
```

### 3.2 BenchmarkRunner 核心模块

**新文件：** `src/benchmark/BenchmarkRunner.hpp` + `src/benchmark/BenchmarkRunner.cpp`

```cpp
// ============================================================================
// BenchmarkRunner - 自动化性能测试运行器
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "test/TestScene.hpp"
#include "pipeline/RenderPipeline.hpp"

namespace SoftGPU {

// ============================================================================
// BenchmarkResult - 单次运行结果
// ============================================================================
struct BenchmarkResult {
    std::string sceneName;
    uint32_t trianglesRendered = 0;
    uint32_t tilesAffected = 0;
    uint64_t fragmentsOutput = 0;

    // 带宽数据
    uint64_t gmemReadBytes = 0;
    uint64_t gmemWriteBytes = 0;
    uint64_t gmemTotalBytes = 0;
    double   bandwidthUtilization = 0.0;  // 0.0 ~ 1.0
    double   l2HitRate = 0.0;            // 0.0 ~ 1.0

    // 各阶段耗时（ms）
    double   totalTimeMs = 0.0;
    double   commandProcessorMs = 0.0;
    double   vertexShaderMs = 0.0;
    double   primitiveAssemblyMs = 0.0;
    double   tilingStageMs = 0.0;
    double   rasterizerMs = 0.0;
    double   fragmentShaderMs = 0.0;
    double   tileWriteBackMs = 0.0;
    double   gmemSyncMs = 0.0;           // syncGMEMToFramebuffer

    // 每帧带宽（MB/s）
    double   effectiveBandwidthMBps = 0.0;
};

// ============================================================================
// BenchmarkConfig - Benchmark 配置
// ============================================================================
struct BenchmarkConfig {
    std::vector<std::string> scenes;    // 场景名列表（空=全部）
    uint32_t runsPerScene = 3;          // 每个场景运行次数
    std::string outputCSV;              // 输出 CSV 路径
    std::string baselineCSV;            // 对比的 baseline CSV
    bool verbose = false;               // 是否打印详细日志
};

// ============================================================================
// BenchmarkRunner - 自动化测试运行器
// ============================================================================
class BenchmarkRunner {
public:
    BenchmarkRunner() = default;

    // 运行 benchmark
    std::vector<BenchmarkResult> run(const BenchmarkConfig& config);

    // 输出 CSV
    void exportCSV(const std::vector<BenchmarkResult>& results,
                   const std::string& filename) const;

    // 与 baseline 对比
    void compareToBaseline(const std::vector<BenchmarkResult>& results,
                           const std::string& baselineCSV,
                           const std::string& outputDiffCSV) const;

    // 打印摘要报告
    void printSummary(const std::vector<BenchmarkResult>& results) const;

private:
    // 运行单次场景
    BenchmarkResult runOnce(const TestScene& scene);

    // 解析场景名称列表（支持 "ALL" 关键字）
    std::vector<const TestScene*> resolveSceneList(
        const std::vector<std::string>& names) const;
};

}  // namespace SoftGPU
```

### 3.3 CSV 输出格式

**输出文件：** `build/benchmark_results.csv`

```csv
scene,triangles,tiles_affected,fragments,gmem_read_bytes,gmem_write_bytes,gmem_total_bytes,bandwidth_util,l2_hit_rate,total_time_ms,vs_time_ms,tiling_time_ms,raster_time_ms,fs_time_ms,total_bandwidth_mbps
Triangle-1Tri,1,4,128,81920,81920,163840,0.0016,0.00,0.312,0.001,0.002,0.180,0.120,0.52
Triangle-Cube,12,18,2048,376320,376320,752640,0.0075,0.00,1.234,0.008,0.015,0.890,0.280,0.61
Triangle-Cubes-100,1200,280,152000,5767200,5767200,11534400,0.1153,0.23,28.450,0.092,0.180,22.100,5.800,0.41
Triangle-SponzaStyle,80,195,48000,4019200,4019200,8038400,0.0804,0.18,12.300,0.045,0.090,9.800,2.100,0.65
PBR-Material,3,9,3072,184320,184320,368640,0.0037,0.00,0.890,0.005,0.008,0.650,0.200,0.41
```

**CSV 列说明：**

| 列名 | 说明 |
|------|------|
| `scene` | 场景名称 |
| `triangles` | 渲染的三角形总数 |
| `tiles_affected` | 被三角形覆盖的 tile 数 |
| `fragments` | 光栅化产生的 fragment 总数 |
| `gmem_read_bytes` | GMEM 读取总字节数 |
| `gmem_write_bytes` | GMEM 写入总字节数 |
| `gmem_total_bytes` | GMEM 总带宽（读+写） |
| `bandwidth_util` | 带宽利用率（0.0 ~ 1.0） |
| `l2_hit_rate` | L2 cache 命中率（0.0 ~ 1.0） |
| `total_time_ms` | 总帧时间（ms） |
| `vs_time_ms` | Vertex Shader 耗时 |
| `tiling_time_ms` | Tiling Stage 耗时 |
| `raster_time_ms` | Rasterizer 耗时 |
| `fs_time_ms` | Fragment Shader 耗时 |
| `total_bandwidth_mbps` | 有效带宽（MB/s） |

### 3.4 Baseline 对比功能

`--compare-to=baseline.csv` 会在每次运行后输出变更百分比：

```csv
scene,metric,baseline,current,delta_pct
Triangle-1Tri,total_time_ms,0.312,0.298,-4.49%
Triangle-1Tri,bandwidth_util,0.0016,0.0015,-6.25%
Triangle-Cube,total_time_ms,1.234,1.180,-4.38%
...
```

---

## 4. 性能优化方向

### 4.1 瓶颈分析（基于 PHASE3 预期数据）

**PHASE3 带宽模型已知数据：**

| 操作 | 字节数/tile | 说明 |
|------|-------------|------|
| Color load | 16 KB | TILE_SIZE × 4 floats |
| Depth load | 4 KB | TILE_SIZE × 1 float |
| Color store | 16 KB | 同上 |
| Depth store | 4 KB | 同上 |
| **每 tile 合计** | **40 KB** | load+store（color+depth）|

**PHASE3 场景预期热点：**

1. **Rasterizer（最耗时）**：`Triangle-Cubes-100` 场景下 22ms/frame（1200 triangles）
   - 原因：逐-fragment 扫描填充，无 SIMD
2. **Fragment Shader**：`Triangle-SponzaStyle` 场景下 2ms/frame
   - 原因：逐 fragment 执行，无批处理
3. **TileWriteBack**：`Triangle-Cubes-100` 场景下 300 tiles × 40KB = 12MB memcpy
   - 原因：每 tile 均需 load + store

### 4.2 优化 1：SIMD 化 Rasterizer

**目标：** 将 2D 三角形扫描从逐像素循环改为 SIMD 向量化（32 pixels/cycle）。

**文件：** `src/stages/Rasterizer_SIMD.hpp` + `src/stages/Rasterizer_SIMD.cpp`

```cpp
// ============================================================================
// RasterizerSIMD - SIMD 加速的光栅化器
// 使用 SSE/AVX 对 8/16/32 个像素并行光栅化
// ============================================================================
class RasterizerSIMD {
public:
    // 批量光栅化一个 triangle，覆盖 tile 内像素
    // pack: 8/16/32 像素同时处理
    void rasterizeTriangle_SIMD(const Triangle& tri,
                                uint32_t tileX, uint32_t tileY,
                                TileBuffer& tileBuffer);

private:
    // SIMD 边界测试（同时测试 8 个像素的 inside/outside）
    __m256 testBounds_SIMD(__m256 px, __m256 py,
                           const Triangle& tri);

    // SIMD 深度测试 + 写入
    void depthTestAndStore_SIMD(__m256i px, __m256i py, __m256 pz,
                                const Fragment& frag,
                                TileBuffer& tileBuffer);
};
```

**预期提升：** Rasterizer 阶段 **2-4× 加速**（取决于 SIMD 宽度）。

### 4.3 优化 2：Fragment Shader 批处理

**目标：** 将多个 fragment 的着色合并为一次批量处理，减少函数调用开销。

```cpp
// ============================================================================
// FragmentShader::executeBatch - 批处理多个 fragment
// 输入：最多 BATCH_SIZE=64 fragments
// 输出：批量插值 + 颜色计算
// ============================================================================
void FragmentShader::executeBatch(const std::vector<Fragment>& batch,
                                  uint32_t tileX, uint32_t tileY) {
    // 批量插值计算（SIMD 化）
    for (const auto& frag : batch) {
        Fragment shaded = shade(frag);  // 仍是逐 fragment，但减少循环开销
        writeToTileBuffer(shaded);
    }
}
```

**注意：** PHASE4 阶段 Fragment Shader 主要瓶颈是逐像素循环，批处理只是减少函数调用开销。真正的 SIMD 化在 PHASE5 实现。

### 4.4 优化 3：TileWriteBack 异步化（可选）

**目标：** 使用 `std::thread` 将 TileWriteBack 的 GMEM memcpy 与下一帧的渲染并行。

```cpp
// RenderPipeline 新增成员
std::thread m_tileWriteBackThread;
std::atomic<bool> m_writeBackRunning{false};

// Per-tile 异步写回（生产者-消费者模式）
void RenderPipeline::executeTile_Async(uint32_t tileIndex, ...) {
    // ... rasterize + shade ...

    // 异步写回：不阻塞当前帧
    enqueueTileStore(tileIndex, tileBuffer);

    // 如果后台线程空闲，则触发
    if (!m_writeBackRunning) {
        m_writeBackRunning = true;
        m_tileWriteBackThread = std::thread([this]() {
            processTileStoreQueue();  // 批量处理队列中的 tile
            m_writeBackRunning = false;
        });
    }
}
```

**注意：** PHASE4 阶段不强制要求多线程化，仅作为可选优化方向。如果 TBR 帧时间已达到 60 FPS 目标，可跳过此优化。

### 4.5 优化 4：TilingStage 加速

**目标：** 将三角形 binning 从逐 tile 遍历改为空间哈希加速。

**当前实现（O(tiles × triangles)）：**
```cpp
for (each tile):
    for (each triangle):
        if (triangle overlaps tile):
            bin.addTriangle(triangle)
```

**优化后（O(triangles)）：**
```cpp
// 每个 triangle 只需检查它覆盖的 tile（4-6 个 tile）
for (each triangle):
    coveredTiles = getCoveredTiles(triangle)  // AABB 裁剪
    for (each tile in coveredTiles):
        bin.addTriangle(triangle)
```

**预期提升：** TilingStage 阶段 **5-10× 加速**（从 0.18ms → 0.02ms for 1200 triangles）。

---

## 5. 验收标准

### 5.1 测试场景验收

| 场景 | 验证点 | 通过条件 |
|------|--------|----------|
| Triangle-1Tri | 绿色像素出现在预期位置 | 中心 ±5 pixel 内有 G > 0.5 |
| Triangle-1Tri | 带宽数据非零 | `read_bytes > 0 && write_bytes > 0` |
| Triangle-Cube | 6 个可见面色值正确 | 前向面白，侧面浅灰 |
| Triangle-Cube | 背面剔除 | 背面无像素写入（Z=far plane） |
| Triangle-Cubes-100 | 100 个独立立方体可见 | 像素级计数 ≈ 100 × visible faces |
| Triangle-Cubes-100 | TilingStage 耗时 < 5ms | `tiling_time_ms < 5.0` |
| Triangle-SponzaStyle | 拱门形状正确 | arch 区域可见通透（无墙面遮挡） |
| Triangle-SponzaStyle | 柱子可见 | 4 根柱子独立存在 |
| PBR-Material | 三种材质可区分 | metal（高光）/ rough（漫反射）色彩不同 |

### 5.2 Benchmark 验收

| 指标 | 目标 |
|------|------|
| `--benchmark` 不崩溃 | 正常运行完全部场景 |
| CSV 输出完整 | 所有场景均出现在 CSV 中 |
| CSV 数据有效 | 无 NaN，无负数，无 > 100% 的 utilization |
| Baseline 对比 | `--compare-to=baseline.csv` 生成 diff 报告 |
| 重复性 | 同一场景 3 次运行，`total_time_ms` 差异 < 5% |

### 5.3 性能验收（优化后）

| 场景 | PHASE3 基准 | PHASE4 目标 | 提升比例 |
|------|-------------|-------------|----------|
| Triangle-1Tri | ~0.31 ms | ~0.25 ms | **≥ 15%** |
| Triangle-Cube | ~1.23 ms | ~0.90 ms | **≥ 25%** |
| Triangle-Cubes-100 | ~28.5 ms | ~15.0 ms | **≥ 45%** |
| Triangle-SponzaStyle | ~12.3 ms | ~7.0 ms | **≥ 40%** |

**注意：** PHASE4 优化以 Rasterizer SIMD + TilingStage 加速为主。Triangle-Cubes-100 场景受益最大（1200 triangles → Rasterizer 热点最突出）。

### 5.4 整体验收

- [ ] `TestSceneRegistry` 可通过 `findByName()` 找到所有 5 个内置场景
- [ ] `BenchmarkRunner::run()` 返回的 `BenchmarkResult` 数量 = 场景数 × runsPerScene
- [ ] CSV 文件可被 Excel / Google Sheets 正确打开（UTF-8 + comma 分隔）
- [ ] `--benchmark --scenes=Triangle-1Tri` 单独运行成功
- [ ] Rasterizer SIMD 实现编译通过（需要 SIMD intrinsics 支持）
- [ ] `printPerformanceReport()` 在 benchmark 模式下输出到 stderr（非干扰 CSV）
- [ ] 优化后 Triangle-Cubes-100 场景 **帧时间 < 15ms**（即 60+ FPS）

---

## 附录：修改文件清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/test/TestScene.hpp` | TestScene + TestSceneRegistry 定义 |
| `src/test/TestScene.cpp` | TestSceneRegistry 实现 + 内置场景注册 |
| `src/test/TestSceneBuilder.hpp` | 动态场景生成器声明 |
| `src/test/TestSceneBuilder.cpp` | 动态场景生成器实现（Cube-100、Sponza 等） |
| `src/benchmark/BenchmarkRunner.hpp` | BenchmarkRunner 声明 |
| `src/benchmark/BenchmarkRunner.cpp` | BenchmarkRunner 实现 |
| `src/benchmark/BenchmarkResult.hpp` | BenchmarkResult 数据结构 |
| `src/stages/Rasterizer_SIMD.hpp` | SIMD 光栅化器声明（可选） |
| `src/stages/Rasterizer_SIMD.cpp` | SIMD 光栅化器实现（可选） |

### 修改文件

| 文件 | 修改类型 |
|------|----------|
| `src/main.cpp` | 新增 `--benchmark` 命令行解析 |
| `src/CMakeLists.txt` | 新增 `test/` 和 `benchmark/` 子目录 |
| `tests/stages/CMakeLists.txt` | 新增 `test_TestScenes` 可执行文件 |
| `tests/stages/test_TestScenes.cpp` | TestScene + TestSceneBuilder 单元测试 |
| `tests/stages/test_Integration.cpp` | 新增 5 个场景的集成测试 |
| `src/core/PipelineTypes.hpp` | 新增 `PBRMaterial` 数据结构（可选） |

### 目录结构（PHASE4 完成后）

```
src/
├── test/                    # NEW: 测试场景
│   ├── CMakeLists.txt
│   ├── TestScene.hpp
│   ├── TestScene.cpp
│   ├── TestSceneBuilder.hpp
│   └── TestSceneBuilder.cpp
├── benchmark/               # NEW: Benchmark 自动化
│   ├── CMakeLists.txt
│   ├── BenchmarkRunner.hpp
│   ├── BenchmarkRunner.cpp
│   └── BenchmarkResult.hpp
├── stages/
│   ├── Rasterizer_SIMD.hpp  # NEW: 可选 SIMD 优化
│   └── Rasterizer_SIMD.cpp
└── ...
```

---

## 附录：命令行接口设计（详细）

### `--benchmark` 完整帮助

```
$ ./SoftGPU --benchmark --help
Usage: ./SoftGPU [OPTIONS]

Benchmark Options:
  --benchmark              启用 benchmark 模式（运行测试场景集）
  --scenes=NAME1,NAME2     指定运行场景（逗号分隔，默认 ALL）
  --runs=N                  每个场景运行次数（默认 3）
  --output=FILE             输出 CSV 文件（默认 build/benchmark_results.csv）
  --compare-to=FILE         与历史 baseline 对比
  --no-dump                 禁用 PPM dump（加速 benchmark）
  --verbose                 打印详细日志
  --help                    显示本帮助

Available Scenes:
  Triangle-1Tri            单三角形（正确性验证）
  Triangle-Cube            立方体（12 triangles）
  Triangle-Cubes-100       100 小立方体（1200 triangles）
  Triangle-SponzaStyle     Sponza 风格走廊（~80 triangles）
  PBR-Material              PBR 材质验证（3 spheres）

Examples:
  ./SoftGPU --benchmark                           # 运行全场景集
  ./SoftGPU --benchmark --scenes=Triangle-1Tri   # 仅运行单三角形
  ./SoftGPU --benchmark --runs=5 --verbose       # 5 次运行 + 详细日志
  ./SoftGPU --benchmark --compare-to=base.csv    # 与 baseline 对比
```
