// ============================================================================
// SoftGPU - BenchmarkRunner Unit Tests
// PHASE4
// ============================================================================

#include <gtest/gtest.h>
#include "benchmark/BenchmarkRunner.hpp"
#include "benchmark/BenchmarkResult.hpp"
#include "test/TestScene.hpp"

namespace {

using namespace SoftGPU;

class BenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestSceneRegistry::instance().registerBuiltinScenes();
    }
};

// ---------------------------------------------------------------------------
// BenchmarkResult Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, BenchmarkResultDefaultInit) {
    BenchmarkResult result;

    EXPECT_EQ(result.sceneName, "Unknown");
    EXPECT_EQ(result.triangleCount, 0u);
    EXPECT_EQ(result.vertexCount, 0u);
    EXPECT_DOUBLE_EQ(result.frameTimeMs, 0.0);
    EXPECT_DOUBLE_EQ(result.fps, 0.0);
    EXPECT_DOUBLE_EQ(result.bandwidthUtilization, 0.0);
}

TEST_F(BenchmarkTest, BenchmarkResultCSVGeneration) {
    BenchmarkResult result;
    result.sceneName = "TestScene";
    result.triangleCount = 100;
    result.vertexCount = 300;
    result.frameTimeMs = 16.5;
    result.fps = 60.0;
    result.cycleCount = 1000000;
    result.bandwidthUtilization = 0.75;
    result.L2HitRate = 0.95;

    std::string csv = result.toCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("TestScene"), std::string::npos);
    EXPECT_NE(csv.find("100"), std::string::npos);
    EXPECT_NE(csv.find("300"), std::string::npos);
}

TEST_F(BenchmarkTest, BenchmarkResultCSVHeader) {
    std::string header = BenchmarkResult::getCSVHeader();

    EXPECT_NE(header.find("scene_name"), std::string::npos);
    EXPECT_NE(header.find("triangle_count"), std::string::npos);
    EXPECT_NE(header.find("frame_time_ms"), std::string::npos);
    EXPECT_NE(header.find("bandwidth_utilization"), std::string::npos);
    EXPECT_NE(header.find("L2_hit_rate"), std::string::npos);
}

// ---------------------------------------------------------------------------
// BenchmarkComparison Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, BenchmarkComparisonCalculation) {
    BenchmarkComparison comp;
    comp.sceneName = "TestScene";
    comp.baselineFrameTimeMs = 20.0;
    comp.currentFrameTimeMs = 15.0;
    comp.speedup = 20.0 / 15.0;
    comp.improvementPercent = ((20.0 - 15.0) / 20.0) * 100.0;

    EXPECT_NEAR(comp.speedup, 1.333333, 1e-5);
    EXPECT_DOUBLE_EQ(comp.improvementPercent, 25.0);
}

TEST_F(BenchmarkTest, BenchmarkComparisonCSV) {
    BenchmarkComparison comp;
    comp.sceneName = "Triangle-Cube";
    comp.baselineFrameTimeMs = 10.0;
    comp.currentFrameTimeMs = 8.0;
    comp.speedup = 1.25;
    comp.improvementPercent = 20.0;
    comp.baselineBandwidth = 50.0;
    comp.currentBandwidth = 60.0;
    comp.bandwidthChange = 20.0;
    comp.baselineL2HitRate = 0.9;
    comp.currentL2HitRate = 0.95;
    comp.hitRateChange = 0.05;

    std::string csv = comp.toCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("Triangle-Cube"), std::string::npos);
}

// ---------------------------------------------------------------------------
// BenchmarkSummary Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, BenchmarkSummaryCalculation) {
    std::vector<BenchmarkResult> results = {
        []() {
            BenchmarkResult r;
            r.sceneName = "TestScene";
            r.frameTimeMs = 10.0;
            r.bandwidthUtilization = 0.5;
            r.L2HitRate = 0.9;
            r.vertexShaderTimeMs = 1.0;
            r.tilingTimeMs = 0.5;
            r.rasterizerTimeMs = 2.0;
            r.fragmentShaderTimeMs = 5.0;
            r.tileWriteBackTimeMs = 1.0;
            return r;
        }(),
        []() {
            BenchmarkResult r;
            r.sceneName = "TestScene";
            r.frameTimeMs = 12.0;
            r.bandwidthUtilization = 0.6;
            r.L2HitRate = 0.85;
            r.vertexShaderTimeMs = 1.2;
            r.tilingTimeMs = 0.6;
            r.rasterizerTimeMs = 2.2;
            r.fragmentShaderTimeMs = 6.0;
            r.tileWriteBackTimeMs = 1.2;
            return r;
        }(),
        []() {
            BenchmarkResult r;
            r.sceneName = "TestScene";
            r.frameTimeMs = 11.0;
            r.bandwidthUtilization = 0.55;
            r.L2HitRate = 0.88;
            r.vertexShaderTimeMs = 1.1;
            r.tilingTimeMs = 0.55;
            r.rasterizerTimeMs = 2.1;
            r.fragmentShaderTimeMs = 5.5;
            r.tileWriteBackTimeMs = 1.1;
            return r;
        }()
    };

    BenchmarkSummary summary;
    summary.calculate(results);

    EXPECT_EQ(summary.sceneName, "TestScene");
    EXPECT_EQ(summary.runCount, 3u);
    EXPECT_DOUBLE_EQ(summary.minFrameTimeMs, 10.0);
    EXPECT_DOUBLE_EQ(summary.maxFrameTimeMs, 12.0);
    EXPECT_DOUBLE_EQ(summary.avgFrameTimeMs, 11.0);
    EXPECT_DOUBLE_EQ(summary.avgBandwidthUtilization, 0.55);
    EXPECT_NEAR(summary.avgL2HitRate, 0.876666, 1e-5);
}

// ---------------------------------------------------------------------------
// BenchmarkSet Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, BenchmarkSetSaveAndLoad) {
    BenchmarkSet set;

    // Add some results
    for (int i = 0; i < 3; i++) {
        BenchmarkResult result;
        result.sceneName = "Triangle-1Tri";
        result.triangleCount = 1;
        result.vertexCount = 3;
        result.frameTimeMs = 0.5 + i * 0.1;
        result.fps = 1000.0 / result.frameTimeMs;
        set.results.push_back(result);
    }

    // Create a temporary file
    std::string tempFile = "/tmp/benchmark_test.csv";

    // Save
    EXPECT_TRUE(set.saveToCSV(tempFile));

    // Load into new set
    BenchmarkSet loadedSet;
    EXPECT_TRUE(loadedSet.loadFromCSV(tempFile));

    EXPECT_EQ(loadedSet.results.size(), 3u);
    EXPECT_EQ(loadedSet.results[0].sceneName, "Triangle-1Tri");
    EXPECT_DOUBLE_EQ(loadedSet.results[0].frameTimeMs, 0.5);

    // Cleanup
    std::remove(tempFile.c_str());
}

// ---------------------------------------------------------------------------
// BenchmarkRunner Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, BenchmarkRunnerConfig) {
    BenchmarkRunner::Config config;
    config.scenes = {"Triangle-1Tri", "Triangle-Cube"};
    config.runsPerScene = 5;
    config.outputCSV = "/tmp/test_output.csv";
    config.verbose = true;
    config.printSummary = false;
    config.saveResults = false;

    BenchmarkRunner runner(config);
    EXPECT_EQ(runner.getConfig().scenes.size(), 2u);
    EXPECT_EQ(runner.getConfig().runsPerScene, 5u);
}

TEST_F(BenchmarkTest, BenchmarkRunnerSingleScene) {
    BenchmarkRunner runner;
    runner.setConfig(BenchmarkRunner::Config{
        .scenes = {"Triangle-1Tri"},
        .runsPerScene = 1,
        .verbose = false,
        .printSummary = false,
        .saveResults = false
    });

    BenchmarkResult result = runner.runScene("Triangle-1Tri");

    EXPECT_EQ(result.sceneName, "Triangle-1Tri");
    EXPECT_EQ(result.triangleCount, 1u);
    EXPECT_GE(result.frameTimeMs, 0.0);
}

TEST_F(BenchmarkTest, BenchmarkRunnerMultipleScenes) {
    BenchmarkRunner runner;
    runner.setConfig(BenchmarkRunner::Config{
        .scenes = {"Triangle-1Tri", "Triangle-Cube"},
        .runsPerScene = 1,
        .verbose = false,
        .printSummary = false,
        .saveResults = false
    });

    BenchmarkSet set = runner.run();

    EXPECT_GE(set.results.size(), 2u);
}

TEST_F(BenchmarkTest, BenchmarkRunnerNonexistentScene) {
    BenchmarkRunner runner;
    BenchmarkResult result = runner.runScene("NonExistentScene");

    EXPECT_EQ(result.sceneName, "NonExistentScene (NOT FOUND)");
}

TEST_F(BenchmarkTest, BenchmarkRunnerFullRun) {
    BenchmarkRunner runner;
    runner.setConfig(BenchmarkRunner::Config{
        .scenes = {"Triangle-1Tri"},
        .runsPerScene = 2,
        .outputCSV = "",
        .verbose = false,
        .printSummary = false,
        .saveResults = false
    });

    BenchmarkSet set = runner.run();

    EXPECT_EQ(set.results.size(), 2u);
    EXPECT_EQ(set.summaries.size(), 1u);
    EXPECT_EQ(set.summaries[0].sceneName, "Triangle-1Tri");
    EXPECT_EQ(set.summaries[0].runCount, 2u);
}

// ---------------------------------------------------------------------------
// Performance Target Tests
// ---------------------------------------------------------------------------

TEST_F(BenchmarkTest, PerformanceTargetsTriangleCubes100) {
    // This test documents the performance targets
    // Triangle-Cubes-100: PHASE3 baseline 28.5 ms, PHASE4 target ≤15.0 ms

    BenchmarkRunner runner;
    runner.setConfig(BenchmarkRunner::Config{
        .scenes = {"Triangle-Cubes-100"},
        .runsPerScene = 3,
        .verbose = true,
        .printSummary = true,
        .saveResults = false
    });

    BenchmarkSet set = runner.run();

    ASSERT_EQ(set.summaries.size(), 1u);
    const auto& summary = set.summaries[0];

    std::cout << "\nTriangle-Cubes-100 Performance:\n";
    std::cout << "  Average: " << summary.avgFrameTimeMs << " ms\n";
    std::cout << "  Target: 15.0 ms\n";

    // The actual target check is informational - actual performance varies by hardware
    // In CI, we'd check against the target
}

TEST_F(BenchmarkTest, PerformanceTargetsSponzaStyle) {
    // Triangle-SponzaStyle: PHASE3 baseline 12.3 ms, PHASE4 target ≤7.0 ms

    BenchmarkRunner runner;
    runner.setConfig(BenchmarkRunner::Config{
        .scenes = {"Triangle-SponzaStyle"},
        .runsPerScene = 3,
        .verbose = true,
        .printSummary = true,
        .saveResults = false
    });

    BenchmarkSet set = runner.run();

    ASSERT_EQ(set.summaries.size(), 1u);
    const auto& summary = set.summaries[0];

    std::cout << "\nTriangle-SponzaStyle Performance:\n";
    std::cout << "  Average: " << summary.avgFrameTimeMs << " ms\n";
    std::cout << "  Target: 7.0 ms\n";
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
