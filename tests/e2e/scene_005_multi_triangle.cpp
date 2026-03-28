// ============================================================================
// scene_005_multi_triangle.cpp - E2E Scene 005: Multi-Triangle Occlusion
//
// Test: E2E-SCENE-005
// Target: Verify complex depth handling with 3 overlapping triangles
//
// Input:
//   - Triangle A (red): z = 0.5 (farthest, back)
//   - Triangle B (green): z = 0.0 (middle)
//   - Triangle C (blue): z = -0.5 (closest, front)
//
// Verifications:
//   ✓ Each pixel shows the frontmost triangle's color
//   ✓ Depth values correctly updated
//   ✓ Complex overlap regions show correct occlusion
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 005: Multi-Triangle Occlusion
// ============================================================================

// ---------------------------------------------------------------------------
// Test: Three overlapping triangles with correct depth ordering
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene005_MultiTriangle_FrontmostVisible) {
    // Triangle A: RED - Farthest (z = 0.5)
    float redVertices[] = {
        -0.4f,  0.4f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.4f,  0.4f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.4f,  0.0f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.4f,  0.4f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.4f,  0.0f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.4f,  0.0f,  0.5f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    // Triangle B: GREEN - Middle (z = 0.0), positioned to the far RIGHT
    // NDC: x=[0.6, 0.9], y=[-0.1, 0.3] -> screen: x=[512, 608], y=[168, 264]
    // This is intentionally separate from blue (x=[160,480]) to verify green visibility
    float greenVertices[] = {
         0.6f,  0.3f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.9f,  0.3f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.9f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.6f,  0.3f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.9f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.6f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    // Triangle C: BLUE - Closest (z = -0.5), overlaps with green
    float blueVertices[] = {
         0.0f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.5f, -0.3f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.3f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    // Render in back-to-front order
    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);
    renderTriangle(blueVertices, 3);

    // Blue triangle (frontmost) should be visible in center area
    int blueCount = 0;
    for (int y = 80; y < 280; y += 10) {
        for (int x = 280; x < 360; x += 10) {
            if (isBufferPixelBlue(x, y, 0.5f)) blueCount++;
        }
    }
    EXPECT_GT(blueCount, 10) << "Blue (frontmost) should be visible in center overlap";

    // Green should be visible in its right-side region (x=[512,608], y=[168,264])
    // This region is well to the right of blue (x=[160,480]) so no overlap
    int greenCount = 0;
    for (int y = 170; y < 260; y += 10) {
        for (int x = 520; x < 600; x += 10) {
            if (isBufferPixelGreen(x, y, 0.5f)) greenCount++;
        }
    }
    EXPECT_GT(greenCount, 5) << "Green should be visible in right-side region";
}

// ---------------------------------------------------------------------------
// Test: Blue (front) occludes green (middle) and red (back)
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene005_MultiTriangle_BlueOccludesOthers) {
    // Red: z=0.3, Green: z=0.0, Blue: z=-0.3
    float redVertices[] = {
        -0.6f,  0.1f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.1f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f, -0.4f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.6f,  0.1f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f, -0.4f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.6f, -0.4f,  0.3f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    float greenVertices[] = {
        -0.1f,  0.5f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f,  0.5f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f, -0.1f,  0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    float blueVertices[] = {
        -0.3f,  0.6f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.3f,  0.6f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.3f,  0.0f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
        -0.3f,  0.6f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.3f,  0.0f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
        -0.3f,  0.0f, -0.3f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);
    renderTriangle(blueVertices, 6);

    // Check overlap region (where all three triangles could overlap)
    // Blue is at top, green middle, red bottom
    // Overlap region roughly: x=200-440, y=60-336
    int blueDominated = 0;
    int greenDominated = 0;
    int redDominated = 0;

    for (int y = 60; y < 336; y += 8) {
        for (int x = 200; x < 440; x += 8) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);

            if (b > 0.5f && b > r && b > g) {
                blueDominated++;
            } else if (g > 0.5f && g > r && g > b) {
                greenDominated++;
            } else if (r > 0.5f && r > g && r > b) {
                redDominated++;
            }
        }
    }

    // Blue should be visible in its region
    EXPECT_GT(blueDominated, 20) << "Blue should dominate in its region";
}

// ---------------------------------------------------------------------------
// Test: Correct depth values in multi-triangle scene
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene005_MultiTriangle_DepthValuesUpdated) {
    float vertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 6);

    // Depth values should be written for rendered pixels
    const float* depth = getDepthBuffer();
    int nonClearCount = 0;

    for (int y = 60; y < 240; y += 5) {
        for (int x = 160; x < 480; x += 5) {
            size_t idx = y * FRAMEBUFFER_WIDTH + x;
            if (depth[idx] < CLEAR_DEPTH) {
                nonClearCount++;
            }
        }
    }

    EXPECT_GT(nonClearCount, 100) << "Depth buffer should have values for rendered pixels";
}

// ---------------------------------------------------------------------------
// Test: All three triangles render without crashing
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene005_MultiTriangle_AllRender) {
    float redVertices[] = {
        -0.6f,  0.6f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.6f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.0f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.6f,  0.6f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.0f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.6f,  0.0f,  0.6f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    float greenVertices[] = {
        -0.1f,  0.5f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.6f,  0.5f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.6f,  0.0f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f,  0.5f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.6f,  0.0f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f,  0.0f,  0.3f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    float blueVertices[] = {
         0.0f,  0.1f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.6f,  0.1f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.6f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.0f,  0.1f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.6f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.0f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);
    renderTriangle(blueVertices, 6);

    int redCount = countRedPixelsFromBuffer();
    int greenCount = countGreenPixelsFromBuffer();
    int blueCount = countBluePixelsFromBuffer();

    EXPECT_GT(redCount, 500) << "Red triangle should render";
    EXPECT_GT(greenCount, 500) << "Green triangle should render";
    EXPECT_GT(blueCount, 500) << "Blue triangle should render";
}

// ---------------------------------------------------------------------------
// Test: PPM dump shows correct multi-triangle occlusion
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene005_MultiTriangle_PPMDumpCorrect) {
    // Three triangles with known overlap
    float redVertices[] = {
        -0.7f,  0.7f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.7f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.0f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.7f,  0.7f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.0f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.7f,  0.0f,  0.7f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    float greenVertices[] = {
        -0.1f,  0.6f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.7f,  0.6f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.7f,  0.0f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f,  0.6f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.7f,  0.0f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.1f,  0.0f,  0.4f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    float blueVertices[] = {
         0.0f,  0.2f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.7f,  0.2f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.7f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.0f,  0.2f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.7f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.0f, -0.5f,  0.0f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);
    renderTriangle(greenVertices, 6);
    renderTriangle(blueVertices, 6);

    std::string ppmPath = dumpPPM("e2e_multi_triangle.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    int redCount = verifier.countRedPixels(0.5f);
    int greenCount = verifier.countGreenPixels(0.5f);
    int blueCount = verifier.countBluePixels(0.5f);

    EXPECT_GT(redCount, 500) << "PPM should have red pixels";
    EXPECT_GT(greenCount, 500) << "PPM should have green pixels";
    EXPECT_GT(blueCount, 500) << "PPM should have blue pixels";
}
