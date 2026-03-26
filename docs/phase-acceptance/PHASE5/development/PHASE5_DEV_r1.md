# PHASE5_DEV_r1 — 性能分析和瓶颈判定模块开发报告

**日期:** 2026-03-26
**开发者:** 白小西 (@xiaoxi)
**阶段:** PHASE5 — 性能分析和瓶颈判定
**代码审查状态:** ✅ 编译通过

---

## 1. 实现概览

### 新增文件

| 文件路径 | 说明 |
|---|---|
| `src/profiler/IProfiler.hpp` | 性能分析器接口定义 |
| `src/profiler/FrameProfiler.hpp/cpp` | 单例性能分析器实现 |
| `src/profiler/BottleneckDetector.hpp/cpp` | 瓶颈分析器 |
| `src/profiler/ProfilerUI.hpp/cpp` | ImGui 可视化面板 |
| `src/profiler/CMakeLists.txt` | profiler 子模块构建配置 |

### 修改文件

| 文件路径 | 说明 |
|---|---|
| `src/pipeline/RenderPipeline.hpp` | 集成 profiler 和 BottleneckDetector |
| `src/pipeline/RenderPipeline.cpp` | 在 render() 中插桩 beginFrame/endFrame + beginStage/endStage |
| `src/pipeline/CMakeLists.txt` | 添加 profiler 链接依赖 |
| `src/CMakeLists.txt` | 添加 profiler 子目录 |

---

## 2. 核心设计

### 2.1 IProfiler 接口

```cpp
enum class StageHandle {
    CommandProcessor, VertexShader, PrimitiveAssembly,
    Rasterizer, FragmentShader, Framebuffer, TileWriteBack, Tiling
};

struct ProfilerStats {
    uint64_t invocations;
    uint64_t cycles;
    double   ms;
    double   percent;
};

class IProfiler {
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void beginStage(StageHandle) = 0;
    virtual void endStage(StageHandle) = 0;
    virtual ProfilerStats getStats(StageHandle) const = 0;
    virtual BottleneckResult detectBottleneck() const = 0;
    virtual double getFrameTimeMs() const = 0;
    virtual double getFps() const = 0;
    virtual void reset() = 0;
};
```

### 2.2 FrameProfiler (单例)

- **TimestampCollector**: 记录每个 stage 的 entry/exit 时间戳 (nanosecond resolution)
- **Aggregator**: 维护 60 帧滑动窗口，计算各 stage 的平均 ms、百分比、FPS
- 每帧 `beginFrame()` 开启，`endFrame()` 关闭并推送数据到 Aggregator
- RAII `ProfilerGuard` 作用域守卫确保 begin/end 配对

### 2.3 BottleneckDetector (三维度评分)

| 瓶颈类型 | 判断条件 |
|---|---|
| **Shader Bound** | `fs_ratio > 70% && core_util < 50%` |
| **Memory Bound** | `bw_util > 85% && core_util < 70%` |
| **Fill Rate Bound** | `raster_eff < 30%` |

MetricsCollector 从 RenderPipeline 收集：
- 带宽利用率 (MemorySubsystem)
- 光栅器效率 (fragments / viewport_pixels)
- 核心利用率 (1.0 - bw_util * 0.5)
- Fragment shader 时间占比

### 2.4 ProfilerUI (ImGui 面板)

- 顶部：FPS 和帧时间摘要
- 瓶颈指示器：类型 + 置信度 + 严重度 + 推荐方案
- Pipeline 架构图：各 stage 颜色编码 (绿 <50% <黄 <80% <红)
- 时间分解柱状图（堆叠条形图 + 图例）
- 详细统计表格（Invocation / Cycles / Time / %）
- 瓶颈评分（Shader / Memory / FillRate / Compute Bound 各维度分值）

---

## 3. 集成方式

RenderPipeline::render() 中插桩：

```cpp
// 每帧边界
FrameProfiler::get().beginFrame();
// 各阶段
FrameProfiler::get().beginStage(StageHandle::CommandProcessor);
m_commandProcessor.execute();
FrameProfiler::get().endStage(StageHandle::CommandProcessor);
// ... 各 stage 类似
FrameProfiler::get().endFrame();
updateBottleneckMetrics(command);  // 更新瓶颈指标
```

TBR 模式下，Rasterizer + FragmentShader 的时间覆盖整个 per-tile 循环。

---

## 4. 构建验证

```
[100%] Built target profiler
[100%] Built target pipeline
```

- ✅ `profiler` 库编译成功 (FrameProfiler.cpp, BottleneckDetector.cpp, ProfilerUI.cpp)
- ✅ `pipeline` 库编译成功 (RenderPipeline.cpp 含 profiler 插桩)
- ⚠️ `platform` / `glad` 存在预存编译问题 (GLFW 宏定义冲突)，与本阶段无关
- ⚠️ 链接测试因 GLFW 库缺失失败，为预存问题

---

## 5. 已知限制

1. **TBR per-tile 统计**: Rasterizer 和 FragmentShader 的 begin/end 包裹整个 per-tile 循环，而非单个 tile。目的是减少 begin/end 调用开销（每帧仅 2 次而非 300×2 次）。
2. **Fragment shader ratio**: 在 BottleneckDetector 中由 FrameProfiler 直接计算，通过 RenderPipeline 注入帧时间比。
3. **Core utilization**: 采用简化模型 `1.0 - bw_util * 0.5`，后续可接入真实硬件 PMU 数据。
4. **Framebuffer stage (TBR 模式)**: TBR 中 Framebuffer 实际执行时间为 0（渲染至 GMEM），此阶段在 TBR 模式下不会累积时间。

---

## 6. Git Commit

```bash
git add src/profiler/ src/pipeline/RenderPipeline.hpp \
        src/pipeline/RenderPipeline.cpp src/pipeline/CMakeLists.txt \
        src/CMakeLists.txt
git commit -m "PHASE5: Add performance profiler and bottleneck detector

- IProfiler interface with StageHandle enum (8 pipeline stages)
- FrameProfiler singleton with TimestampCollector and Aggregator
- 60-frame rolling average for FPS and per-stage ms/percent
- BottleneckDetector with 3-dimension scoring (Shader/Memory/FillRate)
- ProfilerUI ImGui panel with color-coded pipeline diagram
- Bottleneck arrow annotation and recommendation strings
- RenderPipeline::render() instrumented with beginFrame/endFrame
- Integration: MetricsCollector updated each frame from MemorySubsystem
"
```

---

## 7. 下一步 (可选优化)

1. **Rasterizer_SIMD**: 对 `edgeFunction` 批量计算使用 SIMD 向量化 (SSE/NEON)
2. **TilingStage O(tri) 优化**: 将 triangle-to-tile 映射从 O(tri × tiles) 降至 O(tri × affected_tiles_per_tri)
3. **PMU 集成**: 接入真实硬件性能计数器替代软件模拟的核心利用率
4. **GPU 时间戳**: 使用 OpenGL timer query 获取 GPU 侧真实时间
