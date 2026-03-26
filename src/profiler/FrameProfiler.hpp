// ============================================================================
// SoftGPU - FrameProfiler.hpp
// Frame profiler - singleton timestamp collector and aggregator
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#pragma once

#include "IProfiler.hpp"
#include <array>
#include <vector>
#include <chrono>
#include <cstdint>

namespace SoftGPU {

// ============================================================================
// TimestampCollector - Collects nanosecond timestamps per stage
// ============================================================================
class TimestampCollector {
public:
    TimestampCollector();

    // Record entry timestamp for a stage
    void pushEntry(StageHandle stage, uint64_t timestamp_ns);

    // Record exit timestamp for a stage (returns duration ns)
    uint64_t pushExit(StageHandle stage, uint64_t timestamp_ns);

    // Get accumulated time for a stage (ns)
    uint64_t getAccumulatedNs(StageHandle stage) const;

    // Clear all timestamps
    void reset();

    size_t getSampleCount(StageHandle stage) const;

private:
    std::array<uint64_t, STAGE_COUNT> m_accumulatedNs{};
    std::array<size_t, STAGE_COUNT>   m_sampleCounts{};
    std::array<uint64_t, STAGE_COUNT> m_entryTimestamps{};
    std::array<bool, STAGE_COUNT>     m_entryValid{};
};

// ============================================================================
// Aggregator - Aggregates per-frame stats into rolling averages
// ============================================================================
class Aggregator {
public:
    static constexpr size_t FRAME_WINDOW = 60;  // Rolling average over 60 frames

    Aggregator();

    // Feed a frame's per-stage timings (ns)
    void feed(const std::array<uint64_t, STAGE_COUNT>& frameNs,
              uint64_t totalFrameNs,
              const std::array<uint64_t, STAGE_COUNT>& invocations);

    // Get average ms for a stage over the window
    double getAverageMs(StageHandle stage) const;

    // Get average total frame time (ms)
    double getAverageFrameMs() const;

    // Get current FPS (based on rolling average)
    double getAverageFps() const;

    // Get percentage of frame time for a stage
    double getStagePercent(StageHandle stage) const;

    // Get invocations for a stage in the last frame
    uint64_t getInvocations(StageHandle stage) const;

    void reset();

private:
    std::array<std::vector<double>, STAGE_COUNT> m_msHistory;
    std::vector<double> m_frameMsHistory;
    std::array<uint64_t, STAGE_COUNT> m_lastInvocations{};
    uint64_t m_lastTotalNs = 0;
};

// ============================================================================
// FrameProfiler - Singleton profiler implementation
// ============================================================================
class FrameProfiler : public IProfiler {
public:
    // Singleton access
    static FrameProfiler& get();

    // IProfiler interface
    void beginFrame() override;
    void endFrame() override;

    void beginStage(StageHandle stage) override;
    void endStage(StageHandle stage) override;

    ProfilerStats getStats(StageHandle stage) const override;
    std::array<ProfilerStats, STAGE_COUNT> getAllStats() const override;

    BottleneckResult detectBottleneck() const override;

    double getFrameTimeMs() const override;
    double getFps() const override;

    void reset() override;

    // Access the singleton
    static FrameProfiler* instance();

    // Inject MemorySubsystem reference for bandwidth metrics (optional)
    void setMemoryBandwidthUtilization(double bw) { m_bandwidthUtil = bw; }
    void setRasterizerEfficiency(double eff) { m_rasterizerEfficiency = eff; }
    void setCoreUtilization(double util) { m_coreUtilization = util; }

    // Fragment shader ratio for bottleneck detection
    void setFragmentShaderRatio(double ratio) { m_fsRatio = ratio; }

    // Rasterizer efficiency (fragments output / fragments possible)
    double getRasterizerEfficiency() const { return m_rasterizerEfficiency; }

private:
    FrameProfiler();

    std::chrono::high_resolution_clock::time_point m_frameStart;
    bool m_frameActive = false;

    TimestampCollector m_collector;
    Aggregator         m_aggregator;

    // Bottleneck metrics (updated externally by RenderPipeline or manually)
    double m_bandwidthUtil = 0.0;       // 0.0 ~ 1.0
    double m_rasterizerEfficiency = 0.0; // 0.0 ~ 1.0
    double m_coreUtilization = 0.0;      // 0.0 ~ 1.0 (core utilization %)
    double m_fsRatio = 0.0;             // fragment shader time / total frame time
};

}  // namespace SoftGPU
