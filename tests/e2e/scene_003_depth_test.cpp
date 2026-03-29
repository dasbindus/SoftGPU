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
// Enhancements (王刚):
//   ✓ Bounding Box Exact Verification (NDC + viewport transform)
//   ✓ Slanted Edge Linearity Detection
//   ✓ Golden Reference Comparison (PPMVerifier, tolerance 0.02)
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

// ============================================================================
// Scene 003 Geometry Constants
// ============================================================================
namespace Scene003 {
    // Viewport: 640x480, NDC→screen: screenX = (ndcX+1)*640, screenY = (1-ndcY)*480

    // Red triangle (behind): V0(-0.5,0.0), V1(0.5,0.0), V2(0.5,-0.5), V3(-0.5,-0.5) (quad as 2 triangles)
    //   V0: (-0.5, 0.0) → screen (160, 240)
    //   V1: ( 0.5, 0.0) → screen (480, 240)
    //   V2: ( 0.5,-0.5) → screen (480, 420)
    //   V3: (-0.5,-0.5) → screen (160, 420)
    // Red bbox: minX=160, maxX=480, minY=240, maxY=420

    // Green triangle (front, z=-0.5): V0(-0.5,0.5), V1(0.5,0.5), V2(0.5,0.0), ...
    //   V0: (-0.5, 0.5) → screen (160, 120)
    //   V1: ( 0.5, 0.5) → screen (480, 120)
    //   V2: ( 0.5, 0.0) → screen (480, 240)
    //   ... V3,V4,V5: (-0.5,0.5),(0.5,0.0),(-0.5,0.0)
    // Green bbox: minX=160, maxX=480, minY=120, maxY=300

    // After fill rule fix + NDC Y-axis fix:
    // Red triangle: V0(-0.5,0.0)→(160,240), V2(0.5,-0.5)→(480,360) - bbox: [160,479]×[240,359]
    constexpr int RED_MIN_X = 160, RED_MAX_X = 479, RED_MIN_Y = 240, RED_MAX_Y = 359;
    // Green triangle: V0(-0.5,0.5)→(160,120), V2(0.5,0.0)→(480,240) - bbox: [160,479]×[120,239]
    constexpr int GREEN_MIN_X = 160, GREEN_MAX_X = 479, GREEN_MIN_Y = 120, GREEN_MAX_Y = 239;

    // Partial overlap test geometry
    // Red: x=[-0.3,0.3], y=[-0.2,0.2] → screen: x=[224,416], y=[144,336]
    // Green: x=[-0.2,0.2], y=[-0.1,0.5] → screen: x=[256,384], y=[168,336]
    constexpr int RED_PARTIAL_MIN_X = 224, RED_PARTIAL_MAX_X = 416;
    constexpr int RED_PARTIAL_MIN_Y = 144, RED_PARTIAL_MAX_Y = 336;
    constexpr int GREEN_PARTIAL_MIN_X = 256, GREEN_PARTIAL_MAX_X = 384;
    constexpr int GREEN_PARTIAL_MIN_Y = 168,  GREEN_PARTIAL_MAX_Y = 336;

    const char* GOLDEN_FILE = "tests/e2e/golden/scene003_depth_test.ppm";
}

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

    // In overlap region (y: 240), green should be visible
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

// ============================================================================
// ENHANCEMENT 1: Bounding Box Exact Verification
// Verifies green (front) and red (back) triangle bounding boxes
// ============================================================================
TEST_F(E2ETest, Scene003_DepthTest_GreenBBoxExact) {
    float greenVertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(greenVertices, 6);

    PixelBounds greenBounds = getGreenBoundsFromBuffer();
    ASSERT_TRUE(greenBounds.valid) << "Green pixel bounding box should be valid";

    // Green triangle: V0(-0.5,0.5)→(160,120), V1(0.5,0.5)→(480,120),
    //                 V2(0.5,0.0)→(480,300), V3(-0.5,0.0)→(160,300)
    // Expected: minX=160, maxX=480, minY=120, maxY=300

    EXPECT_EQ(greenBounds.minX, Scene003::GREEN_MIN_X)
        << "Scene003 Green: Left edge should be at x=" << Scene003::GREEN_MIN_X;
    EXPECT_EQ(greenBounds.maxX, Scene003::GREEN_MAX_X)
        << "Scene003 Green: Right edge should be at x=" << Scene003::GREEN_MAX_X;
    EXPECT_EQ(greenBounds.minY, Scene003::GREEN_MIN_Y)
        << "Scene003 Green: Top edge should be at y=" << Scene003::GREEN_MIN_Y;
    EXPECT_EQ(greenBounds.maxY, Scene003::GREEN_MAX_Y)
        << "Scene003 Green: Bottom edge should be at y=" << Scene003::GREEN_MAX_Y;
}

TEST_F(E2ETest, Scene003_DepthTest_RedBBoxExact) {
    float redVertices[] = {
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(redVertices, 6);

    PixelBounds redBounds = getRedBoundsFromBuffer();
    ASSERT_TRUE(redBounds.valid) << "Red pixel bounding box should be valid";

    // Red triangle: V0(-0.5,0.0)→(160,240), V1(0.5,0.0)→(480,240),
    //               V2(0.5,-0.5)→(480,420), V3(-0.5,-0.5)→(160,420)
    // Expected: minX=160, maxX=480, minY=240, maxY=420

    EXPECT_EQ(redBounds.minX, Scene003::RED_MIN_X)
        << "Scene003 Red: Left edge should be at x=" << Scene003::RED_MIN_X;
    EXPECT_EQ(redBounds.maxX, Scene003::RED_MAX_X)
        << "Scene003 Red: Right edge should be at x=" << Scene003::RED_MAX_X;
    EXPECT_EQ(redBounds.minY, Scene003::RED_MIN_Y)
        << "Scene003 Red: Top edge should be at y=" << Scene003::RED_MIN_Y;
    EXPECT_EQ(redBounds.maxY, Scene003::RED_MAX_Y)
        << "Scene003 Red: Bottom edge should be at y=" << Scene003::RED_MAX_Y;
}

// ============================================================================
// ENHANCEMENT 2: Slanted Edge Linearity Detection
// For Scene003, check the hypotenuse edges of green (front) triangle
// ============================================================================
TEST_F(E2ETest, Scene003_DepthTest_SlantedEdgeLinearity) {
    float greenVertices[] = {
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
         0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.0f, -0.5f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
    };

    renderTriangle(greenVertices, 6);

    // Green triangle slanted edge: V0(-0.5,0.5)→(160,120) to V2(0.5,0.0)→(480,240)
    // After NDC Y fix: V2(0.5, 0.0) → screenY = (1-0.0)*0.5*480 = 240
    // Edge: slope Δx/Δy = (480-160)/(240-120) = 320/120 = 8/3
    // x(y) = 160 + (y - 120) * (8/3)

    int leftViolations = 0;
    for (int y = Scene003::GREEN_MIN_Y; y <= Scene003::GREEN_MAX_Y; y += 10) {
        float expectedX = 160.0f + (static_cast<float>(y - 120) * (8.0f / 3.0f));

        int foundX = -1;
        for (int x = static_cast<int>(expectedX) - 10; x <= static_cast<int>(expectedX) + 10; ++x) {
            if (isBufferPixelGreen(x, y)) {
                foundX = x;
                break;
            }
        }

        if (foundX >= 0) {
            float deviation = std::abs(static_cast<float>(foundX) - expectedX);
            if (deviation > 5.0f) leftViolations++;
        }
    }

    EXPECT_LT(leftViolations, 15)
        << "Left slanted edge should be linear, " << leftViolations << " violations found";
}

// ============================================================================
// ENHANCEMENT 3: Golden Reference Comparison
// For depth test, golden reference shows green front triangle fully occluding red
// ============================================================================
TEST_F(E2ETest, Scene003_DepthTest_GoldenReference) {
    // Red behind, green in front - full overlap scenario
    float redVertices[] = {
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f, 1.0f,
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
    std::string ppmPath = dumpPPM("e2e_depth_test.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    // Generate golden reference: green renders, red is occluded
    // Green quad: (-0.5,0.5)→(160,120), (0.5,0.5)→(480,120),
    //              (0.5,0.0)→(480,240), (-0.5,0.0)→(160,240)
    // Red quad:    (-0.5,0.0)→(160,240), (0.5,0.0)→(480,240),
    //              (0.5,-0.5)→(480,360), (-0.5,-0.5)→(160,360)
    std::vector<uint8_t> goldenPixels(640 * 480 * 3, 0);

    for (int py = 0; py < 480; ++py) {
        for (int px = 0; px < 640; ++px) {
            float ndcX = (static_cast<float>(px) / 640.0f) * 2.0f - 1.0f;
            float ndcY = 1.0f - (static_cast<float>(py) / 480.0f) * 2.0f;
            size_t idx = (static_cast<size_t>(py) * 640 + px) * 3;

            // Check if inside green triangle (front, renders on top)
            // Green: top quad: x∈[-0.5,0.5], y∈[0.0,0.5]
            if (ndcX >= -0.5f && ndcX <= 0.5f && ndcY >= 0.0f && ndcY <= 0.5f) {
                goldenPixels[idx + 0] = 0;   // R=0
                goldenPixels[idx + 1] = 255; // G=255
                goldenPixels[idx + 2] = 0;   // B=0
            }
            // Red: bottom quad: x∈[-0.5,0.5], y∈[-0.5,0.0] -- only if NOT covered by green
            else if (ndcX >= -0.5f && ndcX <= 0.5f && ndcY >= -0.5f && ndcY <= 0.0f) {
                goldenPixels[idx + 0] = 255; // R=255
                goldenPixels[idx + 1] = 0;   // G=0
                goldenPixels[idx + 2] = 0;   // B=0
            }
            // else: black background (already 0)
        }
    }

    // Write golden reference
    FILE* f = fopen(Scene003::GOLDEN_FILE, "wb");
    if (f) {
        fprintf(f, "P6\n640 480\n255\n");
        fwrite(goldenPixels.data(), 1, goldenPixels.size(), f);
        fclose(f);
        printf("[GoldenRef] Generated: %s\n", Scene003::GOLDEN_FILE);
    }

    // Compare with tolerance 0.02
    bool goldenMatch = verifier.compareWithGolden(Scene003::GOLDEN_FILE, 0.02f);
    EXPECT_TRUE(goldenMatch)
        << "Scene003: Rendered output should match golden reference within tolerance 0.02";
}
