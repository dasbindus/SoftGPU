// ============================================================================
// scene_015_utah_teapot.cpp - E2E Scene 015: Utah Teapot
//
// Test: E2E-SCENE-015
// Target: Golden reference test for Utah Teapot loaded from OBJ
//
// Setup:
//   - Utah Teapot loaded from models/teapot.obj
//   - Per-face flat coloring for visual distinction
//   - Uses default vertex/fragment shader pipeline
//
// Verifications:
//   ✓ OBJ file loads successfully
//   ✓ Teapot geometry renders correctly
//   ✓ Golden reference comparison passes
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/OBJModelScene.hpp"

// ============================================================================
// E2E Tests
// ============================================================================

TEST_F(E2ETest, Scene015_UtahTeapot_LoadsSuccessfully) {
    // Create OBJ model scene with teapot
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/teapot.obj");

    ASSERT_TRUE(scene->isLoaded()) << "Teapot OBJ model should load successfully";
    EXPECT_GT(scene->getTriangleCount(), 0u) << "Should have triangles";
}

TEST_F(E2ETest, Scene015_UtahTeapot_GoldenReference) {
    // Create OBJ model scene with teapot
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/teapot.obj");
    ASSERT_TRUE(scene->isLoaded()) << "Teapot OBJ model should load successfully";

    // Build render command (includes view/projection matrices)
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Render
    m_pipeline->render(cmd);

    // Dump PPM for inspection
    std::string ppmPath = dumpPPM("scene015_utah_teapot.ppm");

    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene015_utah_teapot.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.05f);
    EXPECT_TRUE(match) << "Scene015: Utah teapot should match golden reference";
}

TEST_F(E2ETest, Scene015_UtahTeapot_NonBlackPixels) {
    // Create OBJ model scene with teapot
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/teapot.obj");
    ASSERT_TRUE(scene->isLoaded()) << "Teapot OBJ model should load successfully";

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Render
    m_pipeline->render(cmd);

    // Count non-black pixels
    int nonBlackCount = 0;
    const float* color = getColorBuffer();
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4; i += 4) {
        if (color[i] > 0.01f || color[i+1] > 0.01f || color[i+2] > 0.01f) {
            nonBlackCount++;
        }
    }
    EXPECT_GT(nonBlackCount, 3000) << "Should have > 3000 non-black pixels from teapot";
}

TEST_F(E2ETest, Scene015_UtahTeapot_NoCrash) {
    // Create OBJ model scene with teapot
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/teapot.obj");

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Should not crash
    EXPECT_NO_THROW(m_pipeline->render(cmd));
}
