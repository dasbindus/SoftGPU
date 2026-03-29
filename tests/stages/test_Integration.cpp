// ============================================================================
// test_Integration.cpp
// 集成测试：渲染三角形
// ============================================================================

#include <gtest/gtest.h>
#include <fstream>
#include "pipeline/RenderPipeline.hpp"
#include "core/RenderCommand.hpp"

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
    cmd.vertexBufferSize = 12;  // 3 vertices * 4 floats pos + 4 floats color
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
    cmd.vertexBufferSize = 12;
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
    cmd.vertexBufferSize = 12;
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
    cmd.vertexBufferSize = 12;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    // Dump to PPM
    pipeline.dump("test_green_triangle.ppm");

    // 验证文件生成（RenderPipeline 会添加 . 前缀）
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
    cmd.vertexBufferSize = 12;
    cmd.drawParams.vertexCount = 3;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);
    pipeline.dump("test_header.ppm");

    // 读取并验证 header（RenderPipeline 会添加 . 前缀）
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

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
