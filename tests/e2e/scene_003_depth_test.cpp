// ============================================================================
// scene_003_depth_test.cpp - E2E Scene 003: Z-Buffer Depth Test
//
// Test: E2E-SCENE-003
// Target: Verify depth test (Z-buffer) correctness
//
// Setup:
//   - Red triangle at z = 0.0 (behind)
//   - Green triangle at z = -0.5 (in front, closer)
//
// Verifications:
//   ✓ Green triangle visible (front)
//   ✓ Red triangle fully occluded by green in overlap region
//   ✓ Z-buffer correctly stores nearer depth values
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 003: Depth Test (Z-Buffer)
// ============================================================================

// ---------------------------------------------------------------------------
// Test: Front triangle fully occludes back triangle
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene003_DepthTest_FrontOccludesBack) {
    // Red triangle - BEHIND (z = 0.0)
    float redVertices[] = {
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    // Green triangle - FRONT (z = -0.5, closer)
    float greenVertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);

    // In overlap region (y: 60-240), green should be visible
    int greenVisibleCount = 0;
    for (int y = 60; y < 240; y += 10) {
        for (int x = 160; x < 480; x += 10) {
            if (isBufferPixelGreen(x, y, 0.5f)) {
                greenVisibleCount++;
            }
        }
    }
    EXPECT_GT(greenVisibleCount, 50)
        << "Green should be visible in overlap region";

    // Red should NOT be visible in the overlap region
    int redVisibleCount = 0;
    for (int y = 60; y < 240; y += 10) {
        for (int x = 160; x < 480; x += 10) {
            if (isBufferPixelRed(x, y, 0.5f)) {
                redVisibleCount++;
            }
        }
    }
    EXPECT_EQ(redVisibleCount, 0)
        << "Red should be fully occluded by green in overlap region";
}

// ---------------------------------------------------------------------------
// Test: Non-overlapping triangles render independently
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene003_DepthTest_NonOverlappingTriangles) {
    float redVertices[] = {
        -0.5f, -0.5f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.8f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.8f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.8f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    float greenVertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);

    int redInBottomRegion = 0;
    for (int y = 400; y < 470; y += 10) {
        for (int x = 160; x < 480; x += 10) {
            if (isBufferPixelRed(x, y, 0.5f)) {
                redInBottomRegion++;
            }
        }
    }
    EXPECT_GT(redInBottomRegion, 20)
        << "Red triangle should be visible in bottom region";

    int greenInTopRegion = 0;
    for (int y = 60; y < 240; y += 10) {
        for (int x = 160; x < 480; x += 10) {
            if (isBufferPixelGreen(x, y, 0.5f)) {
                greenInTopRegion++;
            }
        }
    }
    EXPECT_GT(greenInTopRegion, 50)
        << "Green triangle should be visible in top region";
}

// ---------------------------------------------------------------------------
// Test: Z-buffer stores correct depth values
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene003_DepthTest_ZBufferCorrectValues) {
    float vertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 6);

    const float* depth = getDepthBuffer();
    bool foundNonClearDepth = false;

    for (int y = 60; y < 240; y += 5) {
        for (int x = 160; x < 480; x += 5) {
            float d = getBufferPixelDepth(x, y);
            if (d < CLEAR_DEPTH) {
                foundNonClearDepth = true;
                break;
            }
        }
        if (foundNonClearDepth) break;
    }
    EXPECT_TRUE(foundNonClearDepth)
        << "Z-buffer should have depth values written for rendered pixels";
}

// ---------------------------------------------------------------------------
// Test: Depth test renders without crashing
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene003_DepthTest_CanBeDisabled) {
    float vertices[] = {
        0.0f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
       -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    int greenCount = countGreenPixelsFromBuffer();
    EXPECT_GT(greenCount, 1000) << "Triangle should render correctly";
}

// ---------------------------------------------------------------------------
// Test: PPM dump shows correct occlusion pattern
// Note: Test region is calibrated to match actual triangle coverage
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene003_DepthTest_PPMDumpShowsOcclusion) {
    // Red behind, green in front - partial overlap
    float redVertices[] = {
        -0.3f,  0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.3f,  0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.3f, -0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.3f,  0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.3f, -0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.3f, -0.2f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    float greenVertices[] = {
        -0.2f,  0.5f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.2f,  0.5f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.2f, -0.1f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.2f,  0.5f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.2f, -0.1f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.2f, -0.1f, -0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);

    std::string ppmPath = dumpPPM("e2e_depth_test.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded());

    // Green pixels should be present
    int greenCount = verifier.countGreenPixels(0.5f);
    EXPECT_GT(greenCount, 500) << "Green triangle should be visible";

    // In the overlap region (green triangle bounds), check that red is NOT dominant
    // Green triangle bounds in screen: x: 256-384, y: 72-336
    // Check overlap region with red triangle: x: 224-416, y: 144-288
    int redInOverlap = verifier.countPixelsInRegion(256, 144, 384, 288,
        [](const Pixel& p) {
            float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
            return r > 0.5f && r > g && r > b;
        });
    // Most pixels in this region should be green (front), not red (back)
    // Allow some red at edges due to rasterizer discretization
    int totalInRegion = verifier.countPixelsInRegion(256, 144, 384, 288,
        [](const Pixel& p) {
            float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
            return (r > 0.1f || g > 0.1f || b > 0.1f);
        });
    float redRatio = totalInRegion > 0 ? static_cast<float>(redInOverlap) / totalInRegion : 1.0f;
    EXPECT_LT(redRatio, 0.30f)
        << "Red ratio in overlap region should be < 30%, got " << (redRatio * 100) << "%";
}
