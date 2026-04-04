// ============================================================================
// SoftGPU - FrameProfiler.cpp
// Frame profiler implementation
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#include "FrameProfiler.hpp"
#include <algorithm>
#include <cmath>

namespace SoftGPU {

// ============================================================================
// TimestampCollector
// ============================================================================
TimestampCollector::TimestampCollector() {
    reset();
}

void TimestampCollector::reset() {
    m_accumulatedNs.fill(0);
    m_sampleCounts.fill(0);
    m_entryTimestamps.fill(0);
    m_entryValid.fill(false);
}

void TimestampCollector::pushEntry(StageHandle stage, uint64_t timestamp_ns) {
    size_t idx = static_cast<size_t>(stage);
    m_entryTimestamps[idx] = timestamp_ns;
    m_entryValid[idx] = true;
}

uint64_t TimestampCollector::pushExit(StageHandle stage, uint64_t timestamp_ns) {
    size_t idx = static_cast<size_t>(stage);
    if (!m_entryValid[idx]) return 0;
    uint64_t duration = timestamp_ns - m_entryTimestamps[idx];
    m_accumulatedNs[idx] += duration;
    m_sampleCounts[idx] += 1;
    m_entryValid[idx] = false;
    return duration;
}

uint64_t TimestampCollector::getAccumulatedNs(StageHandle stage) const {
    return m_accumulatedNs[static_cast<size_t>(stage)];
}

size_t TimestampCollector::getSampleCount(StageHandle stage) const {
    return m_sampleCounts[static_cast<size_t>(stage)];
}

// ============================================================================
// Aggregator
// ============================================================================
Aggregator::Aggregator() {
    reset();
}

void Aggregator::reset() {
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        m_msHistory[i].clear();
        m_msHistory[i].reserve(FRAME_WINDOW);
    }
    m_frameMsHistory.clear();
    m_frameMsHistory.reserve(FRAME_WINDOW);
    m_lastInvocations.fill(0);
    m_lastTotalNs = 0;
}

void Aggregator::feed(const std::array<uint64_t, STAGE_COUNT>& frameNs,
                      uint64_t totalFrameNs,
                      const std::array<uint64_t, STAGE_COUNT>& invocations)
{
    double totalMs = totalFrameNs / 1e6;

    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        double ms = frameNs[i] / 1e6;
        m_msHistory[i].push_back(ms);
        if (m_msHistory[i].size() > FRAME_WINDOW) {
            m_msHistory[i].erase(m_msHistory[i].begin());
        }
    }

    m_frameMsHistory.push_back(totalMs);
    if (m_frameMsHistory.size() > FRAME_WINDOW) {
        m_frameMsHistory.erase(m_frameMsHistory.begin());
    }

    m_lastInvocations = invocations;
    m_lastTotalNs = totalFrameNs;
}

double Aggregator::getAverageMs(StageHandle stage) const {
    size_t idx = static_cast<size_t>(stage);
    const auto& history = m_msHistory[idx];
    if (history.empty()) return 0.0;
    double sum = 0.0;
    for (double v : history) sum += v;
    return sum / history.size();
}

double Aggregator::getAverageFrameMs() const {
    if (m_frameMsHistory.empty()) return 0.0;
    double sum = 0.0;
    for (double v : m_frameMsHistory) sum += v;
    return sum / m_frameMsHistory.size();
}

double Aggregator::getAverageFps() const {
    double avgMs = getAverageFrameMs();
    if (avgMs <= 0.0) return 0.0;
    return 1000.0 / avgMs;
}

double Aggregator::getStagePercent(StageHandle stage) const {
    double frameMs = getAverageFrameMs();
    if (frameMs <= 0.0) return 0.0;
    double stageMs = getAverageMs(stage);
    return (stageMs / frameMs) * 100.0;
}

uint64_t Aggregator::getInvocations(StageHandle stage) const {
    return m_lastInvocations[static_cast<size_t>(stage)];
}

// ============================================================================
// FrameProfiler
// ============================================================================
FrameProfiler::FrameProfiler() {
    // Nothing special
}

FrameProfiler& FrameProfiler::get() {
    static FrameProfiler instance;
    return instance;
}

FrameProfiler* FrameProfiler::instance() {
    return &get();
}

void FrameProfiler::beginFrame() {
    m_frameStart = std::chrono::high_resolution_clock::now();
    m_collector.reset();
    m_frameActive = true;
    // P2-1: reset per-frame divergence counters
    m_divergenceCount = 0;
    m_divergenceThreads = 0;
    m_divergenceLostCycles = 0;
    m_totalWarpsScheduled = 0;
}

void FrameProfiler::endFrame() {
    if (!m_frameActive) return;

    auto frameEnd = std::chrono::high_resolution_clock::now();
    uint64_t frameNs = std::chrono::duration<uint64_t, std::nano>(
        frameEnd - m_frameStart).count();

    // Collect accumulated ns per stage
    std::array<uint64_t, STAGE_COUNT> stageNs{};
    std::array<uint64_t, STAGE_COUNT> invocations{};
    uint64_t totalNs = 0;
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        StageHandle stage = static_cast<StageHandle>(i);
        stageNs[i] = m_collector.getAccumulatedNs(stage);
        invocations[i] = m_collector.getSampleCount(stage);
        totalNs += stageNs[i];
    }

    // If total measured time < frame wall time, attribute difference to idle/overhead
    if (totalNs < frameNs) {
        // Add "overhead" to CommandProcessor (first stage)
        stageNs[0] += (frameNs - totalNs);
    }

    m_aggregator.feed(stageNs, frameNs, invocations);
    m_frameActive = false;
}

void FrameProfiler::beginStage(StageHandle stage) {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t ns = std::chrono::duration<uint64_t, std::nano>(
        now.time_since_epoch()).count();
    m_collector.pushEntry(stage, ns);
}

void FrameProfiler::endStage(StageHandle stage) {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t ns = std::chrono::duration<uint64_t, std::nano>(
        now.time_since_epoch()).count();
    m_collector.pushExit(stage, ns);
}

ProfilerStats FrameProfiler::getStats(StageHandle stage) const {
    ProfilerStats stats;
    size_t idx = static_cast<size_t>(stage);
    stats.ms = m_aggregator.getAverageMs(stage);
    stats.invocations = m_aggregator.getInvocations(stage);
    stats.percent = m_aggregator.getStagePercent(stage);
    // Rough cycles estimate: assume 1 GHz = 1 cycle per ns
    stats.cycles = static_cast<uint64_t>(stats.ms * 1e6);

    // P2-1: warp divergence — only meaningful for FragmentShader stage
    if (stage == StageHandle::FragmentShader) {
        stats.divergenceCount = m_divergenceCount;
        stats.divergenceThreads = m_divergenceThreads;
        stats.divergenceLostCycles = m_divergenceLostCycles;
    }
    return stats;
}

std::array<ProfilerStats, STAGE_COUNT> FrameProfiler::getAllStats() const {
    std::array<ProfilerStats, STAGE_COUNT> result;
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        result[i] = getStats(static_cast<StageHandle>(i));
    }
    return result;
}

BottleneckResult FrameProfiler::detectBottleneck() const {
    BottleneckResult result;

    // Clamp input metrics to valid [0,1] range before confidence calculation
    // Bugfix 1: prevent confidence overflow when metrics exceed expected range
    float fsRatio = std::clamp(m_fsRatio, 0.0, 1.0);
    float bandwidthUtil = std::clamp(m_bandwidthUtil, 0.0, 1.0);

    // Shader Bound: fs_ratio > 70% && core_util < 50%
    if (fsRatio > 0.70 && m_coreUtilization < 0.50) {
        result.type = BottleneckType::ShaderBound;
        result.confidence = static_cast<float>(std::min(1.0, fsRatio - 0.70) * 5.0);
        result.severity = fsRatio;
        result.description = "Fragment shader is the bottleneck (heavy pixel processing)";
        return result;
    }

    // Memory Bound: bw_util > 85% && core_util < 70%
    if (bandwidthUtil > 0.85 && m_coreUtilization < 0.70) {
        result.type = BottleneckType::MemoryBound;
        result.confidence = static_cast<float>(std::min(1.0, bandwidthUtil - 0.85) * 6.67);
        result.severity = bandwidthUtil;
        result.description = "Memory bandwidth is saturated (GMEM access bottleneck)";
        return result;
    }

    // Fill Rate Bound: rasterizer_eff < 30%
    if (m_rasterizerEfficiency < 0.30 && m_rasterizerEfficiency > 0.0) {
        result.type = BottleneckType::FillRateBound;
        // Bugfix 2: clamp confidence to [0,1] — formula can exceed 1.0 when rasterizerEfficiency < 0
        float rawConfidence = static_cast<float>((0.30 - m_rasterizerEfficiency) / 0.30);
        result.confidence = std::clamp(rawConfidence, 0.0f, 1.0f);
        result.severity = static_cast<float>(1.0 - m_rasterizerEfficiency);
        result.description = "Rasterizer output exceeds fragment shader throughput (fill rate limited)";
        return result;
    }

    result.type = BottleneckType::Unknown;
    result.confidence = 0.0f;
    result.severity = 0.0f;
    result.description = "No significant bottleneck detected";
    return result;
}

double FrameProfiler::getFrameTimeMs() const {
    return m_aggregator.getAverageFrameMs();
}

double FrameProfiler::getFps() const {
    return m_aggregator.getAverageFps();
}

void FrameProfiler::reset() {
    m_collector.reset();
    m_aggregator.reset();
    m_frameActive = false;
    m_bandwidthUtil = 0.0;
    m_rasterizerEfficiency = 0.0;
    m_coreUtilization = 0.0;
    m_fsRatio = 0.0;
    // P2-1: reset divergence counters
    m_divergenceCount = 0;
    m_divergenceThreads = 0;
    m_divergenceLostCycles = 0;
    m_totalWarpsScheduled = 0;
}

void FrameProfiler::recordDivergence(uint32_t threadsInDivergence, uint64_t lostCycles) {
    m_divergenceCount += 1;
    m_divergenceThreads += threadsInDivergence;
    m_divergenceLostCycles += lostCycles;
}

double FrameProfiler::getDivergenceRate() const {
    if (m_totalWarpsScheduled == 0) return 0.0;
    return static_cast<double>(m_divergenceCount) / static_cast<double>(m_totalWarpsScheduled);
}

// ============================================================================
// IProfiler stage name lookup
// ============================================================================
const char* IProfiler::stageToString(StageHandle stage) {
    switch (stage) {
        case StageHandle::CommandProcessor:  return "CommandProcessor";
        case StageHandle::VertexShader:       return "VertexShader";
        case StageHandle::PrimitiveAssembly:  return "PrimitiveAssembly";
        case StageHandle::Rasterizer:          return "Rasterizer";
        case StageHandle::FragmentShader:     return "FragmentShader";
        case StageHandle::Framebuffer:         return "Framebuffer";
        case StageHandle::TileWriteBack:      return "TileWriteBack";
        case StageHandle::Tiling:             return "Tiling";
        default:                              return "Unknown";
    }
}

}  // namespace SoftGPU
