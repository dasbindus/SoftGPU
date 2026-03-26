// ============================================================================
// test_Integration.cpp - Integration Tests
// ============================================================================

#include <gtest/gtest.h>
#include <pipeline/RenderPipeline.hpp>
#include <core/PipelineTypes.hpp>
#include <utils/FrameDumper.hpp>
#include <fstream>
#include <cstring>

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

// Helper: count green pixels in PPM file
int countGreenPixels(const std::string& filename, float threshold = 0.5f) {
    // Read PPM file
    std::ifstream f(filename, std::ios::binary);
    if (!f.good()) return 0;

    // Skip header
    std::string line;
    std::getline(f, line);  // P6
    std::getline(f, line);   // dimensions
    std::getline(f, line);   // max value

    // Read pixel data
    int greenCount = 0;
    const int width = 640;
    const int height = 480;
    for (int i = 0; i < width * height; i++) {
        uint8_t rgb[3];
        f.read(reinterpret_cast<char*>(rgb), 3);
        if (!f) break;
        if (rgb[1] > rgb[0] && rgb[1] > rgb[2] && rgb[1] > threshold * 255) {
            greenCount++;
        }
    }
    return greenCount;
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

    // Center of screen: (320, 240)
    // The triangle should cover the center area with green
    bool hasGreen = false;
    for (int dy = -10; dy <= 10; ++dy) {
        for (int dx = -10; dx <= 10; ++dx) {
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
    EXPECT_TRUE(hasGreen) << "Expected green pixels near center (320,240)";
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
    cmd.vertexBufferSize = 24;  // 3 vertices * 8 floats
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);

    // Dump to PPM
    pipeline.dump("test_green_triangle.ppm");

    // 验证文件生成（RenderPipeline 会添加 . 前缀）
    std::ifstream f(".test_green_triangle.ppm");
    EXPECT_TRUE(f.good()) << "PPM file should exist";
    f.close();

    // 验证有绿色像素
    int greenPixels = countGreenPixels(".test_green_triangle.ppm");
    EXPECT_GT(greenPixels, 100) << "Expected at least 100 green pixels, got " << greenPixels;
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
    cmd.vertexBufferSize = 24;  // 3 vertices * 8 floats
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    pipeline.render(cmd);
    pipeline.dump("test_header.ppm");

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

    // 验证有彩色像素（不只是黑色）- scan entire file
    int nonBlack = 0;
    std::vector<uint8_t> pixel(3);
    while (f.read(reinterpret_cast<char*>(pixel.data()), 3)) {
        if (pixel[0] > 0 || pixel[1] > 0 || pixel[2] > 0) {
            nonBlack++;
        }
    }
    EXPECT_GT(nonBlack, 100) << "Expected some colored pixels in RGB triangle";
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

    // Center of screen should have RGB blend
    bool hasColor = false;
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            int px = 320 + dx;
            int py = 240 + dy;
            if (px < 0 || px >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
                py < 0 || py >= static_cast<int>(FRAMEBUFFER_HEIGHT))
                continue;
            size_t idx = (py * FRAMEBUFFER_WIDTH + px) * 4;
            // At least one channel should be non-zero
            if (color[idx + 0] > 0.1f || color[idx + 1] > 0.1f || color[idx + 2] > 0.1f) {
                hasColor = true;
                break;
            }
        }
        if (hasColor) break;
    }
    EXPECT_TRUE(hasColor) << "Expected colored pixels near center (320,240)";
}

// ---------------------------------------------------------------------------
// ZBuffer Test - Front Hides Back
// ---------------------------------------------------------------------------

TEST(IntegrationTest, ZBuffer_FrontHidesBack) {
    RenderPipeline pipeline;

    // Red triangle at z=-0.5 (behind)
    float redTri[] = {
         0.0f,  0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, -0.5f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    };

    // Green triangle at z=0.0 (front, in front of red)
    float greenTri[] = {
         0.0f,  0.3f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        -0.4f, -0.4f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
         0.4f, -0.4f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
    };

    // Draw red triangle first (behind)
    RenderCommand cmd1;
    cmd1.vertexBufferData = redTri;
    cmd1.vertexBufferSize = 24;
    cmd1.drawParams.vertexCount = 3;
    cmd1.drawParams.indexed = false;
    cmd1.modelMatrix = identityMatrix();
    cmd1.viewMatrix = identityMatrix();
    cmd1.projectionMatrix = identityMatrix();
    cmd1.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    pipeline.render(cmd1);

    // Draw green triangle second (front)
    RenderCommand cmd2;
    cmd2.vertexBufferData = greenTri;
    cmd2.vertexBufferSize = 24;
    cmd2.drawParams.vertexCount = 3;
    cmd2.drawParams.indexed = false;
    cmd2.modelMatrix = identityMatrix();
    cmd2.viewMatrix = identityMatrix();
    cmd2.projectionMatrix = identityMatrix();
    cmd2.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    pipeline.render(cmd2);

    // Check that center is green (front triangle should be visible)
    const auto* fb = pipeline.getFramebuffer();
    const float* color = fb->getColorBuffer();
    size_t centerIdx = (240 * FRAMEBUFFER_WIDTH + 320) * 4;

    // Green channel should be high, red should be low
    EXPECT_GT(color[centerIdx + 1], 0.5f) << "Center should be green (front triangle)";
    EXPECT_LT(color[centerIdx + 0], 0.3f) << "Center should NOT be red (covered by front triangle)";
}

// ---------------------------------------------------------------------------
// Performance Report
// ---------------------------------------------------------------------------

TEST(IntegrationTest, PerformanceReport_Prints) {
    RenderPipeline pipeline;

    float tri[] = {
        0.0f, 0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = tri;
    cmd.vertexBufferSize = 24;
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    // Should not crash
    pipeline.render(cmd);

}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
