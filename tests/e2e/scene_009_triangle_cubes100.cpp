// ============================================================================
// scene_009_triangle_cubes100.cpp - E2E Scene 009: Triangle-Cubes-100
//
// Test: E2E-SCENE-009
// Target: Golden reference test for Triangle-Cubes-100 scene
//
// This test renders the Triangle-Cubes-100 scene (10x10 grid of 100 cubes)
// and verifies rendering correctness through structural tests rather than
// exact pixel comparison (due to complexity of generating exact golden).
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"
#include <cmath>

// ============================================================================
// Tests for Triangle-Cubes-100
// ============================================================================
TEST_F(E2ETest, Scene009_TriangleCubes100_GoldenReference) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr) << "Triangle-Cubes-100 scene should exist";

    // Verify triangle count
    EXPECT_EQ(scene->getTriangleCount(), 1200u) << "Should have 1200 triangles (100 cubes × 12)";

    // Build render command and render
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Dump PPM
    std::string ppmPath = dumpPPM("scene009_triangle_cubes100.ppm");
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM should be generated";

    // Compare with golden reference (uses compiler-specific golden file)
    bool match = verifier.compareWithGolden(getGoldenPath("scene009_triangle_cubes100").c_str(), 0.05f);
    EXPECT_TRUE(match) << "Scene009: Triangle-Cubes-100 should match golden reference";
}

TEST_F(E2ETest, Scene009_TriangleCubes100_NonBlackPixelCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        if (color[i*4] > 0.1f || color[i*4+1] > 0.1f || color[i*4+2] > 0.1f) {
            count++;
        }
    }
    // With 100 cubes visible from above-diagonal, expect substantial coverage
    EXPECT_GT(count, 50000) << "Should have > 50000 non-black pixels from 100 cubes";
}

TEST_F(E2ETest, Scene009_TriangleCubes100_HasMultipleFaceColors) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Each cube face has different color, so we should see color variety
    const float* color = getColorBuffer();
    int colorfulCount = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i*4], g = color[i*4+1], b = color[i*4+2];
        float maxC = std::max({r, g, b});
        float minC = std::min({r, g, b});
        // Check for color variety - pixels that aren't grayscale
        if (maxC > 0.1f && (maxC - minC) > 0.15f) {
            colorfulCount++;
        }
    }
    EXPECT_GT(colorfulCount, 1000) << "Should have colorful pixels from different cube faces";
}

TEST_F(E2ETest, Scene009_TriangleCubes100_CentralRegionHasCubes) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Central region (320±100, 240±100) should have cubes
    PPMVerifier verifier(dumpPPM("scene009_triangle_cubes100.ppm"));
    ASSERT_TRUE(verifier.isLoaded());

    auto stats = verifier.analyzeRegion(220, 140, 420, 340);
    EXPECT_GT(stats.nonBlackPixelCount, 5000)
        << "Central region should have many cubes visible";
}

TEST_F(E2ETest, Scene009_TriangleCubes100_VertexCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // 1200 triangles × 3 vertices = 3600 vertices
    EXPECT_EQ(cmd.drawParams.vertexCount, 3600u)
        << "Should have 3600 vertices (1200 triangles × 3)";
}