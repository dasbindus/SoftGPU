// ============================================================================
// SoftGPU - BenchmarkResult.hpp
// Benchmark 结果数据结构
// PHASE4
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <core/PipelineTypes.hpp>

namespace SoftGPU {

// ============================================================================
// BenchmarkResult - 单次 Benchmark 运行结果
// 包含 15 列指标
// ============================================================================
struct BenchmarkResult {
    // ========== 基本信息 ==========
    std::string sceneName;           // 场景名称
    std::string timestamp;           // 运行时间戳
    uint32_t runIndex;               // 运行序号

    // ========== 几何信息 ==========
    uint32_t triangleCount;          // 三角形数量
    uint32_t vertexCount;            // 顶点数

    // ========== 性能指标 ==========
    double frameTimeMs;              // 单帧耗时 (ms)
    double fps;                     // 帧率 (FPS)
    uint64_t cycleCount;            // 周期计数

    // ========== 带宽指标 ==========
    double bandwidthUtilization;     // 带宽利用率 (0.0 ~ 1.0)
    uint64_t totalReadBytes;         // 总读取字节数
    uint64_t totalWriteBytes;       // 总写入字节数
    double consumedBandwidthGBps;    // 实际消耗带宽 (GB/s)

    // ========== Cache 指标 ==========
    double L2HitRate;               // L2 Cache 命中率 (0.0 ~ 1.0)
    uint64_t L2Hits;                // L2 Cache 命中次数
    uint64_t L2Misses;              // L2 Cache 缺失次数

    // ========== 各阶段耗时 (ms) ==========
    double vertexShaderTimeMs;       // Vertex Shader 耗时
    double tilingTimeMs;             // Tiling 耗时
    double rasterizerTimeMs;         // Rasterizer 耗时
    double fragmentShaderTimeMs;     // Fragment Shader 耗时
    double tileWriteBackTimeMs;     // Tile Write-back 耗时

    // ========== 像素处理 ==========
    uint64_t fragmentsProcessed;    // 处理的片段数
    uint64_t pixelsWritten;         // 写入的像素数

    // ========== P2-2: Warp Scheduling & TEX 指标 ==========
    // Warp scheduler efficiency = active warps / total warps per cycle
    double warpSchedulingEfficiency;  // 0.0 ~ 1.0
    uint64_t totalWarpsScheduled;      // 总调度 warp 数
    uint32_t warpDivergenceCount;      // 分歧发生次数
    double warpDivergenceRate;         // 分歧率 (divergent/total)

    // ========== P2-2: EarlyZ 指标 ==========
    uint64_t earlyZTests;        // EarlyZ 深度测试次数
    uint64_t earlyZPassed;      // EarlyZ 通过次数
    double earlyZPassRate;      // EarlyZ 通过率 (0.0 ~ 1.0)
    uint64_t earlyZRejected;    // EarlyZ 拒绝的 fragment 数

    // ========== P2-2: TEX 指标 ==========
    uint64_t texOperations;     // TEX 操作总次数
    double texHitRate;          // TEX cache 命中率 (0.0 ~ 1.0)
    uint64_t texCacheHits;      // TEX cache 命中次数
    uint64_t texCacheMisses;    // TEX cache 缺失次数
    double texAvgLatencyNs;     // TEX 平均延迟 (ns)

    // 默认构造函数
    BenchmarkResult();

    // 打印结果
    void print() const;

    // 转换为 CSV 行
    std::string toCSV() const;

    // 获取 CSV 表头
    static std::string getCSVHeader();
};

// ============================================================================
// BenchmarkComparison - Baseline 对比结果
// ============================================================================
struct BenchmarkComparison {
    std::string sceneName;
    double baselineFrameTimeMs;
    double currentFrameTimeMs;
    double speedup;
    double improvementPercent;

    // 带宽对比
    double baselineBandwidth;
    double currentBandwidth;
    double bandwidthChange;

    // Cache 对比
    double baselineL2HitRate;
    double currentL2HitRate;
    double hitRateChange;

    BenchmarkComparison() = default;

    void print() const;
    static std::string getCSVHeader();
    std::string toCSV() const;
};

// ============================================================================
// BenchmarkSummary - 多次运行的统计摘要
// ============================================================================
struct BenchmarkSummary {
    std::string sceneName;
    uint32_t runCount;

    // 帧时统计
    double minFrameTimeMs;
    double maxFrameTimeMs;
    double avgFrameTimeMs;
    double stdDevFrameTimeMs;

    // 带宽统计
    double avgBandwidthUtilization;
    double avgL2HitRate;

    // 各阶段平均耗时
    double avgVertexShaderTimeMs;
    double avgTilingTimeMs;
    double avgRasterizerTimeMs;
    double avgFragmentShaderTimeMs;
    double avgTileWriteBackTimeMs;

    BenchmarkSummary() = default;

    void calculate(const std::vector<BenchmarkResult>& results);
    void print() const;
};

// ============================================================================
// BenchmarkSet - 完整 Benchmark 集合
// ============================================================================
struct BenchmarkSet {
    std::vector<BenchmarkResult> results;
    std::vector<BenchmarkComparison> comparisons;
    std::vector<BenchmarkSummary> summaries;

    // 保存所有结果到 CSV 文件
    bool saveToCSV(const std::string& filepath) const;

    // 保存对比结果到 CSV 文件
    bool saveComparisonToCSV(const std::string& filepath) const;

    // 加载结果从 CSV 文件
    bool loadFromCSV(const std::string& filepath);
};

}  // namespace SoftGPU
