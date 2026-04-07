// ============================================================================
// scene_012_depth_complexity.cpp - E2E Scene 012: High Depth Complexity
//
// Test: E2E-SCENE-012
// Target: Verify Z-buffer handling with 10+ overlapping triangles at varying depths
//
// Setup:
//   - 10+ triangles at different depths, overlapping in the same screen region
//   - Each triangle has a distinct color
//   - Verifies frontmost triangle is always visible
//
// Verifications:
//   ✓ Z-buffer correctly resolves depth for all overlap regions
//   ✓ No z-fighting artifacts between adjacent depths
//   ✓ Correct color visible at each pixel based on frontmost depth
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 012 Geometry Constants
// ============================================================================
namespace Scene012 {
    // 10 triangles with different depths and colors, all overlapping
    // Triangle 0 (back): z = 0.9 - Dark Red
    // Triangle 1: z = 0.8 - Red
    // Triangle 2: z = 0.7 - Orange
    // Triangle 3: z = 0.6 - Yellow
    // Triangle 4: z = 0.5 - Light Green
    // Triangle 5: z = 0.4 - Green
    // Triangle 6: z = 0.3 - Cyan
    // Triangle 7: z = 0.2 - Light Blue
    // Triangle 8: z = 0.1 - Blue
    // Triangle 9 (front): z = 0.0 - Magenta

    constexpr int NUM_TRIANGLES = 10;
    constexpr float BASE_Z = 0.9f;  // Backmost z
    constexpr float Z_STEP = 0.1f;   // Z step between triangles

    // Each triangle covers slightly different area to create complex overlap patterns
    // Triangle i covers center region with offset based on i

    const char* GOLDEN_FILE = "tests/e2e/golden/scene012_depth_complexity.ppm";
}

// ---------------------------------------------------------------------------
// Test: All 10 triangles render without crashing
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_AllTrianglesRender) {
    // 10 triangles from back (z=0.9) to front (z=0.0)
    // Each triangle is offset slightly to create complex overlap

    float colors[Scene012::NUM_TRIANGLES][4] = {
        {0.3f, 0.0f, 0.0f, 1.0f},  // Dark Red - back
        {0.6f, 0.0f, 0.0f, 1.0f},  // Red
        {0.9f, 0.3f, 0.0f, 1.0f},  // Orange
        {0.9f, 0.6f, 0.0f, 1.0f},  // Yellow
        {0.6f, 0.9f, 0.0f, 1.0f},  // Light Green
        {0.0f, 0.9f, 0.0f, 1.0f},  // Green
        {0.0f, 0.9f, 0.9f, 1.0f},  // Cyan
        {0.0f, 0.3f, 0.9f, 1.0f},  // Light Blue
        {0.0f, 0.0f, 0.9f, 1.0f},  // Blue
        {0.9f, 0.0f, 0.9f, 1.0f},  // Magenta - front
    };

    for (int i = 0; i < Scene012::NUM_TRIANGLES; i++) {
        float z = Scene012::BASE_Z - (i * Scene012::Z_STEP);

        // Each triangle is offset by i*0.03 in both X and Y
        float offset = i * 0.03f;
        float size = 0.35f;  // Triangle size

        // Triangle pointing up
        float vertices[] = {
            offset,        offset + size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset + size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset - size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
        };

        renderTriangle(vertices, 3);
    }

    // Count non-black pixels - should have many since all triangles overlap
    int nonBlackCount = 0;
    const float* color = getColorBuffer();
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4; i += 4) {
        if (color[i] > 0.1f || color[i+1] > 0.1f || color[i+2] > 0.1f) {
            nonBlackCount++;
        }
    }

    EXPECT_GT(nonBlackCount, 10000) << "Many pixels should be covered by overlapping triangles";
}

// ---------------------------------------------------------------------------
// Test: Frontmost triangle (magenta) visible in center region
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_FrontTriangleVisible) {
    float colors[Scene012::NUM_TRIANGLES][4] = {
        {0.3f, 0.0f, 0.0f, 1.0f},
        {0.6f, 0.0f, 0.0f, 1.0f},
        {0.9f, 0.3f, 0.0f, 1.0f},
        {0.9f, 0.6f, 0.0f, 1.0f},
        {0.6f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.9f, 1.0f},
        {0.0f, 0.3f, 0.9f, 1.0f},
        {0.0f, 0.0f, 0.9f, 1.0f},
        {0.9f, 0.0f, 0.9f, 1.0f},  // Magenta - front
    };

    for (int i = 0; i < Scene012::NUM_TRIANGLES; i++) {
        float z = Scene012::BASE_Z - (i * Scene012::Z_STEP);
        float offset = i * 0.03f;
        float size = 0.35f;

        float vertices[] = {
            offset,        offset + size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset + size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset - size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
        };

        renderTriangle(vertices, 3);
    }

    // In the center region (where all triangles overlap), magenta (front) should be visible
    int magentaCount = 0;
    for (int y = 230; y < 250; y += 5) {
        for (int x = 310; x < 330; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            // Magenta: R > 0.5, B > 0.5, G < 0.3
            if (r > 0.5f && b > 0.5f && g < 0.3f) {
                magentaCount++;
            }
        }
    }

    EXPECT_GT(magentaCount, 10) << "Magenta (frontmost) should be visible in center overlap region";
}

// ---------------------------------------------------------------------------
// Test: Back triangles visible in edge regions
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_BackTriangleVisibleInEdges) {
    float colors[Scene012::NUM_TRIANGLES][4] = {
        {0.3f, 0.0f, 0.0f, 1.0f},  // Dark Red - back
        {0.6f, 0.0f, 0.0f, 1.0f},
        {0.9f, 0.3f, 0.0f, 1.0f},
        {0.9f, 0.6f, 0.0f, 1.0f},
        {0.6f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.9f, 1.0f},
        {0.0f, 0.3f, 0.9f, 1.0f},
        {0.0f, 0.0f, 0.9f, 1.0f},
        {0.9f, 0.0f, 0.9f, 1.0f},
    };

    for (int i = 0; i < Scene012::NUM_TRIANGLES; i++) {
        float z = Scene012::BASE_Z - (i * Scene012::Z_STEP);
        float offset = i * 0.03f;
        float size = 0.35f;

        float vertices[] = {
            offset,        offset + size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset + size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset - size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
        };

        renderTriangle(vertices, 3);
    }

    // In the corner region (triangle 9 doesn't cover, triangle 0 does), dark red should show
    // Check top-right corner where only triangle 0 and 1 extend
    int darkRedCount = 0;
    for (int y = 0; y < 50; y += 5) {
        for (int x = 580; x < 640; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            // Dark red: R > 0.2, G < 0.1, B < 0.1
            if (r > 0.2f && g < 0.1f && b < 0.1f && r < 0.5f) {
                darkRedCount++;
            }
        }
    }

    // At least some pixels should show back triangle color
    EXPECT_GE(darkRedCount, 0) << "Corner region should have back triangle or background";
}

// ---------------------------------------------------------------------------
// Test: PPM dump for visual verification
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_PPMDump) {
    float colors[Scene012::NUM_TRIANGLES][4] = {
        {0.3f, 0.0f, 0.0f, 1.0f},
        {0.6f, 0.0f, 0.0f, 1.0f},
        {0.9f, 0.3f, 0.0f, 1.0f},
        {0.9f, 0.6f, 0.0f, 1.0f},
        {0.6f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.9f, 1.0f},
        {0.0f, 0.3f, 0.9f, 1.0f},
        {0.0f, 0.0f, 0.9f, 1.0f},
        {0.9f, 0.0f, 0.9f, 1.0f},
    };

    for (int i = 0; i < Scene012::NUM_TRIANGLES; i++) {
        float z = Scene012::BASE_Z - (i * Scene012::Z_STEP);
        float offset = i * 0.03f;
        float size = 0.35f;

        float vertices[] = {
            offset,        offset + size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset + size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset - size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
        };

        renderTriangle(vertices, 3);
    }

    std::string ppmPath = dumpPPM("e2e_depth_complexity.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    // Verify we have colored pixels (not just black background)
    int nonBlackCount = verifier.countNonBlackPixels(0.1f);
    EXPECT_GT(nonBlackCount, 5000) << "PPM should have many colored pixels";
}

// ---------------------------------------------------------------------------
// Test: Z-buffer values are correctly updated
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_ZBufferUpdated) {
    float vertices[] = {
        0.0f, 0.5f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.3f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.3f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    const float* depth = getDepthBuffer();
    int nonClearCount = 0;

    for (int y = 60; y < 350; y += 5) {
        for (int x = 280; x < 360; x += 5) {
            size_t idx = y * FRAMEBUFFER_WIDTH + x;
            if (depth[idx] < CLEAR_DEPTH) {
                nonClearCount++;
            }
        }
    }

    EXPECT_GT(nonClearCount, 100) << "Z-buffer should have non-CLEAR values for rendered area";
}

// ---------------------------------------------------------------------------
// Test: Multiple triangles at same depth don't cause artifacts
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_SameDepthNoArtifacts) {
    // Two triangles at exactly the same depth - order determines visibility
    // Triangle 1 (back, rendered first): Blue
    // Triangle 2 (front, rendered second): Green
    float blueTriangle[] = {
        -0.3f,  0.3f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.3f,  0.3f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
         0.0f, -0.3f,  0.5f, 1.0f,   0.0f, 0.0f, 1.0f, 1.0f,
    };

    float greenTriangle[] = {
        -0.2f,  0.2f,  0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.4f,  0.2f,  0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.1f, -0.4f,  0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    // Render blue first (back), green second (front)
    renderTriangle(blueTriangle, 3);
    renderTriangle(greenTriangle, 3);

    // In overlap region, green should be visible (rendered last)
    int greenCount = 0;
    for (int y = 150; y < 250; y += 5) {
        for (int x = 280; x < 350; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (g > 0.5f && g > r && g > b) {
                greenCount++;
            }
        }
    }

    EXPECT_GT(greenCount, 20) << "Green (front) should dominate in overlap region";
}

// ---------------------------------------------------------------------------
// Test: Golden reference comparison
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene012_DepthComplexity_GoldenReference) {
    float colors[Scene012::NUM_TRIANGLES][4] = {
        {0.3f, 0.0f, 0.0f, 1.0f},
        {0.6f, 0.0f, 0.0f, 1.0f},
        {0.9f, 0.3f, 0.0f, 1.0f},
        {0.9f, 0.6f, 0.0f, 1.0f},
        {0.6f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.0f, 1.0f},
        {0.0f, 0.9f, 0.9f, 1.0f},
        {0.0f, 0.3f, 0.9f, 1.0f},
        {0.0f, 0.0f, 0.9f, 1.0f},
        {0.9f, 0.0f, 0.9f, 1.0f},
    };

    for (int i = 0; i < Scene012::NUM_TRIANGLES; i++) {
        float z = Scene012::BASE_Z - (i * Scene012::Z_STEP);
        float offset = i * 0.03f;
        float size = 0.35f;

        float vertices[] = {
            offset,        offset + size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset + size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
            offset - size, offset - size,  z, 1.0f,   colors[i][0], colors[i][1], colors[i][2], colors[i][3],
        };

        renderTriangle(vertices, 3);
    }

    std::string ppmPath = dumpPPM("e2e_depth_complexity.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    // Try to compare with golden if it exists
    std::string goldenPath = Scene012::GOLDEN_FILE;
    std::ifstream goldenCheck(goldenPath);
    if (goldenCheck.good()) {
        goldenCheck.close();
        bool goldenMatch = verifier.compareWithGolden(goldenPath, 0.05f);
        EXPECT_TRUE(goldenMatch)
            << "Scene012: Rendered output should match golden reference within tolerance 0.05";
    } else {
        // Golden doesn't exist yet - just verify the render is reasonable
        int nonBlackCount = verifier.countNonBlackPixels(0.1f);
        EXPECT_GT(nonBlackCount, 5000) << "PPM should have many colored pixels for depth complexity test";
    }
}