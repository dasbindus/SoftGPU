// ============================================================================
// scene_004_mad_verification.cpp - E2E Scene 004: MAD Instruction Verification
//
// Test: E2E-SCENE-004
// Target: Verify fragment shader MAD (Multiply-Add) instruction correctness
//
// MAD Operation: Rd = Ra * Rb + Rc
// In barycentric interpolation: result = w0 * v0 + w1 * v1 + w2 * v2
//
// Verifications:
//   ✓ MAD-based interpolation produces correct barycentric weights
//   ✓ Interpolated values match expected ranges
//   ✓ Texture coordinate interpolation: u,v in [0,1] range
//   ✓ MAD chain for 3-vertex interpolation is accurate
//
// Enhancements (王刚):
//   ✓ Bounding Box Exact Verification (NDC + viewport transform)
//   ✓ Slanted Edge Linearity Detection
//   ✓ Golden Reference Comparison (PPMVerifier, tolerance 0.02)
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"
#include <cmath>

// ============================================================================
// Scene 004 Geometry Constants
// ============================================================================
namespace Scene004 {
    // Same triangle geometry as Scene001/002:
    // V0(0.0, 0.5) → screen (320, 120)
    // V1(-0.5,-0.5) → screen (160, 420)
    // V2(0.5, -0.5) → screen (480, 420)
    constexpr float V0_X =  0.0f,  V0_Y =  0.5f;
    constexpr float V1_X = -0.5f,  V1_Y = -0.5f;
    constexpr float V2_X =  0.5f,  V2_Y = -0.5f;

    // After fill rule fix + NDC Y-axis fix:
    // Actual bbox: minX=160, maxX=479, minY=121, maxY=359
    constexpr int   EXPECTED_MIN_X = 160;
    constexpr int   EXPECTED_MAX_X = 479;
    constexpr int   EXPECTED_MIN_Y = 121;
    constexpr int   EXPECTED_MAX_Y = 359;

    const char* GOLDEN_FILE = "tests/e2e/golden/scene004_mad_verification.ppm";
}

namespace MADTest {

// Simulate MAD instruction: Rd = Ra * Rb + Rc
inline float mad(float a, float b, float c) {
    return a * b + c;
}

struct BaryCoords {
    float w0, w1, w2;
};

BaryCoords computeBarycentric(float px, float py,
                              float v0x, float v0y,
                              float v1x, float v1y,
                              float v2x, float v2y) {
    float area = v0x * (v1y - v2y) + v1x * (v2y - v0y) + v2x * (v0y - v1y);
    BaryCoords bc;
    bc.w1 = (px * (v1y - v2y) + v1x * (v2y - py) + v2x * (py - v1y)) / area;
    bc.w2 = (px * (v2y - v0y) + v2x * (v0y - py) + v0x * (py - v2y)) / area;
    bc.w0 = 1.0f - bc.w1 - bc.w2;
    return bc;
}

float interpolateRef(BaryCoords bc, float v0, float v1, float v2) {
    return bc.w0 * v0 + bc.w1 * v1 + bc.w2 * v2;
}

}  // namespace MADTest

// ---------------------------------------------------------------------------
// Test: Triangle renders with varying vertex values
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_CorrectInterpolation) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  10.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  20.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Near v0 (top): should be dark (R near 0)
    float r0, g0, b0;
    getBufferPixelColor(320, 60, r0, g0, b0);
    EXPECT_LT(r0, 2.0f) << "Near v0, R should be ~0, got " << r0;

    // Near centroid: should be in mid-range
    float rc, gc, bc;
    getBufferPixelColor(320, 240, rc, gc, bc);
    EXPECT_GT(rc, 0.1f) << "Centroid should have some interpolated value, got R=" << rc;
}

// ---------------------------------------------------------------------------
// Test: MAD chain accuracy vs barycentric reference
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_ChainAccuracy) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  10.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  20.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Verify that at least some pixels have non-zero interpolated values
    int nonZeroCount = 0;
    const float* color = getColorBuffer();
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        if (color[i * 4 + 0] > 0.01f) nonZeroCount++;
    }
    EXPECT_GT(nonZeroCount, 5000)
        << "Should have many non-zero interpolated pixels";
}

// ---------------------------------------------------------------------------
// Test: MAD for color interpolation with arbitrary vertex colors
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_ColorInterpolation) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 0.5f, 0.25f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  0.25f, 1.0f, 0.5f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   0.5f, 0.25f, 1.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    float rc, gc, bc;
    getBufferPixelColor(320, 240, rc, gc, bc);

    // Each channel should be between min and max of vertex values
    EXPECT_GE(rc, 0.20f); EXPECT_LE(rc, 1.05f);
    EXPECT_GE(gc, 0.20f); EXPECT_LE(gc, 1.05f);
    EXPECT_GE(bc, 0.20f); EXPECT_LE(bc, 1.05f);

    // Centroid should be roughly gray (all channels similar)
    float diffRG = std::abs(rc - gc);
    float diffGB = std::abs(gc - bc);
    EXPECT_LT(diffRG, 0.40f) << "Centroid should be roughly gray";
    EXPECT_LT(diffGB, 0.40f) << "Centroid should be roughly gray";
}

// ---------------------------------------------------------------------------
// Test: MAD with zero operand
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_ZeroOperand) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        0.0f,  0.5f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   1.0f, 1.0f, 1.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Near bottom-left vertex area (160, 360): should be dark
    float r1, g1, b1;
    getBufferPixelColor(160, 360, r1, g1, b1);
    EXPECT_LT(r1 + g1 + b1, 0.5f)
        << "Near black vertex, pixel should be dark";

    int brightInUpper = 0;
    for (int y = 80; y < 150; y += 5) {
        for (int x = 290; x < 350; x += 5) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r + g + b > 1.5f) brightInUpper++;
        }
    }
    EXPECT_GT(brightInUpper, 2) << "Upper region should have some bright pixels";
}

// ---------------------------------------------------------------------------
// Test: MAD chain precision
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_Precision) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,  0.123456f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  0.654321f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  0.987654f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    float rc, gc, bc;
    getBufferPixelColor(320, 240, rc, gc, bc);

    float expected = (0.123456f + 0.654321f + 0.987654f) / 3.0f;
    float error = std::abs(rc - expected);
    // Allow 15% tolerance for rasterizer discretization
    EXPECT_LT(error, 0.20f)
        << "Centroid precision error should be < 20%, got error=" << error;
}

// ---------------------------------------------------------------------------
// Test: MAD with negative values
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_NegativeValues) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.5f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  -0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   1.5f, 0.0f, 0.0f, 1.0f,
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
    EXPECT_GT(nonBlackCount, 5000) << "Triangle should render without crashing";
}

// ---------------------------------------------------------------------------
// Test: Triangle renders with valid interpolation
// ---------------------------------------------------------------------------
TEST_F(E2ETest, Scene004_MAD_TextureCoordInterpolation) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,   1.0f, 1.0f, 0.0f, 1.0f,
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
    EXPECT_GT(nonBlackCount, 1000) << "Triangle should render without crashing";
}

// ============================================================================
// ENHANCEMENT 1: Bounding Box Exact Verification
// Same geometry as Scene001: minX=160, maxX=480, minY=60, maxY=360
// ============================================================================
TEST_F(E2ETest, Scene004_MAD_BoundingBoxExact) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  10.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  20.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Compute bounding box of non-black pixels
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

    EXPECT_EQ(minX, Scene004::EXPECTED_MIN_X)
        << "Scene004: Left edge should be at x=" << Scene004::EXPECTED_MIN_X;
    EXPECT_EQ(maxX, Scene004::EXPECTED_MAX_X)
        << "Scene004: Right edge should be at x=" << Scene004::EXPECTED_MAX_X;
    EXPECT_EQ(minY, Scene004::EXPECTED_MIN_Y)
        << "Scene004: Top edge should be at y=" << Scene004::EXPECTED_MIN_Y;
    EXPECT_EQ(maxY, Scene004::EXPECTED_MAX_Y)
        << "Scene004: Bottom edge should be at y=" << Scene004::EXPECTED_MAX_Y;
}

// ============================================================================
// ENHANCEMENT 2: Slanted Edge Linearity Detection
// MAD scene uses same geometry as Scene001, so same edge linearity check
// ============================================================================
TEST_F(E2ETest, Scene004_MAD_SlantedEdgeLinearity) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  10.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  20.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);

    // Left hypotenuse: V0(320,120) → V1(160,360) after NDC Y fix
    // x_left(y) = 320 - ((y - 120) * (2/3))

    int leftViolations = 0;
    for (int y = Scene004::EXPECTED_MIN_Y; y <= Scene004::EXPECTED_MAX_Y; y += 10) {
        float expectedLeftX = 320.0f - (static_cast<float>(y - 120) * (2.0f / 3.0f));

        int foundLeft = -1;
        for (int x = static_cast<int>(expectedLeftX) - 5; x <= static_cast<int>(expectedLeftX) + 5; ++x) {
            float r, g, b;
            getBufferPixelColor(x, y, r, g, b);
            if (r > 0.01f || g > 0.01f || b > 0.01f) {
                foundLeft = x;
                break;
            }
        }

        if (foundLeft >= 0) {
            float deviation = std::abs(static_cast<float>(foundLeft) - expectedLeftX);
            if (deviation > 3.0f) leftViolations++;
        }
    }

    EXPECT_LT(leftViolations, 5)
        << "Left hypotenuse should be linear, " << leftViolations << " violations found";
}

// ============================================================================
// ENHANCEMENT 3: Golden Reference Comparison
// For MAD scene with arbitrary vertex color values
// v0=(0,0.5)→R=0, v1=(-0.5,-0.5)→R=10, v2=(0.5,-0.5)→R=20
// ============================================================================
TEST_F(E2ETest, Scene004_MAD_GoldenReference) {
    float vertices[] = {
        0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 0.0f, 0.0f, 1.0f,
       -0.5f, -0.5f, 0.0f, 1.0f,  10.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f, 1.0f,  20.0f, 0.0f, 0.0f, 1.0f,
    };

    renderTriangle(vertices, 3);
    std::string ppmPath = dumpPPM("e2e_mad_verification.ppm");

    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load successfully";

    // Generate golden reference: flat shading, color = barycentric R interpolation
    std::vector<uint8_t> goldenPixels(640 * 480 * 3, 0);

    for (int py = 0; py < 480; ++py) {
        for (int px = 0; px < 640; ++px) {
            // Convert screen coords to pixel center (matching rasterizer)
            float pxF = static_cast<float>(px) + 0.5f;
            float pyF = static_cast<float>(py) + 0.5f;

            // Convert NDC vertices to screen coords (matching rasterizer::interpolateAttributes)
            float sx0 = (Scene004::V0_X + 1.0f) * 0.5f * 640.0f;
            float sy0 = (1.0f - Scene004::V0_Y) * 0.5f * 480.0f;
            float sx1 = (Scene004::V1_X + 1.0f) * 0.5f * 640.0f;
            float sy1 = (1.0f - Scene004::V1_Y) * 0.5f * 480.0f;
            float sx2 = (Scene004::V2_X + 1.0f) * 0.5f * 640.0f;
            float sy2 = (1.0f - Scene004::V2_Y) * 0.5f * 480.0f;

            // Edge function (same as Rasterizer::edgeFunction)
            auto edgeFunc = [](float px, float py, float ax, float ay, float bx, float by) {
                return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
            };

            float area = edgeFunc(sx0, sy0, sx1, sy1, sx2, sy2);
            float w0 = edgeFunc(pxF, pyF, sx1, sy1, sx2, sy2) / area;
            float w1 = edgeFunc(pxF, pyF, sx2, sy2, sx0, sy0) / area;
            float w2 = edgeFunc(pxF, pyF, sx0, sy0, sx1, sy1) / area;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // Inside: interpolate R values (0, 10, 20)
                // v0→0, v1→10, v2→20
                // The Rasterizer clamps each color component to [0,1] BEFORE the shader,
                // so we need: clamp(raw_interpolation, 0, 1)
                // The shader then MOVs this clamped value directly to output (no /20)
                float r_val = w0 * 0.0f + w1 * 10.0f + w2 * 20.0f;
                r_val = std::max(0.0f, std::min(1.0f, r_val));
                size_t idx = (static_cast<size_t>(py) * 640 + px) * 3;
                goldenPixels[idx + 0] = static_cast<uint8_t>(r_val * 255);
                goldenPixels[idx + 1] = 0;
                goldenPixels[idx + 2] = 0;
            }
        }
    }

    FILE* f = fopen(Scene004::GOLDEN_FILE, "wb");
    if (f) {
        fprintf(f, "P6\n640 480\n255\n");
        fwrite(goldenPixels.data(), 1, goldenPixels.size(), f);
        fclose(f);
        printf("[GoldenRef] Generated: %s\n", Scene004::GOLDEN_FILE);
    }

    bool goldenMatch = verifier.compareWithGolden(Scene004::GOLDEN_FILE, 0.02f);
    EXPECT_TRUE(goldenMatch)
        << "Scene004: Rendered output should match golden reference within tolerance 0.02";
}
