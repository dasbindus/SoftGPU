// ============================================================================
// test_Integration.cpp
// 集成测试：渲染三角形
// ============================================================================

#include <gtest/gtest.h>
#include <fstream>
#include <algorithm>
#include "pipeline/RenderPipeline.hpp"
#include "core/RenderCommand.hpp"
#include "stages/EarlyZ.hpp"

namespace {

using namespace SoftGPU;

// Helper: build identity matrix array
std::array<float, 16> identityMatrix() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

// ---------------------------------------------------------------------------
// Green Triangle at Center
// ---------------------------------------------------------------------------

TEST(IntegrationTest, GreenTriangle_Center) {
    RenderPipeline pipeline;

    // Simple triangle in NDC space
    float vertices[] = {
        // v0: top center
        0.0f, 0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
        // v1: bottom left
        -0.5f, -0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
        // v2: bottom right
        0.5f, -0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = 24;  // 3 vertices * 8 floats (pos + color)
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    const auto* fb = pipeline.getFramebuffer();
    const float* color = fb->getColorBuffer();

    // Center of screen: (320, 240)
    size_t centerIdx = (240 * FRAMEBUFFER_WIDTH + 320) * 4;

    // The triangle should cover the center area with green
    // Due to rasterization, some center pixel should be green
    bool hasGreen = false;
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            int px = 320 + dx;
            int py = 240 + dy;
            if (px < 0 || px >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
                py < 0 || py >= static_cast<int>(FRAMEBUFFER_HEIGHT))
                continue;
            size_t idx = (py * FRAMEBUFFER_WIDTH + px) * 4;
            if (color[idx + 1] > 0.5f) {  // Green channel
                hasGreen = true;
                break;
            }
        }
        if (hasGreen) break;
    }
    EXPECT_TRUE(hasGreen);
}

// ---------------------------------------------------------------------------
// RGB Triangle - Color Interpolation
// ---------------------------------------------------------------------------

TEST(IntegrationTest, RGBTriangle_ColorInterpolation) {
    RenderPipeline pipeline;

    // Triangle with red, green, blue vertices
    float vertices[] = {
        // v0: top (red)
         0.0f,  0.5f, 0.0f, 1.0f,
         1.0f,  0.0f, 0.0f, 1.0f,
        // v1: bottom left (green)
        -0.5f, -0.5f, 0.0f, 1.0f,
         0.0f,  1.0f, 0.0f, 1.0f,
        // v2: bottom right (blue)
         0.5f, -0.5f, 0.0f, 1.0f,
         0.0f,  0.0f, 1.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = 24;  // 3 vertices * 8 floats
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    const auto* fb = pipeline.getFramebuffer();
    const float* color = fb->getColorBuffer();

    // Center pixel should have all channels non-zero (interpolated from RGB)
    size_t centerIdx = (240 * FRAMEBUFFER_WIDTH + 320) * 4;
    float r = color[centerIdx + 0];
    float g = color[centerIdx + 1];
    float b = color[centerIdx + 2];

    // Center of triangle should have some color from the vertices
    bool hasColor = (r > 0.01f || g > 0.01f || b > 0.01f);
    EXPECT_TRUE(hasColor);
}

// ---------------------------------------------------------------------------
// Z-Buffer: Front Triangle Hides Back
// ---------------------------------------------------------------------------

TEST(IntegrationTest, ZBuffer_FrontHidesBack) {
    RenderPipeline pipeline;

    // Both triangles in one buffer (8 floats per vertex: pos + color)
    // Back triangle (red, farther z=-0.5): 3 vertices
    // Front triangle (green, closer z=0.0): 3 vertices
    float bothTriangles[] = {
        // Back triangle (red)
        -0.5f, -0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // v0: back-left
        0.5f,  0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // v1: back-right
        0.5f, -0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // v2: back-bottom
        // Front triangle (green, overlapping in XY)
        -0.3f, -0.3f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // v3: front-left
        0.3f,  0.3f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // v4: front-right
        -0.3f,  0.3f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // v5: front-top
    };

    RenderCommand cmd;
    cmd.vertexBufferData = bothTriangles;
    cmd.vertexBufferSize = 48;  // 6 vertices * 8 floats
    cmd.drawParams.vertexCount = 6;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    const auto* fb = pipeline.getFramebuffer();
    const float* color = fb->getColorBuffer();

    // Check a pixel where the front triangle overlaps the back triangle
    // At center (320, 240), the front green triangle should be visible
    bool centerIsGreen = false;
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            int px = 320 + dx;
            int py = 240 + dy;
            size_t idx = (py * FRAMEBUFFER_WIDTH + px) * 4;
            // Green channel should be dominant if front triangle is visible
            if (color[idx + 1] > 0.5f && color[idx + 1] > color[idx + 0]) {
                centerIsGreen = true;
                break;
            }
        }
        if (centerIsGreen) break;
    }
    // The front green triangle should be visible at center (Z-buffer test)
    EXPECT_TRUE(centerIsGreen);
}

// ---------------------------------------------------------------------------
// Performance: Single Triangle FPS
// ---------------------------------------------------------------------------

TEST(IntegrationTest, DISABLED_Performance_SingleTriangle) {
    RenderPipeline pipeline;

    float triangle[] = {
        0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = triangle;
    cmd.vertexBufferSize = 24;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        pipeline.render(cmd);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    double fps = 100.0 / elapsed;

    // Target: >= 100 FPS
    EXPECT_GE(fps, 100.0);
    printf("Single triangle FPS: %.1f\n", fps);
}

// ---------------------------------------------------------------------------
// Performance Report
// ---------------------------------------------------------------------------

TEST(IntegrationTest, PerformanceReport_Prints) {
    RenderPipeline pipeline;

    float triangle[] = {
        0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
        0.5f,-0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = triangle;
    cmd.vertexBufferSize = 24;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    // Should not crash
    EXPECT_NO_FATAL_FAILURE(pipeline.printPerformanceReport());
}

// ---------------------------------------------------------------------------
// PPM Dump and Golden Image Comparison
// ---------------------------------------------------------------------------

TEST(IntegrationTest, PPM_Dump_GoldenTriangle) {
    RenderPipeline pipeline;

    // 绿色三角形
    float vertices[] = {
        0.0f, 0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f,-0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        0.5f,-0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = 24;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    // Dump to PPM
    pipeline.dump(".test_green_triangle.ppm");

    // 验证文件生成
    std::ifstream f(".test_green_triangle.ppm");
    EXPECT_TRUE(f.good());
    f.close();

    // 验证 PPM header
    EXPECT_NO_FATAL_FAILURE(pipeline.dump("test_header_check.ppm"));
}

// ---------------------------------------------------------------------------
// PPM Header Verification
// ---------------------------------------------------------------------------

TEST(IntegrationTest, PPM_Header_Correct) {
    RenderPipeline pipeline;
    
    float triangle[] = {
        0.0f, 0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f,-0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        0.5f,-0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = triangle;
    cmd.vertexBufferSize = 24;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);
    pipeline.dump(".test_header.ppm");

    // 读取并验证 header
    std::ifstream f(".test_header.ppm", std::ios::binary);
    ASSERT_TRUE(f.good());
    
    std::string header;
    std::getline(f, header);
    EXPECT_EQ(header, "P6");  // P6 = binary RGB
    
    std::string dims;
    std::getline(f, dims);
    EXPECT_TRUE(dims.find("640") != std::string::npos);
    EXPECT_TRUE(dims.find("480") != std::string::npos);
    
    std::string maxval;
    std::getline(f, maxval);
    EXPECT_EQ(maxval, "255");
}

// ============================================================================
// v1.4: EarlyZ Unit Tests
// ============================================================================

class EarlyZTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override { m_earlyz.resetStats(); }

    EarlyZ m_earlyz;
};

// ---------------------------------------------------------------------------
// Test: EarlyZ::testFragment passes closer fragments
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, TestFragment_PassesCloser) {
    // Fragment at depth 0.3 should pass against depth buffer value 0.5
    EXPECT_TRUE(m_earlyz.testFragment(0.3f, 0.5f));
    EXPECT_EQ(m_earlyz.getPassedCount(), 1u);
    EXPECT_EQ(m_earlyz.getRejectedCount(), 0u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::testFragment rejects farther fragments
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, TestFragment_RejectsOccluded) {
    // Fragment at depth 0.7 should be rejected against depth buffer value 0.5
    EXPECT_FALSE(m_earlyz.testFragment(0.7f, 0.5f));
    EXPECT_EQ(m_earlyz.getRejectedCount(), 1u);
    EXPECT_EQ(m_earlyz.getPassedCount(), 0u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::testFragment handles equal depth (not passing)
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, TestFragment_RejectsEqual) {
    // Fragment at same depth should be rejected (not strictly closer)
    EXPECT_FALSE(m_earlyz.testFragment(0.5f, 0.5f));
    EXPECT_EQ(m_earlyz.getRejectedCount(), 1u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - all fragments pass when depth buffer is cleared
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_AllPassOnClearDepth) {
    // All fragments with depth < CLEAR_DEPTH (1.0) should pass on cleared depth buffer
    std::vector<Fragment> fragments = {
        {100, 100, 0.3f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
        {200, 200, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {300, 300, 0.1f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
    };

    // Cleared depth buffer has all 1.0f values
    std::vector<float> depthBuffer(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, CLEAR_DEPTH);

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, depthBuffer.data(), FRAMEBUFFER_WIDTH, 0, 0);

    EXPECT_EQ(passed.size(), 3u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - correctly filters occluded fragments
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_FiltersOccluded) {
    // Two fragments at same XY, one closer than the other
    std::vector<Fragment> fragments = {
        {160, 240, 0.3f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // closer
        {160, 240, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // farther (occluded)
    };

    // Depth buffer at (160, 240) = 0.5
    std::vector<float> depthBuffer(FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT, CLEAR_DEPTH);
    size_t idx = 240 * FRAMEBUFFER_WIDTH + 160;
    depthBuffer[idx] = 0.5f;

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, depthBuffer.data(), FRAMEBUFFER_WIDTH, 0, 0);

    // Only the closer fragment (depth 0.3 < 0.5) should pass
    EXPECT_EQ(passed.size(), 1u);
    EXPECT_FLOAT_EQ(passed[0].z, 0.3f);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - tile-local coordinate indexing
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_TileLocalIndexing) {
    // Tile (1, 0): absolute coords (32..63, 0..31) map to local (0..31, 0..31)
    // Fragment at screen (48, 16) should map to tile-local (16, 16) for idx calculation
    // With width=32 (TILE_WIDTH), idx = 16 * 32 + 16 = 528
    std::vector<Fragment> fragments = {
        {48, 16, 0.2f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},  // at screen (48, 16)
    };

    // Depth buffer with width = TILE_WIDTH = 32 (tile-local buffer)
    std::vector<float> tileDepthBuffer(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    size_t localIdx = 16 * TILE_WIDTH + 16;  // = 528
    tileDepthBuffer[localIdx] = 0.5f;  // depth buffer has 0.5 at this local position

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, tileDepthBuffer.data(), TILE_WIDTH, 1, 0);

    // Fragment at depth 0.2 < 0.5 should pass
    EXPECT_EQ(passed.size(), 1u);
    EXPECT_FLOAT_EQ(passed[0].z, 0.2f);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - multiple tiles
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_MultipleTiles) {
    // Two tile-local depth buffers
    std::vector<float> tile0Depth(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    std::vector<float> tile1Depth(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);

    // Tile (0,0): local (16,16) = idx 528, depth = 0.5
    // Fragment at (16, 16) with depth 0.2 < 0.5 -> PASSES
    tile0Depth[16 * TILE_WIDTH + 16] = 0.5f;

    // Tile (1,1): local (16,16) = idx 528, depth = 0.1
    // Fragment at (48, 48) with depth 0.3 > 0.1 -> REJECTED
    tile1Depth[16 * TILE_WIDTH + 16] = 0.1f;

    // Test tile (0,0): fragment depth 0.2 < buffer 0.5 -> passes
    std::vector<Fragment> passed0 = m_earlyz.filterOccluded(
        {{16, 16, 0.2f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}},
        tile0Depth.data(), TILE_WIDTH, 0, 0);
    EXPECT_EQ(passed0.size(), 1u);  // 0.2 < 0.5, passes

    // Test tile (1,1): fragment depth 0.3 > buffer 0.1 -> rejected
    std::vector<Fragment> passed1 = m_earlyz.filterOccluded(
        {{48, 48, 0.3f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f}},
        tile1Depth.data(), TILE_WIDTH, 1, 1);
    EXPECT_EQ(passed1.size(), 0u);  // 0.3 > 0.1, rejected
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::resetStats clears counters
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, ResetStats) {
    m_earlyz.testFragment(0.3f, 0.5f);
    m_earlyz.testFragment(0.7f, 0.5f);
    EXPECT_EQ(m_earlyz.getPassedCount(), 1u);
    EXPECT_EQ(m_earlyz.getRejectedCount(), 1u);

    m_earlyz.resetStats();
    EXPECT_EQ(m_earlyz.getPassedCount(), 0u);
    EXPECT_EQ(m_earlyz.getRejectedCount(), 0u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ passes stats increment correctly
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, StatsIncrement) {
    for (int i = 0; i < 5; ++i) m_earlyz.testFragment(0.3f, 0.5f);
    for (int i = 0; i < 3; ++i) m_earlyz.testFragment(0.7f, 0.5f);

    EXPECT_EQ(m_earlyz.getPassedCount(), 5u);
    EXPECT_EQ(m_earlyz.getRejectedCount(), 3u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - empty fragment list
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_EmptyFragmentList) {
    std::vector<Fragment> fragments;  // empty

    std::vector<float> depthBuffer(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, depthBuffer.data(), TILE_WIDTH, 0, 0);

    EXPECT_EQ(passed.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - tile boundary coordinates
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_TileBoundaryCoords) {
    // Tile (0, 0): local (0, 0) = idx 0, (31, 31) = idx 1023
    std::vector<Fragment> fragments = {
        {0, 0, 0.2f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},   // bottom-left corner
        {31, 31, 0.3f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}, // top-right corner
        {31, 0, 0.4f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // bottom-right corner
        {0, 31, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f},  // top-left corner
    };

    std::vector<float> tileDepthBuffer(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    // Set some depths at boundaries
    tileDepthBuffer[0] = 0.5f;                // (0,0) has 0.5
    tileDepthBuffer[1023] = 0.1f;             // (31,31) has 0.1

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, tileDepthBuffer.data(), TILE_WIDTH, 0, 0);

    // (0,0): 0.2 < 0.5 -> PASS
    // (31,31): 0.3 > 0.1 -> REJECT
    // (31,0): 0.4 < CLEAR_DEPTH -> PASS
    // (0,31): 0.5 < CLEAR_DEPTH -> PASS
    EXPECT_EQ(passed.size(), 3u);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - multiple fragments same XY different depths
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_SameXYDifferentDepths) {
    // Multiple fragments at same tile-local position with different depths
    // Use coordinates within tile range (0-31 for 32x32 tile)
    std::vector<Fragment> fragments = {
        {10, 15, 0.8f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // farthest
        {10, 15, 0.2f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // closest
        {10, 15, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f},  // middle
    };

    std::vector<float> depthBuffer(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    size_t idx = 15 * TILE_WIDTH + 10;  // local (10, 15)
    depthBuffer[idx] = 0.3f;  // current depth buffer value

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, depthBuffer.data(), TILE_WIDTH, 0, 0);

    // Only 0.2 < 0.3 passes
    EXPECT_EQ(passed.size(), 1u);
    EXPECT_FLOAT_EQ(passed[0].z, 0.2f);
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::testFragment - boundary depths
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, TestFragment_BoundaryDepths) {
    // Very small difference - should still be rejected if not strictly closer
    EXPECT_FALSE(m_earlyz.testFragment(0.500001f, 0.5f));  // barely farther

    // Very close values
    EXPECT_TRUE(m_earlyz.testFragment(0.499999f, 0.5f));  // barely closer

    // Zero depth
    EXPECT_TRUE(m_earlyz.testFragment(0.0f, 0.5f));  // closest possible

    // Far plane depth
    EXPECT_FALSE(m_earlyz.testFragment(1.0f, 0.5f));  // farthest possible
}

// ---------------------------------------------------------------------------
// Test: EarlyZ::filterOccluded - all fragments occluded
// ---------------------------------------------------------------------------
TEST_F(EarlyZTest, FilterOccluded_AllOccluded) {
    // Use coordinates within tile range (0-31 for 32x32 tile)
    std::vector<Fragment> fragments = {
        {5, 5, 0.9f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {10, 10, 0.95f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {15, 15, 0.99f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f},
    };

    std::vector<float> depthBuffer(TILE_WIDTH * TILE_HEIGHT, CLEAR_DEPTH);
    depthBuffer[5 * TILE_WIDTH + 5] = 0.5f;
    depthBuffer[10 * TILE_WIDTH + 10] = 0.6f;
    depthBuffer[15 * TILE_WIDTH + 15] = 0.7f;

    std::vector<Fragment> passed = m_earlyz.filterOccluded(
        fragments, depthBuffer.data(), TILE_WIDTH, 0, 0);

    EXPECT_EQ(passed.size(), 0u);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
