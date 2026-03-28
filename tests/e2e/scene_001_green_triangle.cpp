// ============================================================================
// scene_001_green_triangle.cpp - E2E Scene 001: Green Triangle
//
// Test: E2E-SCENE-001
// Target: Verify basic green triangle renders correctly
//
// Verifications:
//   ✓ PPM file generates successfully
//   ✓ Green pixel count > 20000 (triangle area)
//   ✓ Green pixel bounding box centered correctly
//   ✓ No extraneous color pixels (only green dominates)
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 001: Green Triangle Rendering
// ============================================================================

// ---------------------------------------------------------------------------
// Test: Green triangle renders with correct color
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene001_GreenTriangle_RendersGreenPixels) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    int greenCount = countGreenPixelsFromBuffer();
    EXPECT_GT(greenCount, 20000)
        << "Green triangle should have > 20000 green pixels, got " << greenCount;

    int redCount = countRedPixelsFromBuffer();
    int blueCount = countBluePixelsFromBuffer();
    EXPECT_EQ(redCount, 0) << "No red pixels expected in green triangle";
    EXPECT_EQ(blueCount, 0) << "No blue pixels expected in green triangle";
}

// ---------------------------------------------------------------------------
// Test: Green triangle bounding box is centered on screen
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene001_GreenTriangle_BoundingBoxCentered) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    PixelBounds bounds = getGreenBoundsFromBuffer();
    ASSERT_TRUE(bounds.valid) << "Green pixel bounding box should be valid";

    int centerX = (bounds.minX + bounds.maxX) / 2;
    int centerY = (bounds.minY + bounds.maxY) / 2;

    EXPECT_NEAR(centerX, 320, 80)
        << "Triangle center X should be near 320, got " << centerX;
    EXPECT_NEAR(centerY, 240, 80)
        << "Triangle center Y should be near 240, got " << centerY;

    int width = bounds.maxX - bounds.minX;
    int height = bounds.maxY - bounds.minY;
    EXPECT_GT(width, 100) << "Triangle width should be > 100 pixels";
    EXPECT_GT(height, 100) << "Triangle height should be > 100 pixels";
}

// ---------------------------------------------------------------------------
// Test: PPM file generated with correct format
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene001_GreenTriangle_PPMDumpCorrect) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    std::string ppmPath = dumpPPM("e2e_green_triangle.ppm");

    std::ifstream f(ppmPath);
    EXPECT_TRUE(f.good()) << "PPM file should exist at: " << ppmPath;
    f.close();

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    EXPECT_EQ(verifier.width(), 640u);
    EXPECT_EQ(verifier.height(), 480u);

    int greenCount = verifier.countGreenPixels(0.5f);
    EXPECT_GT(greenCount, 20000)
        << "PPM green pixel count should be > 20000, got " << greenCount;
}

// ---------------------------------------------------------------------------
// Test: Green pixels are in upper portion of screen (triangle is top-centered)
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene001_GreenTriangle_TopCenteredPosition) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    PixelBounds bounds = getGreenBoundsFromBuffer();
    ASSERT_TRUE(bounds.valid);

    // Top of triangle should be in upper half of screen
    EXPECT_LT(bounds.minY, 300)
        << "Top of triangle should be in upper portion (y < 300), got " << bounds.minY;
}

// ---------------------------------------------------------------------------
// Test: Triangle interior has solid green (no large holes)
// Note: Some holes due to rasterizer discretization are acceptable
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene001_GreenTriangle_NoLargeHoles) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    PixelBounds bounds = getGreenBoundsFromBuffer();
    ASSERT_TRUE(bounds.valid);

    int totalChecks = 0;
    int greenChecks = 0;

    for (int y = bounds.minY + 5; y < bounds.maxY - 5; y += 10) {
        for (int x = bounds.minX + 5; x < bounds.maxX - 5; x += 10) {
            totalChecks++;
            if (isBufferPixelGreen(x, y)) greenChecks++;
        }
    }

    float fillRatio = static_cast<float>(greenChecks) / totalChecks;
    // Rasterizer may have gaps; require > 40% fill ratio
    EXPECT_GT(fillRatio, 0.40f)
        << "Triangle fill ratio should be > 40%, got " << (fillRatio * 100) << "%";
}
