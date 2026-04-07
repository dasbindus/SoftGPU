// ============================================================================
// scene_014_textured_triangle.cpp - E2E Scene 014: Textured Triangle (PNG)
//
// Test: E2E-SCENE-014
// Target: Golden reference test for Triangle-1Tri-Textured scene with PNG texture
//
// Setup:
//   - Single textured triangle using external PNG texture
//   - Uses Texture Sampling Shader (TEX instruction)
//   - Texture: tests/e2e/golden/texture1.png
//
// Verifications:
//   ✓ PNG texture loading works correctly
//   ✓ TEX instruction samples texture with UV coordinates
//   ✓ Golden reference comparison passes
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"
#include <pipeline/ShaderCore.hpp>

// ============================================================================
// Test: Triangle-1Tri-Textured with PNG texture renders correctly
// ============================================================================
TEST_F(E2ETest, Scene014_TexturedTriangle_GoldenReference) {
    // Register and get the scene
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri-Textured");
    ASSERT_NE(scene, nullptr) << "Triangle-1Tri-Textured scene should exist";

    // Load PNG texture and enable texture sampling shader
    auto& shaderCore = m_pipeline->getFragmentShader().getShaderCore();
    bool loaded = shaderCore.setTextureFromPNG(0, "tests/e2e/golden/texture1.png");
    ASSERT_TRUE(loaded) << "texture1.png should load successfully";

    m_pipeline->getFragmentShader().setShaderFunction(SoftGPU::ShaderCore::getTextureSamplingShader());

    // Build render command
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // Render
    m_pipeline->render(cmd);

    // Dump PPM
    std::string ppmPath = dumpPPM("scene014_textured_triangle.ppm");

    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene014_textured_triangle.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.02f);
    EXPECT_TRUE(match) << "Scene014: Textured triangle should match golden reference";
}

// ============================================================================
// Test: Verify texture sampling produces non-black pixels
// ============================================================================
TEST_F(E2ETest, Scene014_TexturedTriangle_NonBlackPixels) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri-Textured");
    ASSERT_NE(scene, nullptr);

    auto& shaderCore = m_pipeline->getFragmentShader().getShaderCore();
    shaderCore.setTextureFromPNG(0, "tests/e2e/golden/texture1.png");
    m_pipeline->getFragmentShader().setShaderFunction(SoftGPU::ShaderCore::getTextureSamplingShader());

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels manually
    int nonBlackCount = 0;
    const float* color = getColorBuffer();
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * 4; i += 4) {
        if (color[i] > 0.01f || color[i+1] > 0.01f || color[i+2] > 0.01f) {
            nonBlackCount++;
        }
    }
    EXPECT_GT(nonBlackCount, 30000) << "Should have > 30000 non-black pixels from texture sampling";
}
