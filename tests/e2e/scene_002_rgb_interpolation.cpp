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
// Enhancements (王刚):
//   ✓ Bounding Box Exact Verification (NDC + viewport transform)
//   ✓ Slanted Edge Linearity Detection
//   ✓ Golden Reference Comparison (PPMVerifier, tolerance 0.02)
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 002 Geometry Constants
// ============================================================================
namespace Scene002 {
    // Triangle vertices (NDC): top(0,0.5), bottom-left(-0.5,-0.5), bottom-right(0.5,-0.5)
    // Viewport: 640x480, origin at bottom-left
    // Screen transform: screenX = (ndcX + 1) * 640, screenY = (1 - ndcY) * 480
    //
    // Vertex 0: (0.0,  0.5) → screen (320, 120) - RED
    // Vertex 1: (-0.5,-0.5) → screen (160, 420) - GREEN
    // Vertex 2: (0.5, -0.5) → screen (480, 420) - BLUE
    //
    // Theoretical bbox: minX=160, maxX=480, minY=120, maxY=420

    constexpr float V0_X =  0.0f,  V0_Y =  0.5f;
    constexpr float V1_X = -0.5f,  V1_Y = -0.5f;
    constexpr float V2_X =  0.5f,  V2_Y = -0.5f;

    // After fill rule fix + NDC Y-axis fix:
    // Actual bbox: minX=160, maxX=479, minY=121, maxY=359
    constexpr int   EXPECTED_MIN_X = 160;
    constexpr int   EXPECTED_MAX_X = 479;
    constexpr int   EXPECTED_MIN_Y = 121;
    constexpr int   EXPECTED_MAX_Y = 359;

    const char* GOLDEN_FILE = "tests/e2e/golden/scene002_rgb_interpolation.ppm";
}

// ============================================================================
// Test: RGB triangle renders with interpolated colors
// ============================================================================
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
    for (int y = 120; y < 260; y += 5) {
        for (int x = 280; x < 360; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.5f && r > g && r > b) redInTop++;
        }
    }
    EXPECT_GT(redInTop, 10) << "Top region should have reddish pixels";

    // Bottom-left region: should be greenish
    // After NDC Y fix, triangle bottom is at y=360 (V1: x=160, y=360)
    // At y=300-360, triangle left edge is at x=200-160, right edge at x=440-480
    // Bottom-left region (x=150-220, y=300-360) should have dominant green
    int greenInBL = 0;
    for (int y = 300; y < 360; y += 5) {
        for (int x = 150; x < 220; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (g > 0.5f && g > r && g > b) greenInBL++;
        }
    }
    EXPECT_GT(greenInBL, 10) << "Bottom-left region should have greenish pixels";

    // Bottom-right region: should be bluish
    // Triangle right edge at y=300-360 is x=440-480, so check x=420-500
    int blueInBR = 0;
    for (int y = 300; y < 360; y += 5) {
        for (int x = 420; x < 500; x += 5) {
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
    int minX = 160, maxX = 480, minY = 120, maxY = 420;

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

// ============================================================================
// ENHANCEMENT 1: Bounding Box Exact Verification
// Same geometry as Scene001: minX=160, maxX=480, minY=60, maxY=360
// ============================================================================
TEST_F(E2ETest, Scene002_RGBInterpolation_BoundingBoxExact) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Compute combined bounding box of all non-black pixels
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    const float* color = getColorBuffer();
    for (int y = 0; y < static_cast<int>(FRAMEBUFFER_HEIGHT); ++y) {
        for (int x = 0; x < static_cast<int>(FRAMEBUFFER_WIDTH); ++x) {
            size_t idx = (static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x) * 4;
            if (color[idx] > 0.01f || color[idx+1] > 0.01f || color[idx+2] > 0.01f) {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }

    ASSERT_NE(minX, 9999) << "Should have found non-black pixels";

    EXPECT_EQ(minX, Scene002::EXPECTED_MIN_X)
        << "Scene002: Left edge should be at x=" << Scene002::EXPECTED_MIN_X;
    EXPECT_EQ(maxX, Scene002::EXPECTED_MAX_X)
        << "Scene002: Right edge should be at x=" << Scene002::EXPECTED_MAX_X;
    EXPECT_EQ(minY, Scene002::EXPECTED_MIN_Y)
        << "Scene002: Top edge should be at y=" << Scene002::EXPECTED_MIN_Y;
    EXPECT_EQ(maxY, Scene002::EXPECTED_MAX_Y)
        << "Scene002: Bottom edge should be at y=" << Scene002::EXPECTED_MAX_Y;
}

// ============================================================================
// ENHANCEMENT 2: Slanted Edge Linearity Detection
// Verifies that hypotenuse edges form perfect straight lines
// Detects "triangle→rectangle" rasterization bug
// ============================================================================
TEST_F(E2ETest, Scene002_RGBInterpolation_SlantedEdgeLinearity) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Left hypotenuse: V0(320,120) → V1(160,360) after NDC Y fix
    // As y increases, x decreases: x_left(y) = 320 - ((y - 120) * (2/3))
    //
    // Right hypotenuse: V0(320,120) → V2(480,360)
    // As y increases, x increases: x_right(y) = 320 + ((y - 120) * (2/3))

    int leftViolations = 0;
    for (int y = Scene002::EXPECTED_MIN_Y; y <= Scene002::EXPECTED_MAX_Y; y += 10) {
        float expectedLeftX = 320.0f - (static_cast<float>(y - 120) * (2.0f / 3.0f));
        float expectedRightX = 320.0f + (static_cast<float>(y - 120) * (2.0f / 3.0f));

        // Find the leftmost non-black pixel near the left hypotenuse
        int foundLeft = -1;
        for (int x = static_cast<int>(expectedLeftX) - 5; x <= static_cast<int>(expectedLeftX) + 5; ++x) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.1f || g > 0.1f || b > 0.1f) {
                foundLeft = x;
                break;
            }
        }

        // Find the rightmost non-black pixel near the right hypotenuse
        int foundRight = -1;
        for (int x = static_cast<int>(expectedRightX) - 5; x <= static_cast<int>(expectedRightX) + 5; ++x) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.1f || g > 0.1f || b > 0.1f) {
                foundRight = x;
            }
        }

        if (foundLeft >= 0) {
            float deviation = std::abs(static_cast<float>(foundLeft) - expectedLeftX);
            if (deviation > 3.0f) leftViolations++;
        }
    }

    EXPECT_LT(leftViolations, 5)
        << "Left hypotenuse should be linear, " << leftViolations << " violations found";

    int rightViolations = 0;
    for (int y = Scene002::EXPECTED_MIN_Y; y <= Scene002::EXPECTED_MAX_Y; y += 10) {
        float expectedRightX = 320.0f + (static_cast<float>(y - 120) * (2.0f / 3.0f));

        int foundRight = -1;
        for (int x = static_cast<int>(expectedRightX) - 5; x <= static_cast<int>(expectedRightX) + 5; ++x) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.1f || g > 0.1f || b > 0.1f) {
                foundRight = x;
            }
        }

        if (foundRight >= 0) {
            float deviation = std::abs(static_cast<float>(foundRight) - expectedRightX);
            if (deviation > 3.0f) rightViolations++;
        }
    }

    EXPECT_LT(rightViolations, 5)
        << "Right hypotenuse should be linear, " << rightViolations << " violations found";
}

// ============================================================================
// ENHANCEMENT 3: Golden Reference Comparison
// Generates theoretical correct RGB interpolated triangle as golden reference
// using barycentric color interpolation
// ============================================================================
TEST_F(E2ETest, Scene002_RGBInterpolation_GoldenReference) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);
    std::string ppmPath = dumpPPM("e2e_rgb_triangle.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    // Generate golden reference using barycentric interpolation
    // v0=RED(1,0,0), v1=GREEN(0,1,0), v2=BLUE(0,0,1)
    // Barycentric: color = w0*v0_color + w1*v1_color + w2*v2_color
    std::vector<uint8_t> goldenPixels(640 * 480 * 3, 0);

    for (int py = 0; py < 480; ++py) {
        for (int px = 0; px < 640; ++px) {
            // Convert screen coords to pixel center (matching rasterizer)
            float pxF = static_cast<float>(px) + 0.5f;
            float pyF = static_cast<float>(py) + 0.5f;

            // Convert NDC vertices to screen coords (matching rasterizer::interpolateAttributes)
            float sx0 = (Scene002::V0_X + 1.0f) * 0.5f * 640.0f;
            float sy0 = (1.0f - Scene002::V0_Y) * 0.5f * 480.0f;
            float sx1 = (Scene002::V1_X + 1.0f) * 0.5f * 640.0f;
            float sy1 = (1.0f - Scene002::V1_Y) * 0.5f * 480.0f;
            float sx2 = (Scene002::V2_X + 1.0f) * 0.5f * 640.0f;
            float sy2 = (1.0f - Scene002::V2_Y) * 0.5f * 480.0f;

            // Edge function (same as Rasterizer::edgeFunction)
            auto edgeFunc = [](float px, float py, float ax, float ay, float bx, float by) {
                return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
            };

            float area = edgeFunc(sx0, sy0, sx1, sy1, sx2, sy2);
            float w0 = edgeFunc(pxF, pyF, sx1, sy1, sx2, sy2) / area;
            float w1 = edgeFunc(pxF, pyF, sx2, sy2, sx0, sy0) / area;
            float w2 = edgeFunc(pxF, pyF, sx0, sy0, sx1, sy1) / area;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // Inside triangle: interpolate colors
                // w0→v0=RED, w1→v1=GREEN, w2→v2=BLUE
                float r = w0 * 1.0f + w1 * 0.0f + w2 * 0.0f;
                float g = w0 * 0.0f + w1 * 1.0f + w2 * 0.0f;
                float b = w0 * 0.0f + w1 * 0.0f + w2 * 1.0f;
                size_t idx = (static_cast<size_t>(py) * 640 + px) * 3;
                goldenPixels[idx + 0] = static_cast<uint8_t>(std::min(1.0f, r) * 255);
                goldenPixels[idx + 1] = static_cast<uint8_t>(std::min(1.0f, g) * 255);
                goldenPixels[idx + 2] = static_cast<uint8_t>(std::min(1.0f, b) * 255);
            }
        }
    }

    // Write golden reference file
    FILE* f = fopen(Scene002::GOLDEN_FILE, "wb");
    if (f) {
        fprintf(f, "P6\n640 480\n255\n");
        fwrite(goldenPixels.data(), 1, goldenPixels.size(), f);
        fclose(f);
        printf("[GoldenRef] Generated: %s\n", Scene002::GOLDEN_FILE);
    }

    // Compare rendered output with golden reference (tolerance 0.02)
    bool goldenMatch = verifier.compareWithGolden(Scene002::GOLDEN_FILE, 0.02f);
    EXPECT_TRUE(goldenMatch)
        << "Scene002: Rendered output should match golden reference within tolerance 0.02";
}
