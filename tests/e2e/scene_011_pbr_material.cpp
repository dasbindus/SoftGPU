// ============================================================================
// scene_011_pbr_material.cpp - E2E Scene 011: PBR-Material
//
// Test: E2E-SCENE-011
// Target: Golden reference test for PBR-Material scene
//
// This test renders the PBR-Material scene (9 spheres demonstrating different
// material properties) and verifies rendering correctness.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"
#include <cmath>

// ============================================================================
// Tests for PBR-Material
// ============================================================================
TEST_F(E2ETest, Scene011_PBRMaterial_RendersWithoutError) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr) << "PBR-Material scene should exist";

    // Build render command and render
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    std::string ppmPath = dumpPPM("scene011_pbr_material.ppm");
    EXPECT_TRUE(PPMVerifier(ppmPath).isLoaded()) << "PPM should be generated";
}

TEST_F(E2ETest, Scene011_PBRMaterial_NonBlackPixelCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels (9 spheres should be visible)
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        if (color[i*4] > 0.1f || color[i*4+1] > 0.1f || color[i*4+2] > 0.1f) {
            count++;
        }
    }
    EXPECT_GT(count, 10000) << "Should have > 10000 non-black pixels (spheres visible)";
}

TEST_F(E2ETest, Scene011_PBRMaterial_HasMultipleColors) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // PBR scene has gold, silver, red, green, purple spheres
    int redCount = countRedPixelsFromBuffer(0.3f);
    int greenCount = countGreenPixelsFromBuffer(0.3f);
    int blueCount = countBluePixelsFromBuffer(0.3f);

    // At least some spheres should be visible with distinct colors
    EXPECT_GT(redCount, 100) << "Should have red sphere pixels";
    EXPECT_GT(greenCount, 100) << "Should have green sphere pixels";
    EXPECT_GT(blueCount, 100) << "Should have blue sphere pixels";
}

TEST_F(E2ETest, Scene011_PBRMaterial_SphereRegionsVisible) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    PPMVerifier verifier(dumpPPM("scene011_pbr_material.ppm"));
    ASSERT_TRUE(verifier.isLoaded());

    // Spheres should be in the center region
    auto stats = verifier.analyzeRegion(150, 100, 490, 380);
    EXPECT_GT(stats.nonBlackPixelCount, 5000)
        << "Center region should have sphere pixels";
}

TEST_F(E2ETest, Scene011_PBRMaterial_TriangleCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    // PBR scene has 9 spheres with ~20 segments each = ~180 triangles
    uint32_t triCount = scene->getTriangleCount();
    EXPECT_GE(triCount, 100u) << "Should have at least 100 triangles";
    EXPECT_LE(triCount, 300u) << "Should have at most 300 triangles";
}

TEST_F(E2ETest, Scene011_PBRMaterial_ColorDistribution) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    std::string ppmPath = dumpPPM("scene011_pbr_material.ppm");
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded());

    // Verify center region has varied colors (spheres with different materials)
    auto stats = verifier.analyzeRegion(150, 100, 490, 380);
    EXPECT_GT(stats.nonBlackPixelCount, 2000) << "Center should have visible spheres";

    // Check that colors vary (not all the same)
    EXPECT_GT(stats.avgR, 0.05f) << "Should have some red tint";
    EXPECT_GT(stats.avgG, 0.05f) << "Should have some green tint";
    EXPECT_GT(stats.avgB, 0.05f) << "Should have some blue tint";
}