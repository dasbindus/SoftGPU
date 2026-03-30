// ============================================================================
// scene_007_triangle_1tri.cpp - E2E Scene 007: Triangle-1Tri
//
// Test: E2E-SCENE-007
// Target: Golden reference test for Triangle-1Tri scene
//
// This test renders the Triangle-1Tri scene (single green triangle)
// and compares against the golden reference.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"

// ============================================================================
// Test: Triangle-1Tri renders with correct geometry
// ============================================================================
TEST_F(E2ETest, Scene007_Triangle1Tri_GoldenReference) {
    // Register and get the scene
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri");
    ASSERT_NE(scene, nullptr) << "Triangle-1Tri scene should exist";

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Render
    m_pipeline->render(cmd);

    // Dump PPM
    std::string ppmPath = dumpPPM("scene007_triangle_1tri.ppm");

    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene007_triangle_1tri.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.02f);
    EXPECT_TRUE(match) << "Scene007: Triangle-1Tri should match golden reference";
}

TEST_F(E2ETest, Scene007_Triangle1Tri_GreenPixelCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    int greenCount = countGreenPixelsFromBuffer();
    EXPECT_GT(greenCount, 20000) << "Should have > 20000 green pixels";
}

TEST_F(E2ETest, Scene007_Triangle1Tri_BoundingBox) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    PixelBounds bounds = getGreenBoundsFromBuffer();
    ASSERT_TRUE(bounds.valid);

    // Triangle should be centered in upper portion of screen
    int centerX = (bounds.minX + bounds.maxX) / 2;
    EXPECT_NEAR(centerX, 320, 80) << "Triangle center X should be near 320";
}