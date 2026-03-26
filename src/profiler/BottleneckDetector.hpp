// ============================================================================
// SoftGPU - BottleneckDetector.hpp
// Bottleneck analysis based on profiling metrics
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#pragma once

#include "IProfiler.hpp"
#include "FrameProfiler.hpp"

namespace SoftGPU {

// ============================================================================
// MetricsCollector - Collects bottleneck-relevant metrics from pipeline
// ============================================================================
class MetricsCollector {
public:
    MetricsCollector();

    // Update bottleneck metrics from external sources (RenderPipeline, etc.)
    void updateBandwidthUtilization(double util);
    void updateRasterizerEfficiency(double efficiency);
    void updateCoreUtilization(double util);
    void updateFragmentShaderRatio(double ratio);
    void updateFragmentCount(uint64_t count);
    void updateTriangleCount(uint64_t count);
    void updatePixelCount(uint64_t count);

    // Getters
    double getBandwidthUtilization()   const { return m_bandwidthUtil; }
    double getRasterizerEfficiency()   const { return m_rasterizerEfficiency; }
    double getCoreUtilization()        const { return m_coreUtilization; }
    double getFragmentShaderRatio()    const { return m_fsRatio; }
    uint64_t getFragmentCount()        const { return m_fragmentCount; }
    uint64_t getTriangleCount()        const { return m_triangleCount; }
    uint64_t getPixelCount()           const { return m_pixelCount; }

    void reset();

private:
    double m_bandwidthUtil = 0.0;
    double m_rasterizerEfficiency = 0.0;
    double m_coreUtilization = 0.0;
    double m_fsRatio = 0.0;
    uint64_t m_fragmentCount = 0;
    uint64_t m_triangleCount = 0;
    uint64_t m_pixelCount = 0;
};

// ============================================================================
// BottleneckDetector - Analyzes pipeline for bottlenecks
// ============================================================================
class BottleneckDetector {
public:
    BottleneckDetector();

    // Set metrics source
    void setMetricsCollector(const MetricsCollector* collector);
    void setFrameProfiler(const FrameProfiler* profiler);

    // Run bottleneck analysis
    BottleneckResult analyze() const;

    // Get detailed scores (0.0 ~ 1.0)
    float getShaderBoundScore()   const;
    float getMemoryBoundScore()  const;
    float getFillRateBoundScore() const;
    float getComputeBoundScore() const;

    // Recommendation string
    std::string getRecommendation(const BottleneckResult& result) const;

    void reset();

private:
    const MetricsCollector* m_metrics = nullptr;
    const FrameProfiler*   m_profiler = nullptr;
};

}  // namespace SoftGPU
