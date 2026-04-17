// ============================================================================
// E2E_Framework.hpp - E2E Test Framework Header
// SoftGPU v1.0 - End-to-End Scene Tests
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#pragma once

#include <gtest/gtest.h>
#include <pipeline/RenderPipeline.hpp>
#include <core/PipelineTypes.hpp>
#include <utils/FrameDumper.hpp>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <array>
#include <memory>
#include <functional>

using namespace SoftGPU;

// ============================================================================
// Test Configuration
// ============================================================================
struct E2ETestConfig {
    const char* output_dir = ".";
    const char* golden_dir = "tests/e2e/golden";
    bool verbose = false;
    bool update_golden = false;
};

extern E2ETestConfig g_e2e_config;

// ============================================================================
// Pixel - represents one PPM pixel
// ============================================================================
struct Pixel {
    uint8_t r, g, b;
    Pixel(uint8_t rr = 0, uint8_t gg = 0, uint8_t bb = 0) : r(rr), g(gg), b(bb) {}
};

// ============================================================================
// PixelStats - statistical analysis of a pixel region
// ============================================================================
struct PixelStats {
    int pixelCount = 0;
    int greenPixelCount = 0;
    int redPixelCount = 0;
    int bluePixelCount = 0;
    int nonBlackPixelCount = 0;
    float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f;
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    int centerX = 0, centerY = 0;
};

// ============================================================================
// PixelBounds - bounding box of pixels matching a predicate
// ============================================================================
struct PixelBounds {
    int minX = -1, maxX = -1, minY = -1, maxY = -1;
    bool valid = false;
    int count = 0;
    int centerX() const { return valid ? (minX + maxX) / 2 : -1; }
    int centerY() const { return valid ? (minY + maxY) / 2 : -1; }
};

// ============================================================================
// PPMVerifier - Load and verify PPM files
// ============================================================================
class PPMVerifier {
public:
    static constexpr uint32_t WIDTH  = FRAMEBUFFER_WIDTH;
    static constexpr uint32_t HEIGHT = FRAMEBUFFER_HEIGHT;

    PPMVerifier() = default;
    explicit PPMVerifier(const std::string& filename);

    bool load(const std::string& filename);
    Pixel getPixel(int x, int y) const;
    Pixel getPixelTL(int x, int y) const;
    bool isLoaded() const { return m_loaded; }

    template<typename Pred>
    int countPixels(Pred pred) const;

    int countPixelsInRegion(int x1, int y1, int x2, int y2, std::function<bool(const Pixel&)> pred) const;

    PixelStats analyzeRegion(int x1, int y1, int x2, int y2) const;

    bool assertPixelRGB(int x, int y, float er, float eg, float eb, float tolerance = 0.05f) const;

    // Compare with golden reference file
    // tolerance: per-pixel max channel error (0.0-1.0, default 0.01 = 1%)
    // Returns true if error rate <= 5%, with detailed mismatch count
    bool compareWithGolden(const std::string& goldenPath, float tolerance = 0.01f) const;

    // Generate golden reference PPM from current buffer
    // Saves as filename with "_golden.ppm" suffix
    bool generateGoldenReference(const std::string& filename) const;

    template<typename Pred>
    PixelBounds findBounds(Pred pred) const;

    int countGreenPixels(float threshold = 0.5f) const;
    int countRedPixels(float threshold = 0.5f) const;
    int countBluePixels(float threshold = 0.5f) const;
    int countNonBlackPixels(float threshold = 0.01f) const;

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

private:
    bool m_loaded = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<uint8_t> m_pixels;  // interleaved RGB
};

// ============================================================================
// E2ETest - Base class for E2E tests
// ============================================================================
class E2ETest : public ::testing::Test {
protected:
    std::unique_ptr<RenderPipeline> m_pipeline;
    std::string m_lastPPMFilename;
    std::string m_output_dir;

    void SetUp() override;
    void TearDown() override;

    // Batch rendering mode - for multi-triangle tests
    // beginFrame() starts collecting, endFrame() renders all at once
    void beginFrame();
    void addTriangle(const float* vertices, size_t vertexCount);
    void endFrame();

    // Legacy single-triangle render (for backward compatibility)
    void renderTriangle(const float* vertices, size_t vertexCount);
    std::string dumpPPM(const char* filename);

    const float* getColorBuffer() const;
    const float* getDepthBuffer() const;

    int countGreenPixelsFromBuffer(float threshold = 0.5f) const;
    int countRedPixelsFromBuffer(float threshold = 0.5f) const;
    int countBluePixelsFromBuffer(float threshold = 0.5f) const;

    bool isBufferPixelGreen(int x, int y, float threshold = 0.5f) const;
    bool isBufferPixelRed(int x, int y, float threshold = 0.5f) const;
    bool isBufferPixelBlue(int x, int y, float threshold = 0.5f) const;

    void getBufferPixelColor(int x, int y, float& r, float& g, float& b) const;
    float getBufferPixelDepth(int x, int y) const;

    PixelBounds getGreenBoundsFromBuffer() const;
    PixelBounds getRedBoundsFromBuffer() const;

    static std::array<float, 16> identityMatrix();

    // Get compiler-specific suffix for golden file differentiation
    // Returns "_gcc" for GCC, "_clang" for Clang, empty string otherwise
    static const char* getCompilerSuffix();

    // Get full golden file path with compiler-specific suffix
    // e.g., getGoldenPath("scene001") returns "tests/e2e/golden/scene001_gcc.ppm" on GCC
    static std::string getGoldenPath(const std::string& baseName);

private:
    // Batch rendering state - collected triangle data for multi-triangle tests
    bool m_batchMode = false;
    std::vector<float> m_batchVertices;
    size_t m_batchVertexCount = 0;
};

// ============================================================================
// Golden Reference Generator
// ============================================================================
namespace GoldenRef {
    bool pointInTriangle(float px, float py,
                         float v0x, float v0y,
                         float v1x, float v1y,
                         float v2x, float v2y);

    void generateFlatTrianglePPM(
        const char* filename,
        uint32_t width, uint32_t height,
        float v0x, float v0y, float v1x, float v1y, float v2x, float v2y,
        float cr, float cg, float cb,
        float bgR = 0.0f, float bgG = 0.0f, float bgB = 0.0f);
}
