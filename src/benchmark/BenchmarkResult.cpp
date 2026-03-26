// ============================================================================
// SoftGPU - BenchmarkResult.cpp
// Benchmark 结果数据结构实现
// PHASE4
// ============================================================================

#include "BenchmarkResult.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>

namespace SoftGPU {

// ============================================================================
// BenchmarkResult Implementation
// ============================================================================
BenchmarkResult::BenchmarkResult()
    : sceneName("Unknown")
    , timestamp("")
    , runIndex(0)
    , triangleCount(0)
    , vertexCount(0)
    , frameTimeMs(0.0)
    , fps(0.0)
    , cycleCount(0)
    , bandwidthUtilization(0.0)
    , totalReadBytes(0)
    , totalWriteBytes(0)
    , consumedBandwidthGBps(0.0)
    , L2HitRate(0.0)
    , L2Hits(0)
    , L2Misses(0)
    , vertexShaderTimeMs(0.0)
    , tilingTimeMs(0.0)
    , rasterizerTimeMs(0.0)
    , fragmentShaderTimeMs(0.0)
    , tileWriteBackTimeMs(0.0)
    , fragmentsProcessed(0)
    , pixelsWritten(0)
{
    // 获取当前时间戳
    std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    timestamp = buf;
}

void BenchmarkResult::print() const {
    std::cout << "========================================\n";
    std::cout << "Benchmark Result: " << sceneName << "\n";
    std::cout << "========================================\n";
    std::cout << "Timestamp: " << timestamp << "\n";
    std::cout << "Run Index: " << runIndex << "\n";
    std::cout << "\n--- Geometry ---\n";
    std::cout << "  Triangle Count: " << triangleCount << "\n";
    std::cout << "  Vertex Count: " << vertexCount << "\n";
    std::cout << "\n--- Performance ---\n";
    std::cout << "  Frame Time: " << std::fixed << std::setprecision(3) << frameTimeMs << " ms\n";
    std::cout << "  FPS: " << std::fixed << std::setprecision(1) << fps << "\n";
    std::cout << "  Cycle Count: " << cycleCount << "\n";
    std::cout << "\n--- Bandwidth ---\n";
    std::cout << "  Utilization: " << std::fixed << std::setprecision(2)
              << (bandwidthUtilization * 100.0) << "%\n";
    std::cout << "  Read: " << (totalReadBytes / 1024.0) << " KB\n";
    std::cout << "  Write: " << (totalWriteBytes / 1024.0) << " KB\n";
    std::cout << "  Consumed: " << std::fixed << std::setprecision(2)
              << consumedBandwidthGBps << " GB/s\n";
    std::cout << "\n--- Cache ---\n";
    std::cout << "  L2 Hit Rate: " << std::fixed << std::setprecision(2)
              << (L2HitRate * 100.0) << "%\n";
    std::cout << "  Hits: " << L2Hits << "\n";
    std::cout << "  Misses: " << L2Misses << "\n";
    std::cout << "\n--- Stage Times ---\n";
    std::cout << "  Vertex Shader: " << std::fixed << std::setprecision(3)
              << vertexShaderTimeMs << " ms\n";
    std::cout << "  Tiling: " << std::fixed << std::setprecision(3)
              << tilingTimeMs << " ms\n";
    std::cout << "  Rasterizer: " << std::fixed << std::setprecision(3)
              << rasterizerTimeMs << " ms\n";
    std::cout << "  Fragment Shader: " << std::fixed << std::setprecision(3)
              << fragmentShaderTimeMs << " ms\n";
    std::cout << "  Tile Write-back: " << std::fixed << std::setprecision(3)
              << tileWriteBackTimeMs << " ms\n";
    std::cout << "\n--- Pixel Processing ---\n";
    std::cout << "  Fragments Processed: " << fragmentsProcessed << "\n";
    std::cout << "  Pixels Written: " << pixelsWritten << "\n";
    std::cout << "========================================\n";
}

std::string BenchmarkResult::toCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);

    // 15 columns
    ss << sceneName << ","
       << timestamp << ","
       << runIndex << ","
       << triangleCount << ","
       << vertexCount << ","
       << frameTimeMs << ","
       << fps << ","
       << cycleCount << ","
       << bandwidthUtilization << ","
       << totalReadBytes << ","
       << totalWriteBytes << ","
       << consumedBandwidthGBps << ","
       << L2HitRate << ","
       << L2Hits << ","
       << L2Misses << ","
       << vertexShaderTimeMs << ","
       << tilingTimeMs << ","
       << rasterizerTimeMs << ","
       << fragmentShaderTimeMs << ","
       << tileWriteBackTimeMs << ","
       << fragmentsProcessed << ","
       << pixelsWritten;

    return ss.str();
}

std::string BenchmarkResult::getCSVHeader() {
    return "scene_name,timestamp,run_index,triangle_count,vertex_count,"
           "frame_time_ms,fps,cycle_count,"
           "bandwidth_utilization,total_read_bytes,total_write_bytes,consumed_bandwidth_gbps,"
           "L2_hit_rate,L2_hits,L2_misses,"
           "vertex_shader_time_ms,tiling_time_ms,rasterizer_time_ms,"
           "fragment_shader_time_ms,tile_writeback_time_ms,"
           "fragments_processed,pixels_written";
}

// ============================================================================
// BenchmarkComparison Implementation
// ============================================================================
void BenchmarkComparison::print() const {
    std::cout << "\n========================================\n";
    std::cout << "Comparison: " << sceneName << "\n";
    std::cout << "========================================\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  Frame Time: " << baselineFrameTimeMs << " ms -> "
              << currentFrameTimeMs << " ms\n";
    std::cout << "  Speedup: " << std::setprecision(2) << speedup << "x\n";
    std::cout << "  Improvement: " << improvementPercent << "%\n";
    std::cout << "\n  Bandwidth: " << baselineBandwidth << " -> "
              << currentBandwidth << " GB/s (" << bandwidthChange << "%)\n";
    std::cout << "\n  L2 Hit Rate: " << (baselineL2HitRate * 100.0) << "% -> "
              << (currentL2HitRate * 100.0) << "% (" << (hitRateChange * 100.0) << "%)\n";
    std::cout << "========================================\n";
}

std::string BenchmarkComparison::getCSVHeader() {
    return "scene_name,baseline_frame_time_ms,current_frame_time_ms,"
           "speedup,improvement_percent,"
           "baseline_bandwidth_gbps,current_bandwidth_gbps,bandwidth_change,"
           "baseline_L2_hit_rate,current_L2_hit_rate,hit_rate_change";
}

std::string BenchmarkComparison::toCSV() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << sceneName << ","
       << baselineFrameTimeMs << ","
       << currentFrameTimeMs << ","
       << speedup << ","
       << improvementPercent << ","
       << baselineBandwidth << ","
       << currentBandwidth << ","
       << bandwidthChange << ","
       << baselineL2HitRate << ","
       << currentL2HitRate << ","
       << hitRateChange;
    return ss.str();
}

// ============================================================================
// BenchmarkSummary Implementation
// ============================================================================
void BenchmarkSummary::calculate(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) return;

    sceneName = results[0].sceneName;
    runCount = static_cast<uint32_t>(results.size());

    // Calculate frame time statistics
    minFrameTimeMs = results[0].frameTimeMs;
    maxFrameTimeMs = results[0].frameTimeMs;
    double sumFrameTime = 0.0;
    double sumSqFrameTime = 0.0;

    double sumBandwidth = 0.0;
    double sumL2HitRate = 0.0;
    double sumVSTime = 0.0, sumTilingTime = 0.0, sumRastTime = 0.0;
    double sumFSTime = 0.0, sumTWTime = 0.0;

    for (const auto& r : results) {
        minFrameTimeMs = std::min(minFrameTimeMs, r.frameTimeMs);
        maxFrameTimeMs = std::max(maxFrameTimeMs, r.frameTimeMs);
        sumFrameTime += r.frameTimeMs;
        sumSqFrameTime += r.frameTimeMs * r.frameTimeMs;

        sumBandwidth += r.bandwidthUtilization;
        sumL2HitRate += r.L2HitRate;
        sumVSTime += r.vertexShaderTimeMs;
        sumTilingTime += r.tilingTimeMs;
        sumRastTime += r.rasterizerTimeMs;
        sumFSTime += r.fragmentShaderTimeMs;
        sumTWTime += r.tileWriteBackTimeMs;
    }

    avgFrameTimeMs = sumFrameTime / runCount;
    double variance = (sumSqFrameTime / runCount) - (avgFrameTimeMs * avgFrameTimeMs);
    stdDevFrameTimeMs = std::sqrt(std::max(0.0, variance));

    avgBandwidthUtilization = sumBandwidth / runCount;
    avgL2HitRate = sumL2HitRate / runCount;
    avgVertexShaderTimeMs = sumVSTime / runCount;
    avgTilingTimeMs = sumTilingTime / runCount;
    avgRasterizerTimeMs = sumRastTime / runCount;
    avgFragmentShaderTimeMs = sumFSTime / runCount;
    avgTileWriteBackTimeMs = sumTWTime / runCount;
}

void BenchmarkSummary::print() const {
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Summary: " << sceneName << "\n";
    std::cout << "========================================\n";
    std::cout << "Runs: " << runCount << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n--- Frame Time ---\n";
    std::cout << "  Min: " << minFrameTimeMs << " ms\n";
    std::cout << "  Max: " << maxFrameTimeMs << " ms\n";
    std::cout << "  Avg: " << avgFrameTimeMs << " ms\n";
    std::cout << "  StdDev: " << stdDevFrameTimeMs << " ms\n";
    std::cout << "\n--- Bandwidth & Cache ---\n";
    std::cout << "  Avg Bandwidth: " << std::setprecision(2)
              << (avgBandwidthUtilization * 100.0) << "%\n";
    std::cout << "  Avg L2 Hit Rate: " << std::setprecision(2)
              << (avgL2HitRate * 100.0) << "%\n";
    std::cout << "\n--- Stage Times ---\n";
    std::cout << "  Vertex Shader: " << avgVertexShaderTimeMs << " ms\n";
    std::cout << "  Tiling: " << avgTilingTimeMs << " ms\n";
    std::cout << "  Rasterizer: " << avgRasterizerTimeMs << " ms\n";
    std::cout << "  Fragment Shader: " << avgFragmentShaderTimeMs << " ms\n";
    std::cout << "  Tile Write-back: " << avgTileWriteBackTimeMs << " ms\n";
    std::cout << "========================================\n";
}

// ============================================================================
// BenchmarkSet Implementation
// ============================================================================
bool BenchmarkSet::saveToCSV(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << "\n";
        return false;
    }

    // Write header
    file << BenchmarkResult::getCSVHeader() << "\n";

    // Write results
    for (const auto& r : results) {
        file << r.toCSV() << "\n";
    }

    file.close();
    std::cout << "Results saved to: " << filepath << "\n";
    return true;
}

bool BenchmarkSet::saveComparisonToCSV(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << "\n";
        return false;
    }

    // Write header
    file << BenchmarkComparison::getCSVHeader() << "\n";

    // Write comparisons
    for (const auto& c : comparisons) {
        file << c.toCSV() << "\n";
    }

    file.close();
    std::cout << "Comparison saved to: " << filepath << "\n";
    return true;
}

bool BenchmarkSet::loadFromCSV(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filepath << "\n";
        return false;
    }

    results.clear();

    std::string line;
    // Skip header
    if (!std::getline(file, line)) return false;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        BenchmarkResult r;
        std::string val;

        std::getline(ss, r.sceneName, ',');
        std::getline(ss, r.timestamp, ',');
        std::getline(ss, val, ','); r.runIndex = std::stoul(val);
        std::getline(ss, val, ','); r.triangleCount = std::stoul(val);
        std::getline(ss, val, ','); r.vertexCount = std::stoul(val);
        std::getline(ss, val, ','); r.frameTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.fps = std::stod(val);
        std::getline(ss, val, ','); r.cycleCount = std::stoull(val);
        std::getline(ss, val, ','); r.bandwidthUtilization = std::stod(val);
        std::getline(ss, val, ','); r.totalReadBytes = std::stoull(val);
        std::getline(ss, val, ','); r.totalWriteBytes = std::stoull(val);
        std::getline(ss, val, ','); r.consumedBandwidthGBps = std::stod(val);
        std::getline(ss, val, ','); r.L2HitRate = std::stod(val);
        std::getline(ss, val, ','); r.L2Hits = std::stoull(val);
        std::getline(ss, val, ','); r.L2Misses = std::stoull(val);
        std::getline(ss, val, ','); r.vertexShaderTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.tilingTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.rasterizerTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.fragmentShaderTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.tileWriteBackTimeMs = std::stod(val);
        std::getline(ss, val, ','); r.fragmentsProcessed = std::stoull(val);
        std::getline(ss, val, ','); r.pixelsWritten = std::stoull(val);

        results.push_back(r);
    }

    file.close();
    return true;
}

}  // namespace SoftGPU
