// ============================================================================
// SoftGPU - BottleneckDetector.cpp
// Bottleneck analysis implementation
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#include "BottleneckDetector.hpp"
#include <algorithm>
#include <cmath>

namespace SoftGPU {

// ============================================================================
// MetricsCollector
// ============================================================================
MetricsCollector::MetricsCollector() {
    reset();
}

void MetricsCollector::reset() {
    m_bandwidthUtil = 0.0;
    m_rasterizerEfficiency = 0.0;
    m_coreUtilization = 0.0;
    m_fsRatio = 0.0;
    m_fragmentCount = 0;
    m_triangleCount = 0;
    m_pixelCount = 0;
}

void MetricsCollector::updateBandwidthUtilization(double util) {
    m_bandwidthUtil = std::clamp(util, 0.0, 1.0);
}

void MetricsCollector::updateRasterizerEfficiency(double efficiency) {
    m_rasterizerEfficiency = std::clamp(efficiency, 0.0, 1.0);
}

void MetricsCollector::updateCoreUtilization(double util) {
    m_coreUtilization = std::clamp(util, 0.0, 1.0);
}

void MetricsCollector::updateFragmentShaderRatio(double ratio) {
    m_fsRatio = std::clamp(ratio, 0.0, 1.0);
}

void MetricsCollector::updateFragmentCount(uint64_t count) {
    m_fragmentCount = count;
}

void MetricsCollector::updateTriangleCount(uint64_t count) {
    m_triangleCount = count;
}

void MetricsCollector::updatePixelCount(uint64_t count) {
    m_pixelCount = count;
}

// ============================================================================
// BottleneckDetector
// ============================================================================
BottleneckDetector::BottleneckDetector() {
    reset();
}

void BottleneckDetector::reset() {
    m_metrics = nullptr;
    m_profiler = nullptr;
}

void BottleneckDetector::setMetricsCollector(const MetricsCollector* collector) {
    m_metrics = collector;
}

void BottleneckDetector::setFrameProfiler(const FrameProfiler* profiler) {
    m_profiler = profiler;
}

float BottleneckDetector::getShaderBoundScore() const {
    if (!m_profiler) return 0.0f;
    double fsMs = m_profiler->getStats(StageHandle::FragmentShader).ms;
    double frameMs = m_profiler->getFrameTimeMs();
    if (frameMs <= 0.0) return 0.0f;
    float ratio = static_cast<float>(fsMs / frameMs);
    return std::clamp(ratio, 0.0f, 1.0f);
}

float BottleneckDetector::getMemoryBoundScore() const {
    if (!m_metrics) return 0.0f;
    return static_cast<float>(m_metrics->getBandwidthUtilization());
}

float BottleneckDetector::getFillRateBoundScore() const {
    if (!m_metrics) return 0.0f;
    float eff = static_cast<float>(m_metrics->getRasterizerEfficiency());
    // Lower efficiency → higher fill rate bound score
    return std::clamp(1.0f - eff, 0.0f, 1.0f);
}

float BottleneckDetector::getComputeBoundScore() const {
    if (!m_profiler) return 0.0f;
    double vsMs = m_profiler->getStats(StageHandle::VertexShader).ms;
    double paMs = m_profiler->getStats(StageHandle::PrimitiveAssembly).ms;
    double frameMs = m_profiler->getFrameTimeMs();
    if (frameMs <= 0.0) return 0.0f;
    float ratio = static_cast<float>((vsMs + paMs) / frameMs);
    return std::clamp(ratio, 0.0f, 1.0f);
}

BottleneckResult BottleneckDetector::analyze() const {
    BottleneckResult result;

    float shaderScore   = getShaderBoundScore();
    float memoryScore  = getMemoryBoundScore();
    float fillScore    = getFillRateBoundScore();
    float computeScore = getComputeBoundScore();

    // Use FrameProfiler bottleneck detection if available
    if (m_profiler) {
        BottleneckResult fpResult = m_profiler->detectBottleneck();
        if (fpResult.confidence > 0.3f) {
            return fpResult;
        }
    }

    // Determine bottleneck based on highest score
    const float threshold = 0.5f;
    float maxScore = std::max({shaderScore, memoryScore, fillScore, computeScore});

    if (maxScore < threshold) {
        result.type = BottleneckType::Unknown;
        result.confidence = 0.0f;
        result.severity = maxScore;
        result.description = "Pipeline running efficiently — no significant bottleneck";
        return result;
    }

    if (shaderScore == maxScore) {
        result.type = BottleneckType::ShaderBound;
        result.confidence = shaderScore;
        result.severity = shaderScore;
        result.description = "Fragment shader dominates frame time — complex pixel shaders";
    } else if (memoryScore == maxScore) {
        result.type = BottleneckType::MemoryBound;
        result.confidence = memoryScore;
        result.severity = memoryScore;
        result.description = "Memory bandwidth saturated — high GMEM traffic";
    } else if (fillScore == maxScore) {
        result.type = BottleneckType::FillRateBound;
        result.confidence = fillScore;
        result.severity = fillScore;
        result.description = "Rasterizer produces more fragments than FS can consume";
    } else {
        result.type = BottleneckType::ComputeBound;
        result.confidence = computeScore;
        result.severity = computeScore;
        result.description = "Vertex/primitive processing is the bottleneck";
    }

    return result;
}

std::string BottleneckDetector::getRecommendation(const BottleneckResult& result) const {
    switch (result.type) {
        case BottleneckType::ShaderBound:
            return "Reduce fragment shader complexity, use early Z, or reduce overdraw";
        case BottleneckType::MemoryBound:
            return "Reduce GMEM traffic: use tile buffer, increase cache locality, or reduce resolution";
        case BottleneckType::FillRateBound:
            return "Reduce fragment count: lower resolution, use LOD, or reduce draw calls";
        case BottleneckType::ComputeBound:
            return "Reduce vertex workload: use frustum culling, level-of-detail, or simplify geometry";
        default:
            return "Pipeline is well-balanced";
    }
}

}  // namespace SoftGPU
