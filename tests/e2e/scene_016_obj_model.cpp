// ============================================================================
// scene_016_obj_model.cpp - E2E Scene 016: OBJ Model Loading
//
// Test: E2E-SCENE-016
// Target: Golden reference test for OBJ model loading and rendering
//
// Setup:
//   - Loads OBJ model from file (tests/e2e/models/cube.obj)
//   - Uses default vertex/fragment shader pipeline
//   - Per-face colors for visual distinction
//
// Verifications:
//   ✓ OBJ file loads successfully
//   ✓ Model renders with visible geometry
//   ✓ Golden reference comparison passes
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/OBJModelScene.hpp"

// ============================================================================
// E2E Tests
// ============================================================================

TEST_F(E2ETest, Scene016_OBJModel_LoadsSuccessfully) {
    // Create OBJ model scene with cube
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/cube.obj");

    ASSERT_TRUE(scene->isLoaded()) << "OBJ model should load successfully";
    EXPECT_GT(scene->getTriangleCount(), 0u) << "Should have triangles";
}

TEST_F(E2ETest, Scene016_OBJModel_GoldenReference) {
    // Create OBJ model scene with cube
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/cube.obj");
    ASSERT_TRUE(scene->isLoaded()) << "OBJ model should load successfully";

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Render
    m_pipeline->render(cmd);

    // Dump PPM for inspection
    std::string ppmPath = dumpPPM("scene016_obj_model.ppm");

    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene016_obj_model.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.05f);
    EXPECT_TRUE(match) << "Scene016: OBJ model should match golden reference";
}

TEST_F(E2ETest, Scene016_OBJModel_NonBlackPixels) {
    // Create OBJ model scene with cube
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/cube.obj");
    ASSERT_TRUE(scene->isLoaded()) << "OBJ model should load successfully";

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
    EXPECT_GT(nonBlackCount, 1000) << "Should have > 1000 non-black pixels from cube";
}

TEST_F(E2ETest, Scene016_OBJModel_NoCrash) {
    // Create OBJ model scene with cube
    auto scene = std::make_shared<SoftGPU::OBJModelScene>("tests/e2e/models/cube.obj");

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Should not crash
    EXPECT_NO_THROW(m_pipeline->render(cmd));
}
