# PHASE5 Design Document — SoftGPU 性能分析与瓶颈判定

**项目名称：** SoftGPU Pixel Workshop
**阶段：** PHASE5 — Profiler 模块 + 瓶颈判定 + 性能可视化 + 优化
**架构师：** 陈二虎（@erhu）
**日期：** 2026-03-26
**版本：** v1.0
**依赖：** PHASE4（测试场景集 + Benchmark + SIMD Rasterizer 预备）

---

## 1. 阶段目标

### 1.1 核心目标

PHASE5 聚焦于 GPU 管线的**性能可观测性**与**瓶颈自动判定**：

1. **Profiler 模块**：实时收集各 Stage 的时间戳、调用计数、带宽数据，每帧聚合输出
2. **BottleneckDetector**：基于性能数据自动判定瓶颈类型（Shader Bound / Memory Bound / Fill Rate Bound）
3. **ProfilerUI**：ImGui 可视化面板（架构图颜色标注、时间分解柱状图、瓶颈箭头）
4. **针对性优化**：基于瓶颈分析，实施 SIMD 光栅化加速 + TilingStage 优化

### 1.2 与 PHASE4 的关键差异

| 维度 | PHASE4 | PHASE5 |
|------|--------|--------|
| 性能数据 | 仅 CSV 事后分析 | 实时帧级 Profiler |
| 瓶颈判定 | 人工读 CSV | BottleneckDetector 自动判定 |
| 可视化 | 无 | ImGui ProfilerUI 面板 |
| Rasterizer | scalar 逐像素 | SIMD 化（可选） |
| TilingStage | O(tri×tiles) 遍历 | O(tri) 空间哈希优化 |

### 1.3 瓶颈判定规则

```
Shader Bound:     Fragment Time / Total Time > 70% && Core Utilization < 50%
Memory Bound:     GMEM Bandwidth > 85% && Core Utilization < 70%
Fill Rate Bound:  Raster Output Fragments < (Theoretical Peak) × 30%
No Bottleneck:    以上均不满足
```

---

## 2. 架构总览

### 2.1 模块依赖关系

```
RenderPipeline
    │
    ├── IProfiler (interface)
    │       │
    │       └── FrameProfiler (singleton, collects per-frame data)
    │               │
    │               ├── TimestampCollector (per-stage timestamps)
    │               ├── Aggregator         (frame-level stats)
    │               └── BottleneckDetector  (rule-based classification)
    │
    ├── ProfilerUI (ImGui panel, reads from FrameProfiler)
    │
    └── 各 Stage (IStage) — 注入 Profiler hook
            ├── CommandProcessor
            ├── VertexShader
            ├── PrimitiveAssembly
            ├── TilingStage
            ├── Rasterizer / RasterizerSIMD
            ├── FragmentShader
            └── TileWriteBack
```

### 2.2 数据流

```
每帧 RenderPipeline::render() 调用
    │
    ├─ [Pre-Frame]   FrameProfiler::beginFrame()
    │
    ├─ [Per-Stage]   Stage.execute() + TimestampCollector::stamp(stage, begin/end)
    │
    ├─ [Per-Tile]    Rasterizer + FragmentShader + TileWriteBack
    │
    ├─ [Post-Frame]  FrameProfiler::endFrame()
    │                       │
    │                       ├─ Aggregator::compute() → FrameStats
    │                       ├─ BottleneckDetector::detect() → BottleneckType
    │                       └─ ProfilerUI::render(stats)  → ImGui Draw Calls
    │
    └─ [Next Frame]  stats available for ProfilerUI render
```

---

## 3. IProfiler 接口设计

### 3.1 接口定义

**文件：** `src/profiler/IProfiler.hpp`

```cpp
// ============================================================================
// IProfiler - 性能分析器接口
// 所有性能收集器实现此接口
// ============================================================================
#pragma once
#include <cstdint>
#include <string>

namespace SoftGPU {

// ============================================================================
// StageHandle - 阶段标识符
// ============================================================================
enum class StageHandle : uint8_t {
    CommandProcessor = 0,
    VertexShader,
    PrimitiveAssembly,
    TilingStage,
    Rasterizer,
    FragmentShader,
    TileWriteBack,
    GMEMSync,
    COUNT
};

// ============================================================================
// ProfilerStats - 单帧聚合性能统计
// ============================================================================
struct ProfilerStats {
    // 各阶段耗时（ms）
    float stageTimeMs[static_cast<size_t>(StageHandle::COUNT)] = {};

    // 各阶段调用计数
    uint32_t stageInvocations[static_cast<size_t>(StageHandle::COUNT)] = {};

    // GMEM 带宽
    float gmemReadMB  = 0.0f;
    float gmemWriteMB = 0.0f;
    float gmemBandwidthUtil = 0.0f;  // 0.0 ~ 1.0

    // 光栅化产出
    uint32_t fragmentsGenerated = 0;
    uint32_t fragmentsShaded   = 0;
    uint32_t tilesAffected     = 0;

    // 片段着色占比
    float fragmentTimeRatio = 0.0f;  // fragmentShader / total

    // 光栅化效率（实际产出 / 理论峰值）
    float rasterEfficiency = 0.0f;   // 0.0 ~ 1.0

    // 总帧时间（ms）
    float totalFrameMs = 0.0f;
};

// ============================================================================
// IProfiler - 性能分析器接口
// ============================================================================
class IProfiler {
public:
    virtual ~IProfiler() = default;

    // 阶段名称（用于显示）
    virtual const char* getStageName(StageHandle stage) const = 0;

    // 每帧开始（重置帧级计数器）
    virtual void beginFrame() = 0;

    // 记录阶段开始时间戳
    virtual void stampBegin(StageHandle stage) = 0;

    // 记录阶段结束时间戳（自动计算耗时）
    virtual void stampEnd(StageHandle stage) = 0;

    // 每帧结束（聚合数据）
    virtual void endFrame() = 0;

    // 获取当前帧的聚合统计
    virtual const ProfilerStats& getStats() const = 0;

    // 获取核心利用率（0.0 ~ 1.0）
    // 通过 (sum of active stage time) / (total frame time) 近似
    virtual float getCoreUtilization() const = 0;

    // 是否启用（debug 开关）
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};

}  // namespace SoftGPU
```

---

## 4. FrameProfiler 类设计

### 4.1 FrameProfiler 实现

**文件：** `src/profiler/FrameProfiler.hpp` + `src/profiler/FrameProfiler.cpp`

```cpp
// ============================================================================
// FrameProfiler - 帧级性能分析器（单例）
// 负责时间戳采集与数据聚合
// ============================================================================
#pragma once
#include "IProfiler.hpp"
#include <chrono>
#include <array>
#include <vector>

namespace SoftGPU {

// ============================================================================
// FrameStats - 单帧原始时间戳数据
// ============================================================================
struct FrameStats {
    // per-stage 时间戳（ns）
    uint64_t stageBeginNs[static_cast<size_t>(StageHandle::COUNT)] = {};
    uint64_t stageEndNs[static_cast<size_t>(StageHandle::COUNT)]   = {};

    // per-stage 是否在当前帧被调用
    bool     stageActive[static_cast<size_t>(StageHandle::COUNT)]  = {};

    // GMEM 带宽（从 MemorySubsystem 读取）
    uint64_t gmemReadBytes  = 0;
    uint64_t gmemWriteBytes = 0;

    // 光栅化统计
    uint32_t fragmentsGenerated = 0;
    uint32_t fragmentsShaded   = 0;
    uint32_t tilesAffected     = 0;
};

// ============================================================================
// TimestampCollector - 时间戳采集器
// 使用 chrono::steady_clock 保证单调性
// ============================================================================
class TimestampCollector {
public:
    void reset() { /* zero all timestamps */ }

    void recordBegin(StageHandle stage, uint64_t nowNs) {
        m_data[static_cast<size_t>(stage)].begin = nowNs;
        m_data[static_cast<size_t>(stage)].active = true;
    }

    void recordEnd(StageHandle stage, uint64_t nowNs) {
        m_data[static_cast<size_t>(stage)].end = nowNs;
        // active stays true once set
    }

    uint64_t getDurationNs(StageHandle stage) const {
        const auto& d = m_data[static_cast<size_t>(stage)];
        return (d.end > d.begin) ? (d.end - d.begin) : 0;
    }

    bool isActive(StageHandle stage) const {
        return m_data[static_cast<size_t>(stage)].active;
    }

private:
    struct TimestampPair {
        uint64_t begin = 0;
        uint64_t end   = 0;
        bool     active = false;
    };
    std::array<TimestampPair, static_cast<size_t>(StageHandle::COUNT)> m_data;
};

// ============================================================================
// Aggregator - 数据聚合器
// 将原始时间戳聚合成 ProfilerStats
// ============================================================================
class Aggregator {
public:
    explicit Aggregator(const MemorySubsystem& memory);

    // 聚合单帧数据 → ProfilerStats
    void compute(const FrameStats& frame, ProfilerStats& out) const;

    // 设置理论光栅化峰值（fragments/second）
    void setRasterTheoreticalPeak(float peakFps) { m_rasterPeakFps = peakFps; }

private:
    const MemorySubsystem& m_memory;
    float m_rasterPeakFps = 0.0f;  // set by BottleneckDetector
};

// ============================================================================
// FrameProfiler - 帧级性能分析器（单例）
// ============================================================================
class FrameProfiler : public IProfiler {
public:
    static FrameProfiler& instance();

    // IProfiler 实现
    const char* getStageName(StageHandle stage) const override;
    void beginFrame() override;
    void stampBegin(StageHandle stage) override;
    void stampEnd(StageHandle stage) override;
    void endFrame() override;
    const ProfilerStats& getStats() const override { return m_stats; }
    float getCoreUtilization() const override;
    void setEnabled(bool enabled) override { m_enabled = enabled; }
    bool isEnabled() const override { return m_enabled; }

    // 外部数据注入（RenderPipeline 调用）
    void setGMEMStats(uint64_t readBytes, uint64_t writeBytes);
    void setRasterStats(uint32_t fragmentsGenerated, uint32_t fragmentsShaded,
                        uint32_t tilesAffected);

    // 关联 MemorySubsystem（用于带宽读取）
    void attachMemorySubsystem(const MemorySubsystem* memory);

    // 获取 BottleneckDetector
    const class BottleneckDetector& getBottleneckDetector() const { return m_bottleneckDetector; }

private:
    FrameProfiler();
    uint64_t nowNs() const;

    bool m_enabled = true;
    uint64_t m_frameBeginNs = 0;

    TimestampCollector m_timestamps;
    FrameStats m_frame;
    ProfilerStats m_stats;
    Aggregator m_aggregator;
    class BottleneckDetector m_bottleneckDetector;
    const MemorySubsystem* m_memory = nullptr;

    std::chrono::steady_clock::time_point m_clockOrigin;
};

}  // namespace SoftGPU
```

### 4.2 FrameProfiler 实现要点

```cpp
// ============================================================================
// FrameProfiler.cpp 关键实现
// ============================================================================

uint64_t FrameProfiler::nowNs() const {
    using namespace std::chrono;
    return uint64_t(duration_cast<nanoseconds>(
        steady_clock::now() - m_clockOrigin).count());
}

void FrameProfiler::stampBegin(StageHandle stage) {
    if (!m_enabled) return;
    m_timestamps.recordBegin(stage, nowNs());
}

void FrameProfiler::stampEnd(StageHandle stage) {
    if (!m_enabled) return;
    uint64_t t = nowNs();
    m_timestamps.recordEnd(stage, t);
    m_frame.stageActive[static_cast<size_t>(stage)] = true;
}

void FrameProfiler::beginFrame() {
    if (!m_enabled) return;
    m_frameBeginNs = nowNs();
    m_timestamps.reset();
    // reset frame stats
    std::memset(&m_frame, 0, sizeof(FrameStats));
}

void FrameProfiler::endFrame() {
    if (!m_enabled) return;

    // 填充 end timestamps for stages that didn't call stampEnd
    // (some stages may not have been invoked this frame)
    for (size_t i = 0; i < static_cast<size_t>(StageHandle::COUNT); ++i) {
        if (m_frame.stageActive[i] && m_frame.stageEndNs[i] == 0) {
            m_frame.stageEndNs[i] = nowNs();
        }
    }

    // 从 MemorySubsystem 读取带宽数据
    if (m_memory) {
        m_frame.gmemReadBytes  = m_memory->getTotalReadBytes();
        m_frame.gmemWriteBytes = m_memory->getTotalWriteBytes();
    }

    // 聚合
    m_aggregator.compute(m_frame, m_stats);

    // 瓶颈判定
    m_bottleneckDetector.detect(m_stats);

    // 总帧时间
    m_stats.totalFrameMs = float(nowNs() - m_frameBeginNs) / 1e6f;
}
```

---

## 5. BottleneckDetector 类设计

### 5.1 瓶颈类型枚举

**文件：** `src/profiler/BottleneckDetector.hpp`

```cpp
// ============================================================================
// BottleneckDetector - 瓶颈自动判定
// 使用规则引擎，基于 ProfilerStats 判定瓶颈类型
// ============================================================================
#pragma once
#include "IProfiler.hpp"

namespace SoftGPU {

// ============================================================================
// BottleneckType - 瓶颈类型
// ============================================================================
enum class BottleneckType : uint8_t {
    Unknown    = 0,
    ShaderBound = 1,     // Fragment shader 太重
    MemoryBound = 2,     // GMEM 带宽饱和
    FillRateBound = 3,   // 光栅化产出不足（核心空闲等待数据）
    NoBottleneck  = 4,   // 管线平衡，无明显瓶颈
};

[[nodiscard]] const char* bottleneckTypeToString(BottleneckType type);

// ============================================================================
// BottleneckDetector - 瓶颈判定器
// ============================================================================
class BottleneckDetector {
public:
    // 判定规则阈值（可配置）
    struct Thresholds {
        float shader_bound_fragment_ratio = 0.70f;  // >70% time in fragment
        float shader_bound_core_util      = 0.50f;  // <50% core util
        float memory_bound_bw_util         = 0.85f;  // >85% bandwidth
        float memory_bound_core_util       = 0.70f;  // <70% core util
        float fill_rate_efficiency         = 0.30f;  // <30% of peak raster output
    };

    explicit BottleneckDetector(const Thresholds& thresholds = {});

    // 执行瓶颈判定
    void detect(const ProfilerStats& stats);

    // 获取结果
    BottleneckType getBottleneck() const { return m_bottleneck; }
    float getConfidence() const { return m_confidence; }  // 0.0 ~ 1.0

    // 获取判定理由（用于 UI 显示）
    std::string getReason() const { return m_reason; }

    // 设置/获取阈值
    void setThresholds(const Thresholds& t) { m_thresholds = t; }
    const Thresholds& getThresholds() const { return m_thresholds; }

    // 获取各瓶颈得分（用于柱状图显示）
    float getShaderBoundScore()  const { return m_shaderScore; }
    float getMemoryBoundScore()  const { return m_memoryScore; }
    float getFillRateBoundScore() const { return m_fillRateScore; }

private:
    Thresholds m_thresholds;
    BottleneckType m_bottleneck = BottleneckType::Unknown;
    float m_confidence = 0.0f;
    std::string m_reason;

    // 各维度得分（0.0 ~ 1.0）
    float m_shaderScore    = 0.0f;
    float m_memoryScore    = 0.0f;
    float m_fillRateScore  = 0.0f;

    // 内部判定逻辑
    void computeScores(const ProfilerStats& stats);
    void selectBottleneck();
};

// ============================================================================
// Thresholds 默认值（行业经验值）
// ============================================================================
constexpr BottleneckDetector::Thresholds DEFAULT_BOTTLENECK_THRESHOLDS = {
    .shader_bound_fragment_ratio = 0.70f,
    .shader_bound_core_util      = 0.50f,
    .memory_bound_bw_util        = 0.85f,
    .memory_bound_core_util       = 0.70f,
    .fill_rate_efficiency        = 0.30f,
};

}  // namespace SoftGPU
```

### 5.2 BottleneckDetector 判定算法

```cpp
// ============================================================================
// BottleneckDetector.cpp
// ============================================================================

#include "BottleneckDetector.hpp"
#include <sstream>
#include <algorithm>

namespace SoftGPU {

const char* bottleneckTypeToString(BottleneckType type) {
    switch (type) {
        case BottleneckType::ShaderBound:    return "Shader Bound";
        case BottleneckType::MemoryBound:     return "Memory Bound";
        case BottleneckType::FillRateBound:  return "Fill Rate Bound";
        case BottleneckType::NoBottleneck:    return "Balanced";
        default:                              return "Unknown";
    }
}

BottleneckDetector::BottleneckDetector(const Thresholds& thresholds)
    : m_thresholds(thresholds) {}

void BottleneckDetector::detect(const ProfilerStats& stats) {
    computeScores(stats);
    selectBottleneck();
}

void BottleneckDetector::computeScores(const ProfilerStats& stats) {
    using std::max;

    // ── Shader Bound Score ─────────────────────────────────────────────
    // 条件：Fragment Time Ratio 高 && Core Utilization 低
    // 解释：大量时间花在 shader 上，但核心整体利用率不高（说明 shader 太慢）
    float fsRatio   = stats.fragmentTimeRatio;         // 0.0 ~ 1.0
    float coreUtil  = stats.coreUtilization;            // 0.0 ~ 1.0 (from FrameProfiler)

    float shaderTimeScore = max(0.0f, min(1.0f, fsRatio / m_thresholds.shader_bound_fragment_ratio));
    float shaderUtilScore = (coreUtil < m_thresholds.shader_bound_core_util)
        ? (1.0f - coreUtil / m_thresholds.shader_bound_core_util)
        : 0.0f;
    m_shaderScore = (shaderTimeScore + shaderUtilScore) * 0.5f;

    // ── Memory Bound Score ────────────────────────────────────────────
    // 条件：带宽利用率高 && Core Utilization 低
    // 解释：管线等待内存读写，核心空闲
    float bwUtil = stats.gmemBandwidthUtil;             // 0.0 ~ 1.0

    float memoryBwScore = max(0.0f, min(1.0f,
        (bwUtil - m_thresholds.memory_bound_bw_util) / (1.0f - m_thresholds.memory_bound_bw_util)));
    float memoryUtilScore = (coreUtil < m_thresholds.memory_bound_core_util)
        ? (1.0f - coreUtil / m_thresholds.memory_bound_core_util)
        : 0.0f;
    m_memoryScore = (memoryBwScore + memoryUtilScore) * 0.5f;

    // ── Fill Rate Bound Score ─────────────────────────────────────────
    // 条件：光栅化效率低（产出碎片数远低于理论峰值）
    // 解释：核心因数据依赖空闲，rasterizer 产出不足
    float rasterEff = stats.rasterEfficiency;            // 0.0 ~ 1.0
    m_fillRateScore = (rasterEff < m_thresholds.fill_rate_efficiency)
        ? (1.0f - rasterEff / m_thresholds.fill_rate_efficiency)
        : 0.0f;
}

void BottleneckDetector::selectBottleneck() {
    struct Candidate {
        BottleneckType type;
        float score;
    };
    Candidate candidates[] = {
        { BottleneckType::ShaderBound,    m_shaderScore },
        { BottleneckType::MemoryBound,    m_memoryScore },
        { BottleneckType::FillRateBound,  m_fillRateScore },
    };

    // 找最大得分
    Candidate best = candidates[0];
    for (size_t i = 1; i < std::size(candidates); ++i) {
        if (candidates[i].score > best.score) best = candidates[i];
    }

    // 阈值：得分 > 0.5 才认为存在瓶颈，否则为 NoBottleneck
    if (best.score < 0.5f) {
        m_bottleneck = BottleneckType::NoBottleneck;
        m_confidence = 0.0f;
        m_reason = "All scores < 0.5: pipeline is balanced";
    } else {
        m_bottleneck = best.type;
        m_confidence = best.score;
        std::ostringstream oss;
        oss << bottleneckTypeToString(m_bottleneck)
            << " (score=" << best.score << ")";
        m_reason = oss.str();
    }
}

}  // namespace SoftGPU
```

---

## 6. ProfilerUI 类设计（ImGui 可视化）

### 6.1 ProfilerUI 声明

**文件：** `src/profiler/ProfilerUI.hpp` + `src/profiler/ProfilerUI.cpp`

```cpp
// ============================================================================
// ProfilerUI - ImGui 性能分析面板
// 提供架构图颜色标注、时间分解柱状图、瓶颈箭头标注
// ============================================================================
#pragma once
#include "FrameProfiler.hpp"
#include <string>

namespace SoftGPU {

// ============================================================================
// ProfilerUI - 性能可视化面板
// ============================================================================
class ProfilerUI {
public:
    explicit ProfilerUI(FrameProfiler& profiler);

    // 渲染 ImGui 面板（每帧调用）
    void render();

    // 是否展开（默认打开）
    void setOpen(bool open) { m_open = open; }
    bool isOpen() const { return m_open; }

    // 窗口位置/大小预设
    void setWindowPos(float x, float y);
    void setWindowSize(float w, float h);

    // 是否显示架构图（默认 true）
    void setShowArchitecture(bool show) { m_showArchitecture = show; }

private:
    // ── Panel: 帧摘要 ─────────────────────────────────────────────────
    void renderFrameSummary(const ProfilerStats& stats);

    // ── Panel: 架构图颜色标注 ────────────────────────────────────────
    void renderArchitectureDiagram(const ProfilerStats& stats);

    // ── Panel: 时间分解柱状图 ────────────────────────────────────────
    void renderTimeBreakdown(const ProfilerStats& stats);

    // ── Panel: 瓶颈判定结果 ──────────────────────────────────────────
    void renderBottleneckPanel(const BottleneckDetector& detector);

    // ── Panel: GMEM 带宽仪表 ─────────────────────────────────────────
    void renderBandwidthGauge(const ProfilerStats& stats);

    // ── Panel: 光栅化效率仪表 ────────────────────────────────────────
    void renderRasterEfficiencyGauge(const ProfilerStats& stats);

    // ── 辅助：画进度条 ────────────────────────────────────────────────
    void progressBar(const char* label, float fraction,
                     float r, float g, float b);

    // ── 辅助：瓶颈类型对应的 ImGui 颜色 ─────────────────────────────
    ImVec4 getBottleneckColor(BottleneckType type) const;

    FrameProfiler& m_profiler;
    bool m_open = true;
    bool m_showArchitecture = true;

    // 窗口布局参数
    ImVec2 m_windowPos{10.0f, 10.0f};
    ImVec2 m_windowSize{480.0f, 700.0f};

    // 滚动偏移（时间轴历史视图）
    static constexpr size_t HISTORY_SIZE = 60;  // 保留 60 帧历史
    struct HistoryEntry { float frameMs; float bottleneckScore[3]; };
    std::vector<HistoryEntry> m_history;
};

}  // namespace SoftGPU
```

### 6.2 架构图颜色标注

**渲染逻辑：** 遍历 RenderPipeline 的各 Stage，根据 `stageTimeMs` 占比决定颜色：

| 占比范围 | 颜色 | 含义 |
|----------|------|------|
| < 10% | 蓝 `#4A90D9` | 空闲/轻负载 |
| 10-30% | 绿 `#4CAF50` | 正常负载 |
| 30-60% | 黄 `#FFA726` | 中等负载（注意） |
| > 60% | 红 `#EF5350` | 高负载（瓶颈可疑） |

```
┌──────────────────────────────────────────────────────────┐
│  SoftGPU Pipeline Architecture (Real-time Utilization)   │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  [CommandProcessor] ████░░░░░░░░░░░░░░░░░░░░░░  8%  [蓝] │
│         ↓                                                │
│  [VertexShader    ] ██████░░░░░░░░░░░░░░░░░░░ 15%  [蓝] │
│         ↓                                                │
│  [PrimitiveAssem. ] ██░░░░░░░░░░░░░░░░░░░░░░░  5%  [蓝] │
│         ↓                                                │
│  [TilingStage     ] ██████████████░░░░░░░░░░░ 40%  [黄] │
│         ↓                                                │
│  [Rasterizer_SIMD ] ████████████████████░░░░░ 55%  [红] │  ← 瓶颈
│         ↓                                                │
│  [FragmentShader  ] ██████████████████████████ 78%  [红] │  ← 瓶颈
│         ↓                                                │
│  [TileWriteBack   ] ████████░░░░░░░░░░░░░░░░░ 20%  [蓝] │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 6.3 时间分解柱状图

使用 ImGui `PlotHistogram`：

```cpp
void ProfilerUI::renderTimeBreakdown(const ProfilerStats& stats) {
    ImGui::SeparatorText("Time Breakdown (ms)");

    // 准备数据（按时间降序排列）
    struct StageTime {
        const char* name;
        float ms;
        ImU32 color;
    };

    StageTime stages[] = {
        {"FragmentShader",  stats.stageTimeMs[static_cast<int>(StageHandle::FragmentShader)], IM_COL32(239,83,80,255)},
        {"Rasterizer",      stats.stageTimeMs[static_cast<int>(StageHandle::Rasterizer)],      IM_COL32(255,167,38,255)},
        {"TilingStage",     stats.stageTimeMs[static_cast<int>(StageHandle::TilingStage)],     IM_COL32(76,175,80,255)},
        {"VertexShader",    stats.stageTimeMs[static_cast<int>(StageHandle::VertexShader)],    IM_COL32(74,144,217,255)},
        {"TileWriteBack",   stats.stageTimeMs[static_cast<int>(StageHandle::TileWriteBack)],   IM_COL32(156,139,204,255)},
        {"CmdProcessor",    stats.stageTimeMs[static_cast<int>(StageHandle::CommandProcessor)], IM_COL32(128,128,128,255)},
    };

    // 找最大值用于归一化
    float maxMs = 0.0f;
    for (auto& s : stages) maxMs = std::max(maxMs, s.ms);

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    for (auto& s : stages) {
        float normalized = (maxMs > 0.0f) ? (s.ms / maxMs) : 0.0f;
        ImGui::Text("%-16s", s.name);
        ImGui::SameLine(120.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
            ImGui::ColorConvertFloat4ToU32(ImVec4(s.color)));
        ImGui::ProgressBar(normalized, ImVec2(200.0f, 14.0f), "");
        ImGui::PopStyleColor();
        ImGui::SameLine(330.0f);
        ImGui::Text("%.3f ms", s.ms);
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
    }

    // 总帧时间
    ImGui::Separator();
    ImGui::Text("Total Frame: %.3f ms (%.1f FPS)",
        stats.totalFrameMs,
        (stats.totalFrameMs > 0.0f) ? (1000.0f / stats.totalFrameMs) : 0.0f);
}
```

### 6.4 瓶颈箭头标注

在架构图底部，用大号红色箭头 + 文字标注瓶颈 Stage：

```cpp
void ProfilerUI::renderBottleneckPanel(const BottleneckDetector& detector) {
    BottleneckType type = detector.getBottleneck();
    ImVec4 color = getBottleneckColor(type);

    ImGui::SeparatorText("Bottleneck Detection");

    // 瓶颈类型标题
    ImGui::TextColored(color, "  >>  %s  <<",
        bottleneckTypeToString(type));
    ImGui::SameLine();
    ImGui::Text("  (confidence: %.0f%%)", detector.getConfidence() * 100.0f);

    // 判定理由
    ImGui::TextWrapped("Reason: %s", detector.getReason().c_str());

    // 三项得分条
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    progressBar("Shader Bound",    detector.getShaderBoundScore(),   0.94f, 0.33f, 0.32f);
    progressBar("Memory Bound",    detector.getMemoryBoundScore(),   1.00f, 0.71f, 0.27f);
    progressBar("Fill Rate Bound", detector.getFillRateBoundScore(),0.35f, 0.73f, 0.31f);

    // 优化建议
    ImGui::Separator();
    ImGui::SeparatorText("Optimization Hint");

    const char* hint = [&](){
        switch (type) {
            case BottleneckType::ShaderBound:
                return "→ Reduce shader complexity or use SIMD in FragmentShader";
            case BottleneckType::MemoryBound:
                return "→ Increase tile size or enable tile write-back async";
            case BottleneckType::FillRateBound:
                return "→ SIMD rasterization + batch fragment shader dispatch";
            case BottleneckType::NoBottleneck:
                return "→ Pipeline is balanced; no immediate optimization needed";
            default:
                return "→ Enable profiling for detailed analysis";
        }
    }();
    ImGui::TextWrapped("%s", hint);
}
```

---

## 7. RenderPipeline 集成

### 7.1 Profiler Hook 注入点

**文件：** `src/pipeline/RenderPipeline.cpp`

在每个 Stage 执行前后注入 Profiler hook：

```cpp
void RenderPipeline::render(const RenderCommand& command) {
    FrameProfiler::instance().beginFrame();

    // ── CommandProcessor ─────────────────────────────────────────────
    {
        auto& profiler = FrameProfiler::instance();
        profiler.stampBegin(StageHandle::CommandProcessor);
        m_commandProcessor.execute();
        profiler.stampEnd(StageHandle::CommandProcessor);
    }

    // ── VertexShader ─────────────────────────────────────────────────
    {
        auto& profiler = FrameProfiler::instance();
        profiler.stampBegin(StageHandle::VertexShader);
        m_vertexShader.execute();
        profiler.stampEnd(StageHandle::VertexShader);
    }

    // ── PrimitiveAssembly ───────────────────────────────────────────
    {
        auto& profiler = FrameProfiler::instance();
        profiler.stampBegin(StageHandle::PrimitiveAssembly);
        m_primitiveAssembly.execute();
        profiler.stampEnd(StageHandle::PrimitiveAssembly);
    }

    // ── TilingStage ─────────────────────────────────────────────────
    {
        auto& profiler = FrameProfiler::instance();
        profiler.stampBegin(StageHandle::TilingStage);
        m_tilingStage.execute();
        profiler.stampEnd(StageHandle::TilingStage);
    }

    // ── Per-Tile Loop ───────────────────────────────────────────────
    uint32_t tilesAffected = 0;
    uint32_t totalFragmentsGenerated = 0;
    uint32_t totalFragmentsShaded = 0;

    for (uint32_t tileIndex = 0; tileIndex < NUM_TILES; ++tileIndex) {
        const TileBin& bin = m_tilingStage.getTileBin(tileIndex);
        if (bin.triangleIndices.empty()) continue;

        tilesAffected++;

        // ── Rasterizer (per tile) ─────────────────────────────────────
        {
            auto& profiler = FrameProfiler::instance();
            profiler.stampBegin(StageHandle::Rasterizer);
            // ... rasterize tiles ...
            profiler.stampEnd(StageHandle::Rasterizer);
            totalFragmentsGenerated += /* raster output count */;
        }

        // ── FragmentShader (per tile) ────────────────────────────────
        {
            auto& profiler = FrameProfiler::instance();
            profiler.stampBegin(StageHandle::FragmentShader);
            // ... shade fragments ...
            profiler.stampEnd(StageHandle::FragmentShader);
            totalFragmentsShaded += /* shaded count */;
        }

        // ── TileWriteBack (per tile) ─────────────────────────────────
        {
            auto& profiler = FrameProfiler::instance();
            profiler.stampBegin(StageHandle::TileWriteBack);
            // ... store tile ...
            profiler.stampEnd(StageHandle::TileWriteBack);
        }
    }

    // ── GMEM Sync ───────────────────────────────────────────────────
    {
        auto& profiler = FrameProfiler::instance();
        profiler.stampBegin(StageHandle::GMEMSync);
        syncGMEMToFramebuffer();
        profiler.stampEnd(StageHandle::GMEMSync);
    }

    // 注入非阶段时间数据
    FrameProfiler::instance().setRasterStats(
        totalFragmentsGenerated, totalFragmentsShaded, tilesAffected);

    FrameProfiler::instance().endFrame();
}
```

### 7.2 ImGuiRenderer 集成 ProfilerUI

**文件：** `src/app/Application.cpp`（或 `src/renderer/Renderer.cpp`）

```cpp
// Application 主循环中：
void Application::renderFrame() {
    // ... render SoftGPU frame ...
    m_pipeline.render(command);

    // ── ImGui 新帧 ───────────────────────────────────────────────────
    m_imguiRenderer.newFrame();

    // ── 渲染 SoftGPU ProfilerUI ──────────────────────────────────────
    if (m_showProfilerUI) {
        m_profilerUI.render();
    }

    // ── 渲染 ImGui ───────────────────────────────────────────────────
    m_imguiRenderer.endFrame();
    m_imguiRenderer.render();
}
```

---

## 8. 性能优化设计

### 8.1 SIMD 光栅化（Rasterizer_SIMD）

**目标：** 将 2D 三角形扫描从逐像素循环改为 SIMD 向量化（8-32 pixels/cycle）。

**文件：** `src/stages/Rasterizer_SIMD.hpp` + `src/stages/Rasterizer_SIMD.cpp`

**接口保持兼容：** `Rasterizer_SIMD` 继承自 `Rasterizer`，通过 `#ifdef SOFTGPU_SIMD` 条件编译切换。

```cpp
// ============================================================================
// Rasterizer_SIMD - SIMD 加速光栅化器
// 继承自 Rasterizer，替代 executeTile() 中的逐像素扫描
// ============================================================================
class RasterizerSIMD : public Rasterizer {
public:
    RasterizerSIMD() = default;

    // 重写：批量光栅化一个 triangle 到 tile buffer
    // SIMD 宽度：8 像素（128-bit SSE）/ 16 像素（256-bit AVX）
    void rasterizeTriangle_SIMD(const Triangle& tri,
                                uint32_t tileX, uint32_t tileY,
                                TileBuffer& tileBuffer);

protected:
    // 内部 SIMD 边界测试（同时测试 N 个像素）
    template<size_t N>
    __m256i testBoundsPack(__m256i px, __m256i py,
                           const Triangle& tri);

    // 内部 SIMD 深度测试 + 写入
    template<size_t N>
    void depthTestAndStorePack(__m256i px, __m256i py, __m256 pz,
                               const Fragment& frag,
                               TileBuffer& tileBuffer);
};
```

**预期提升：** Rasterizer 阶段 **2-4× 加速**（1200 triangles 场景从 ~22ms → ~8ms）。

### 8.2 TilingStage 加速（O(tri) 替代 O(tri×tiles)）

**当前实现（PHASE2-4）：**

```cpp
// O(tiles × triangles) — 300 tiles × 1200 triangles = 360,000 iterations
for (uint32_t tileIdx = 0; tileIdx < NUM_TILES; ++tileIdx) {
    for (uint32_t triIdx = 0; triIdx < triangles.size(); ++triIdx) {
        if (triangleOverlapsTile(tri, tileIdx))
            bin[tileIdx].add(triIdx);
    }
}
```

**优化后（O(tri × covered_tiles)）：**

```cpp
// O(triangles × covered_tiles_per_triangle) — 1200 × 4-6 = ~6,000 iterations
for (uint32_t triIdx = 0; triIdx < triangles.size(); ++triIdx) {
    const Triangle& tri = triangles[triIdx];

    // 计算三角形 AABB（4 个顶点）
    int32_t minTX, maxTX, minTY, maxTY;
    computeTriangleAABB(tri, minTX, maxTX, minTY, maxTY);

    // 限制在有效 tile 范围
    minTX = std::max(minTX, 0);
    maxTX = std::min(maxTX, static_cast<int32_t>(NUM_TILES_X) - 1);
    minTY = std::max(minTY, 0);
    maxTY = std::min(maxTY, static_cast<int32_t>(NUM_TILES_Y) - 1);

    // 每个 triangle 只遍历它覆盖的 tile（通常是 4-6 个）
    for (int32_t ty = minTY; ty <= maxTY; ++ty) {
        for (int32_t tx = minTX; tx <= maxTX; ++tx) {
            uint32_t tileIdx = ty * NUM_TILES_X + tx;
            if (triangleOverlapsTile(tri, tx, ty))
                bin[tileIdx].add(triIdx);
        }
    }
}
```

**预期提升：** TilingStage 阶段 **10-50× 加速**（1200 triangles 从 ~0.18ms → ~0.01ms）。

---

## 9. 新增文件清单

| 文件 | 说明 |
|------|------|
| `src/profiler/IProfiler.hpp` | 性能分析器接口 |
| `src/profiler/FrameProfiler.hpp` | 帧级性能分析器（单例） |
| `src/profiler/FrameProfiler.cpp` | 时间戳采集与聚合实现 |
| `src/profiler/BottleneckDetector.hpp` | 瓶颈判定器声明 |
| `src/profiler/BottleneckDetector.cpp` | 瓶颈判定算法实现 |
| `src/profiler/ProfilerUI.hpp` | ImGui 可视化面板声明 |
| `src/profiler/ProfilerUI.cpp` | ImGui 可视化面板实现 |
| `src/stages/Rasterizer_SIMD.hpp` | SIMD 光栅化器声明（可选） |
| `src/stages/Rasterizer_SIMD.cpp` | SIMD 光栅化器实现（可选） |

### 修改文件

| 文件 | 修改类型 |
|------|----------|
| `src/pipeline/RenderPipeline.cpp` | 注入 FrameProfiler hook |
| `src/app/Application.cpp` | 调用 ProfilerUI.render() |
| `src/renderer/Renderer.cpp` | 集成 ProfilerUI |
| `src/CMakeLists.txt` | 新增 `profiler/` 子目录 |
| `src/stages/TilingStage.cpp` | O(tri) 优化 binning |

---

## 10. 验收标准

### 10.1 Profiler 功能验收

| 验证点 | 通过条件 |
|--------|----------|
| `FrameProfiler::beginFrame/endFrame` 每帧调用 | `getStats().totalFrameMs > 0` |
| 所有 8 个 Stage 时间戳记录 | `stageTimeMs[stage] >= 0`（未被调用则为 0） |
| GMEM 带宽数据与 MemorySubsystem 一致 | `gmemReadMB` 在 benchmark 场景中非零 |
| 瓶颈判定输出有效类型 | `getBottleneck()` 返回已知类型（非 Unknown） |

### 10.2 ProfilerUI 验收

| 验证点 | 通过条件 |
|--------|----------|
| ImGui 面板正常渲染 | 无 crash，窗口可见 |
| 架构图颜色随负载变化 | FragmentShader 高负载时显示红色 |
| 柱状图数据与帧统计一致 | 手动计算总和 ≈ totalFrameMs |
| 瓶颈箭头文字正确 | `getBottleneck()` 类型对应正确的优化建议 |
| 窗口可折叠/展开 | `m_open` 状态正确切换 |

### 10.3 性能验收

| 场景 | PHASE4 基准 | PHASE5 目标 | 提升比例 |
|------|-------------|-------------|----------|
| Triangle-Cubes-100 (Rasterizer) | ~22 ms | ~8 ms | **≥ 60%** |
| Triangle-Cubes-100 (TilingStage) | ~0.18 ms | ~0.02 ms | **≥ 85%** |
| Triangle-SponzaStyle (FragmentShader) | ~2.1 ms | ~1.5 ms | **≥ 25%** |

### 10.4 整体验收

- [ ] `FrameProfiler::instance()` 单例可正常获取
- [ ] `--profiler` 命令行开关启用/禁用 Profiler
- [ ] ProfilerUI 窗口默认显示在左上角，可拖动
- [ ] 60 帧历史数据在 UI 中正确滚动显示
- [ ] `RasterizerSIMD` 编译通过（需要 SIMD intrinsics 支持，`-msse4.2` 或 `-mavx2` 标志）
- [ ] TilingStage O(tri) 优化后 `getNumAffectedTiles()` 结果不变（正确性保证）

---

## 附录：命令行接口

```
$ ./SoftGPU --help
...
Profiler Options:
  --profiler              启用实时性能分析 UI（默认关闭）
  --profiler-closed        启动时 ProfilerUI 默认折叠
  --profiler-no-architecture  隐藏架构图面板
  --bottleneck-thresholds=SHADER,MEMORY,FILL  自定义瓶颈阈值（0.0~1.0）

Examples:
  ./SoftGPU --profiler                    # 启用 profiler UI
  ./SoftGPU --benchmark --profiler         # benchmark + profiler
  ./SoftGPU --profiler-closed              # profiler UI 默认折叠
  ./SoftGPU --profiler --bottleneck-thresholds=0.8,0.9,0.4
```
