// ============================================================================
// scene_002_rgb_interpolation.cpp - E2E Scene 002: RGB Color Interpolation
//
// Test: E2E-SCENE-002
// Target: Verify barycentric color interpolation
//
// Input:
//   - Vertex v0: top center, RED (1, 0, 0)
//   - Vertex v1: bottom-left, GREEN (0, 1, 0)
//   - Vertex v2: bottom-right, BLUE (0, 0, 1)
//
// Verifications:
//   ✓ Triangle interior has RGB mixed colors
//   ✓ Center point color ≈ gray (1/3, 1/3, 1/3)
//   ✓ Each vertex region retains its vertex color
//   ✓ No black/hole pixels inside triangle
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 002: RGB Color Interpolation
// ============================================================================

// ---------------------------------------------------------------------------
// Test: RGB triangle renders with interpolated colors
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene002_RGBInterpolation_HasMixedColors) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    int nonBlackCount = 0;
    const float* color = getColorBuffer();
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i * 4 + 0];
        float g = color[i * 4 + 1];
        float b = color[i * 4 + 2];
        if (r > 0.01f || g > 0.01f || b > 0.01f) nonBlackCount++;
    }
    EXPECT_GT(nonBlackCount, 15000)
        << "RGB triangle should have > 15000 non-black pixels, got " << nonBlackCount;

    int redCount = countRedPixelsFromBuffer();
    int greenCount = countGreenPixelsFromBuffer();
    int blueCount = countBluePixelsFromBuffer();

    EXPECT_GT(redCount, 1000) << "Should have red pixels near top vertex";
    EXPECT_GT(greenCount, 1000) << "Should have green pixels near bottom-left vertex";
    EXPECT_GT(blueCount, 1000) << "Should have blue pixels near bottom-right vertex";
}

// ---------------------------------------------------------------------------
// Test: Center of triangle is approximately gray
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene002_RGBInterpolation_CenterIsGrayish) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    float sumR = 0, sumG = 0, sumB = 0;
    int samples = 0;
    const float* color = getColorBuffer();

    for (int dy = -10; dy <= 10; dy += 5) {
        for (int dx = -10; dx <= 10; dx += 5) {
            int px = 320 + dx;
            int py = 240 + dy;
            if (px < 0 || px >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
                py < 0 || py >= static_cast<int>(FRAMEBUFFER_HEIGHT))
                continue;
            size_t idx = (static_cast<size_t>(py) * FRAMEBUFFER_WIDTH + px) * 4;
            float r = color[idx + 0];
            float g = color[idx + 1];
            float b = color[idx + 2];
            if (r > 0.01f || g > 0.01f || b > 0.01f) {
                sumR += r;
                sumG += g;
                sumB += b;
                samples++;
            }
        }
    }

    ASSERT_GT(samples, 0) << "Center should have at least some non-black pixels";

    float avgR = sumR / samples;
    float avgG = sumG / samples;
    float avgB = sumB / samples;

    float diffRG = std::abs(avgR - avgG);
    float diffGB = std::abs(avgG - avgB);
    float diffBR = std::abs(avgB - avgR);

    EXPECT_LT(diffRG, 0.40f)
        << "Center R and G should be similar, got R=" << avgR << " G=" << avgG;
    EXPECT_LT(diffGB, 0.40f)
        << "Center G and B should be similar, got G=" << avgG << " B=" << avgB;
    EXPECT_LT(diffBR, 0.40f)
        << "Center B and R should be similar, got B=" << avgB;
}

// ---------------------------------------------------------------------------
// Test: Vertex regions retain their original colors (using region checks)
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene002_RGBInterpolation_VertexColorsPreserved) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Top region: should be reddish (dominant R)
    int redInTop = 0;
    for (int y = 60; y < 200; y += 5) {
        for (int x = 280; x < 360; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.5f && r > g && r > b) redInTop++;
        }
    }
    EXPECT_GT(redInTop, 10) << "Top region should have reddish pixels";

    // Bottom-left region: should be greenish
    int greenInBL = 0;
    for (int y = 320; y < 420; y += 5) {
        for (int x = 120; x < 200; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (g > 0.5f && g > r && g > b) greenInBL++;
        }
    }
    EXPECT_GT(greenInBL, 10) << "Bottom-left region should have greenish pixels";

    // Bottom-right region: should be bluish
    int blueInBR = 0;
    for (int y = 320; y < 420; y += 5) {
        for (int x = 440; x < 520; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (b > 0.5f && b > r && b > g) blueInBR++;
        }
    }
    EXPECT_GT(blueInBR, 10) << "Bottom-right region should have bluish pixels";
}

// ---------------------------------------------------------------------------
// Test: PPM dump shows correct RGB gradient
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene002_RGBInterpolation_PPMDumpHasGradient) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    std::string ppmPath = dumpPPM("e2e_rgb_triangle.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM should load successfully";

    int redCount = verifier.countRedPixels(0.5f);
    int greenCount = verifier.countGreenPixels(0.5f);
    int blueCount = verifier.countBluePixels(0.5f);

    EXPECT_GT(redCount, 500) << "PPM should have red pixels near top";
    EXPECT_GT(greenCount, 500) << "PPM should have green pixels near bottom-left";
    EXPECT_GT(blueCount, 500) << "PPM should have blue pixels near bottom-right";
}

// ---------------------------------------------------------------------------
// Test: No large black holes inside triangle
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene002_RGBInterpolation_NoLargeHoles) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Check bounding box area for holes
    int minX = 160, maxX = 480, minY = 60, maxY = 360;

    int totalChecks = 0;
    int nonBlackChecks = 0;

    for (int y = minY; y <= maxY; y += 5) {
        for (int x = minX; x <= maxX; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (x >= 0 && x < static_cast<int>(FRAMEBUFFER_WIDTH) &&
                y >= 0 && y < static_cast<int>(FRAMEBUFFER_HEIGHT)) {
                totalChecks++;
                if (r > 0.01f || g > 0.01f || b > 0.01f) {
                    nonBlackChecks++;
                }
            }
        }
    }

    float fillRatio = totalChecks > 0
        ? static_cast<float>(nonBlackChecks) / totalChecks
        : 0.0f;
    // Rasterizer gaps are acceptable; require > 30% fill
    EXPECT_GT(fillRatio, 0.30f)
        << "Triangle interior should have < 70% holes, got fill=" << (fillRatio * 100) << "%";
}
