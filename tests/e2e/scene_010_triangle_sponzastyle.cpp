// ============================================================================
// scene_010_triangle_sponzastyle.cpp - E2E Scene 010: Triangle-SponzaStyle
//
// Test: E2E-SCENE-010
// Target: Golden reference test for Triangle-SponzaStyle scene
//
// This test renders the Triangle-SponzaStyle scene (box room with floor,
// ceiling, walls, and pillars) and verifies rendering correctness.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"
#include <cmath>

// ============================================================================
// Tests for Triangle-SponzaStyle
// ============================================================================
TEST_F(E2ETest, Scene010_TriangleSponzaStyle_RendersWithoutError) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr) << "Triangle-SponzaStyle scene should exist";

    // Build render command and render
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    std::string ppmPath = dumpPPM("scene010_triangle_sponzastyle.ppm");
    EXPECT_TRUE(PPMVerifier(ppmPath).isLoaded()) << "PPM should be generated";
}

TEST_F(E2ETest, Scene010_TriangleSponzaStyle_NonBlackPixelCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels (room surfaces should be visible)
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        if (color[i*4] > 0.1f || color[i*4+1] > 0.1f || color[i*4+2] > 0.1f) {
            count++;
        }
    }
    EXPECT_GT(count, 50000) << "Should have > 50000 non-black pixels (room surfaces visible)";
}

TEST_F(E2ETest, Scene010_TriangleSponzaStyle_FloorRegionHasColor) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Floor should be in lower portion of screen (warm brown color)
    PPMVerifier verifier(dumpPPM("scene010_triangle_sponzastyle.ppm"));
    ASSERT_TRUE(verifier.isLoaded());

    // Check lower portion where floor should be
    auto stats = verifier.analyzeRegion(100, 300, 540, 450);
    EXPECT_GT(stats.nonBlackPixelCount, 5000)
        << "Lower region (floor) should have visible pixels";

    // Floor color should be warm (reddish-brown)
    EXPECT_GT(stats.avgR, 0.3f) << "Floor should have reddish tint";
}

TEST_F(E2ETest, Scene010_TriangleSponzaStyle_WallRegionsHaveColor) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    PPMVerifier verifier(dumpPPM("scene010_triangle_sponzastyle.ppm"));
    ASSERT_TRUE(verifier.isLoaded());

    // Left wall region
    auto leftStats = verifier.analyzeRegion(50, 100, 150, 400);
    EXPECT_GT(leftStats.nonBlackPixelCount, 1000) << "Left wall should be visible";

    // Right wall region
    auto rightStats = verifier.analyzeRegion(490, 100, 590, 400);
    EXPECT_GT(rightStats.nonBlackPixelCount, 1000) << "Right wall should be visible";
}

TEST_F(E2ETest, Scene010_TriangleSponzaStyle_TriangleCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    // The scene has: floor, ceiling, 4 walls, 4 pillars, decorative cubes
    // Expected: > 50 triangles
    uint32_t triCount = scene->getTriangleCount();
    EXPECT_GE(triCount, 50u) << "Should have at least 50 triangles";
    EXPECT_LE(triCount, 200u) << "Should have at most 200 triangles (reasonable for this scene)";
}

TEST_F(E2ETest, Scene010_TriangleSponzaStyle_UpperRegionHasCeiling) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    PPMVerifier verifier(dumpPPM("scene010_triangle_sponzastyle.ppm"));
    ASSERT_TRUE(verifier.isLoaded());

    // Upper portion where ceiling should be
    auto stats = verifier.analyzeRegion(100, 50, 540, 150);
    EXPECT_GT(stats.nonBlackPixelCount, 1000)
        << "Upper region (ceiling) should have visible pixels";
}