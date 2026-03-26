// ============================================================================
// SoftGPU - IProfiler.hpp
// Performance profiler interface
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace SoftGPU {

// ============================================================================
// StageHandle - Pipeline stage identifier
// ============================================================================
enum class StageHandle {
    CommandProcessor = 0,
    VertexShader,
    PrimitiveAssembly,
    Rasterizer,
    FragmentShader,
    Framebuffer,
    TileWriteBack,
    Tiling,
    STAGE_COUNT
};

constexpr size_t STAGE_COUNT = static_cast<size_t>(StageHandle::STAGE_COUNT);

// ============================================================================
// ProfilerStats - Per-stage statistics
// ============================================================================
struct ProfilerStats {
    uint64_t invocations = 0;   // Number of times this stage was called
    uint64_t cycles = 0;         // Cycle count (derived from ns / ns_per_cycle)
    double   ms = 0.0;          // Wall-clock time in milliseconds
    double   percent = 0.0;     // Percentage of total frame time

    void reset() {
        invocations = 0;
        cycles = 0;
        ms = 0.0;
        percent = 0.0;
    }
};

// ============================================================================
// BottleneckType - Identified bottleneck category
// ============================================================================
enum class BottleneckType {
    Unknown,
    ShaderBound,     // Fragment shader heavy, low core utilization
    MemoryBound,     // High bandwidth utilization, core waiting
    FillRateBound,   // Rasterizer output too many fragments
    ComputeBound,    // Vertex / geometry processing heavy
};

// ============================================================================
// BottleneckResult - Bottleneck analysis result
// ============================================================================
struct BottleneckResult {
    BottleneckType type = BottleneckType::Unknown;
    float          confidence = 0.0f;   // 0.0 ~ 1.0
    float          severity   = 0.0f;   // 0.0 ~ 1.0 (how severe)
    std::string    description;
};

// ============================================================================
// IProfiler - Profiler interface
// ============================================================================
class IProfiler {
public:
    virtual ~IProfiler() = default;

    // Frame boundaries (call matched pairs)
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    // Begin / end profiling for a specific stage
    virtual void beginStage(StageHandle stage) = 0;
    virtual void endStage(StageHandle stage) = 0;

    // Get statistics for a specific stage
    virtual ProfilerStats getStats(StageHandle stage) const = 0;

    // Get all stage stats as array
    virtual std::array<ProfilerStats, STAGE_COUNT> getAllStats() const = 0;

    // Bottleneck detection
    virtual BottleneckResult detectBottleneck() const = 0;

    // Overall frame stats
    virtual double getFrameTimeMs() const = 0;
    virtual double getFps() const = 0;

    // Reset all counters
    virtual void reset() = 0;

    // Stage name lookup
    static const char* stageToString(StageHandle stage);
};

// ============================================================================
// ProfilerGuard - RAII scope guard for stage profiling
// ============================================================================
class ProfilerGuard {
public:
    ProfilerGuard(IProfiler* profiler, StageHandle stage)
        : m_profiler(profiler), m_stage(stage) {
        if (m_profiler) m_profiler->beginStage(m_stage);
    }
    ~ProfilerGuard() {
        if (m_profiler) m_profiler->endStage(m_stage);
    }
    ProfilerGuard(const ProfilerGuard&) = delete;
    ProfilerGuard& operator=(const ProfilerGuard&) = delete;

private:
    IProfiler* m_profiler;
    StageHandle m_stage;
};

}  // namespace SoftGPU
