// ============================================================================
// SoftGPU - BenchmarkRunner.hpp
// 自动化 Benchmark 运行器
// PHASE4
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "BenchmarkResult.hpp"
#include "../test/TestScene.hpp"
#include "../pipeline/RenderPipeline.hpp"

namespace SoftGPU {

// ============================================================================
// BenchmarkRunner - 自动化 Benchmark 运行器
// ============================================================================
class BenchmarkRunner {
public:
    // 配置
    struct Config {
        std::vector<std::string> scenes;           // 要运行的场景列表
        uint32_t runsPerScene = 3;                 // 每个场景运行次数
        std::string outputCSV;                      // 输出 CSV 文件路径
        std::string baselineCSV;                    // Baseline CSV 文件路径（用于对比）
        bool verbose = true;                        // 详细输出
        bool printSummary = true;                  // 打印摘要
        bool saveResults = true;                   // 保存结果
    };

    BenchmarkRunner() = default;
    explicit BenchmarkRunner(const Config& config);

    // 设置配置
    void setConfig(const Config& config);
    const Config& getConfig() const { return m_config; }

    // 运行所有 Benchmark
    BenchmarkSet run();

    // 运行单个场景
    BenchmarkResult runScene(const std::string& sceneName);

    // 运行单个场景多次
    std::vector<BenchmarkResult> runSceneMultiple(const std::string& sceneName, uint32_t count);

    // 对比 Baseline
    BenchmarkSet runWithComparison();

    // 静态工厂方法（命令行友好）
    static BenchmarkRunner createFromCommandLine(int argc, char* argv[]);
    static void printUsage(const char* programName);

private:
    Config m_config;

    // 内部：运行一次渲染并收集结果
    BenchmarkResult runSingleFrame(std::shared_ptr<TestScene> scene);

    // 内部：从 CSV 加载 Baseline
    BenchmarkSet loadBaseline(const std::string& filepath);

    // 内部：计算对比
    void calculateComparison(const BenchmarkSet& baseline, const BenchmarkSet& current,
                            BenchmarkSet& outResult);
};

// ============================================================================
// Command-line parser helper
// ============================================================================
class CmdLineParser {
public:
    CmdLineParser(int argc, char* argv[]);

    bool hasOption(const std::string& opt) const;
    std::string getOption(const std::string& opt, const std::string& defaultVal = "") const;
    std::vector<std::string> getArguments() const;

private:
    int m_argc;
    char** m_argv;
};

}  // namespace SoftGPU
