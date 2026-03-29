// ============================================================================
// SoftGPU - BenchmarkRunner.cpp
// 自动化 Benchmark 运行器实现
// PHASE4
// ============================================================================

#include "BenchmarkRunner.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>

namespace SoftGPU {

// ============================================================================
// BenchmarkRunner Implementation
// ============================================================================
BenchmarkRunner::BenchmarkRunner(const Config& config) : m_config(config) {}

void BenchmarkRunner::setConfig(const Config& config) {
    m_config = config;
}

BenchmarkSet BenchmarkRunner::run() {
    BenchmarkSet set;

    // 注册内置场景
    TestSceneRegistry::instance().registerBuiltinScenes();

    for (const auto& sceneName : m_config.scenes) {
        if (m_config.verbose) {
            std::cout << "\n[" << sceneName << "] Running " << m_config.runsPerScene << " iterations...\n";
        }

        for (uint32_t i = 0; i < m_config.runsPerScene; i++) {
            BenchmarkResult result = runScene(sceneName);
            set.results.push_back(result);

            if (m_config.verbose) {
                std::cout << "  Run " << (i + 1) << ": " << std::fixed
                          << std::setprecision(3) << result.frameTimeMs << " ms, "
                          << "FPS: " << std::setprecision(1) << result.fps << "\n";
            }
        }

        // 计算当前场景的摘要
        std::vector<BenchmarkResult> sceneResults;
        for (const auto& r : set.results) {
            if (r.sceneName == sceneName) {
                sceneResults.push_back(r);
            }
        }
        if (!sceneResults.empty()) {
            BenchmarkSummary summary;
            summary.calculate(sceneResults);
            set.summaries.push_back(summary);
        }
    }

    // 保存结果
    if (m_config.saveResults && !m_config.outputCSV.empty()) {
        set.saveToCSV(m_config.outputCSV);
    }

    // 打印摘要
    if (m_config.printSummary) {
        std::cout << "\n\n";
        std::cout << "########################################\n";
        std::cout << "#     BENCHMARK SUMMARY                #\n";
        std::cout << "########################################\n";
        for (const auto& summary : set.summaries) {
            summary.print();
        }
    }

    return set;
}

BenchmarkResult BenchmarkRunner::runScene(const std::string& sceneName) {
    auto scene = TestSceneRegistry::instance().getScene(sceneName);
    if (!scene) {
        std::cerr << "Error: Scene not found: " << sceneName << "\n";
        BenchmarkResult result;
        result.sceneName = sceneName + " (NOT FOUND)";
        return result;
    }
    return runSingleFrame(scene);
}

std::vector<BenchmarkResult> BenchmarkRunner::runSceneMultiple(const std::string& sceneName, uint32_t count) {
    std::vector<BenchmarkResult> results;
    for (uint32_t i = 0; i < count; i++) {
        results.push_back(runScene(sceneName));
    }
    return results;
}

BenchmarkResult BenchmarkRunner::runSingleFrame(std::shared_ptr<TestScene> scene) {
    RenderCommand command;
    scene->buildRenderCommand(command);

    // 创建渲染管线
    RenderPipeline pipeline;

    // 预热（第一次运行通常较慢）
    pipeline.render(command);

    // 重置计数器
    pipeline.getMemorySubsystem().resetCounters();

    // 正式测试
    auto startTime = std::chrono::high_resolution_clock::now();
    pipeline.render(command);
    auto endTime = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // 直接收集结果，不使用 const reference
    BenchmarkResult result;
    result.sceneName = scene->getName();
    result.runIndex = 0;
    result.triangleCount = scene->getTriangleCount();
    result.vertexCount = command.drawParams.vertexCount;
    result.frameTimeMs = elapsedMs;
    result.fps = (elapsedMs > 0.0) ? (1000.0 / elapsedMs) : 0.0;

    // 带宽统计
    auto& mem = pipeline.getMemorySubsystem();
    result.bandwidthUtilization = mem.getBandwidthUtilization();
    result.totalReadBytes = mem.getReadBytes();
    result.totalWriteBytes = mem.getWriteBytes();
    result.consumedBandwidthGBps = mem.getConsumedBandwidthGBps();

    // Cache 统计
    result.L2HitRate = mem.getL2Cache().getHitRate();
    result.L2Hits = mem.getL2Cache().getHits();
    result.L2Misses = mem.getL2Cache().getMisses();

    // 各阶段耗时 - 从 FrameProfiler 获取真实时间
    const auto& perf = pipeline.getVertexShader().getCounters();
    result.cycleCount = perf.cycle_count;
    result.vertexShaderTimeMs = perf.elapsed_ms;

    // 从 FrameProfiler 获取真实阶段时间
    FrameProfiler& profiler = FrameProfiler::get();
    result.fragmentShaderTimeMs = profiler.getStats(StageHandle::FragmentShader).ms;
    result.tilingTimeMs = profiler.getStats(StageHandle::Tiling).ms;
    result.rasterizerTimeMs = profiler.getStats(StageHandle::Rasterizer).ms;
    result.tileWriteBackTimeMs = profiler.getStats(StageHandle::TileWriteBack).ms;

    // 像素处理
    result.fragmentsProcessed = perf.extra_count1;
    result.pixelsWritten = command.drawParams.vertexCount * 2;  // 估算

    return result;
}

BenchmarkSet BenchmarkRunner::runWithComparison() {
    BenchmarkSet current = run();

    if (!m_config.baselineCSV.empty()) {
        BenchmarkSet baseline = loadBaseline(m_config.baselineCSV);
        calculateComparison(baseline, current, current);

        // 保存对比结果
        if (m_config.saveResults) {
            std::string compFile = m_config.outputCSV;
            if (!compFile.empty()) {
                size_t dotPos = compFile.find_last_of('.');
                if (dotPos != std::string::npos) {
                    compFile.insert(dotPos, "_comparison");
                } else {
                    compFile += "_comparison";
                }
                current.saveComparisonToCSV(compFile);
            }
        }
    }

    return current;
}

BenchmarkSet BenchmarkRunner::loadBaseline(const std::string& filepath) {
    BenchmarkSet set;
    set.loadFromCSV(filepath);
    std::cout << "Loaded baseline from: " << filepath << "\n";
    return set;
}

void BenchmarkRunner::calculateComparison(const BenchmarkSet& baseline,
                                          const BenchmarkSet& current,
                                          BenchmarkSet& outResult) {
    // 按场景分组计算对比
    for (const auto& currResult : current.results) {
        // 找到对应的 baseline 结果
        for (const auto& baseResult : baseline.results) {
            if (currResult.sceneName == baseResult.sceneName) {
                BenchmarkComparison comp;
                comp.sceneName = currResult.sceneName;
                comp.baselineFrameTimeMs = baseResult.frameTimeMs;
                comp.currentFrameTimeMs = currResult.frameTimeMs;
                comp.speedup = (baseResult.frameTimeMs > 0.0) ?
                               (baseResult.frameTimeMs / currResult.frameTimeMs) : 1.0;
                comp.improvementPercent = (baseResult.frameTimeMs > 0.0) ?
                                         ((baseResult.frameTimeMs - currResult.frameTimeMs) / baseResult.frameTimeMs * 100.0) : 0.0;
                comp.baselineBandwidth = baseResult.consumedBandwidthGBps;
                comp.currentBandwidth = currResult.consumedBandwidthGBps;
                comp.bandwidthChange = (baseResult.consumedBandwidthGBps > 0.0) ?
                                       ((currResult.consumedBandwidthGBps - baseResult.consumedBandwidthGBps) / baseResult.consumedBandwidthGBps * 100.0) : 0.0;
                comp.baselineL2HitRate = baseResult.L2HitRate;
                comp.currentL2HitRate = currResult.L2HitRate;
                comp.hitRateChange = currResult.L2HitRate - baseResult.L2HitRate;

                outResult.comparisons.push_back(comp);
                break;
            }
        }
    }

    // 打印对比
    std::cout << "\n\n";
    std::cout << "########################################\n";
    std::cout << "#     BASELINE COMPARISON              #\n";
    std::cout << "########################################\n";
    for (const auto& comp : outResult.comparisons) {
        comp.print();
    }
}

// ============================================================================
// Command-line factory
// ============================================================================
BenchmarkRunner BenchmarkRunner::createFromCommandLine(int argc, char* argv[]) {
    Config config;

    CmdLineParser parser(argc, argv);

    // 解析选项
    if (parser.hasOption("--scenes")) {
        std::string scenesStr = parser.getOption("--scenes", "");
        // 逗号分隔的场景名
        std::string current;
        for (char c : scenesStr) {
            if (c == ',') {
                if (!current.empty()) {
                    config.scenes.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            config.scenes.push_back(current);
        }
    } else {
        // 默认所有场景
        TestSceneRegistry::instance().registerBuiltinScenes();
        config.scenes = TestSceneRegistry::instance().getAllSceneNames();
    }

    if (parser.hasOption("--runs")) {
        config.runsPerScene = std::stoul(parser.getOption("--runs", "3"));
    }

    if (parser.hasOption("--output")) {
        config.outputCSV = parser.getOption("--output", "benchmark_results.csv");
    }

    if (parser.hasOption("--compare-to")) {
        config.baselineCSV = parser.getOption("--compare-to", "");
    }

    config.verbose = parser.hasOption("-v") || parser.hasOption("--verbose");
    config.printSummary = !parser.hasOption("--no-summary");
    config.saveResults = !parser.hasOption("--no-save");

    return BenchmarkRunner(config);
}

void BenchmarkRunner::printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --scenes <list>    Comma-separated list of scenes to run\n";
    std::cout << "                     Available: Triangle-1Tri, Triangle-Cube,\n";
    std::cout << "                                  Triangle-Cubes-100, Triangle-SponzaStyle,\n";
    std::cout << "                                  PBR-Material\n";
    std::cout << "  --runs <n>         Number of runs per scene (default: 3)\n";
    std::cout << "  --output <file>    Output CSV file (default: benchmark_results.csv)\n";
    std::cout << "  --compare-to <f>   Compare against baseline CSV\n";
    std::cout << "  -v, --verbose      Verbose output\n";
    std::cout << "  --no-summary       Don't print summary\n";
    std::cout << "  --no-save          Don't save results\n";
    std::cout << "  -h, --help         Show this help\n";
}

// ============================================================================
// CmdLineParser Implementation
// ============================================================================
CmdLineParser::CmdLineParser(int argc, char* argv[]) : m_argc(argc), m_argv(argv) {}

bool CmdLineParser::hasOption(const std::string& opt) const {
    for (int i = 1; i < m_argc; i++) {
        if (std::string(m_argv[i]) == opt) {
            return true;
        }
    }
    return false;
}

std::string CmdLineParser::getOption(const std::string& opt, const std::string& defaultVal) const {
    for (int i = 1; i < m_argc; i++) {
        if (std::string(m_argv[i]) == opt) {
            if (i + 1 < m_argc && m_argv[i + 1][0] != '-') {
                return m_argv[i + 1];
            }
        }
    }
    return defaultVal;
}

std::vector<std::string> CmdLineParser::getArguments() const {
    std::vector<std::string> args;
    for (int i = 1; i < m_argc; i++) {
        if (m_argv[i][0] != '-') {
            args.push_back(m_argv[i]);
        }
    }
    return args;
}

}  // namespace SoftGPU
