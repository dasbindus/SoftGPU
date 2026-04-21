// ============================================================================
// scene_017_triangle_strip.cpp - E2E Scene 017: Triangle Strip
//
// Test: E2E-SCENE-017
// Target: Golden reference test for Triangle Strip rendering
//
// This test renders using TRIANGLE_STRIP mode and compares against golden reference.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::mat4;
using glm::perspective;
using glm::lookAt;
using glm::radians;

// ============================================================================
// Test: Triangle Strip renders correctly
// ============================================================================
TEST_F(E2ETest, Scene017_TriangleStrip_GoldenReference) {
    // Create a quad using triangle strip (4 vertices -> 2 triangles)
    // Vertices form a 2D quad in the center of the screen
    std::vector<float> vertices = {
        // x, y, z, w, r, g, b, a (8 floats per vertex)
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // bottom-left (red)
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // bottom-right (green)
        -0.5f,  0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  // top-left (blue)
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,  // top-right (yellow)
    };

    // Build render command with triangle strip
    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.primitiveType = PrimitiveType::TRIANGLE_STRIP;
    cmd.drawParams.indexed = false;

    // Use perspective projection
    float aspect = 640.0f / 480.0f;
    mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
    mat4 view = lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mat4 model = mat4(1.0f);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cmd.projectionMatrix[i * 4 + j] = proj[i][j];
            cmd.viewMatrix[i * 4 + j] = view[i][j];
            cmd.modelMatrix[i * 4 + j] = model[i][j];
        }
    }

    // Render
    m_pipeline->render(cmd);

    // Dump PPM
    std::string ppmPath = dumpPPM("scene017_triangle_strip.ppm");

#if defined(__APPLE__)
    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene017_triangle_strip.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.02f);
    EXPECT_TRUE(match) << "Scene017: Triangle Strip should match golden reference";
#else
    // DISABLED on GCC - cross-platform floating point precision differences
    GTEST_SKIP() << "Golden reference test disabled on GCC due to cross-platform precision differences";
#endif
}

TEST_F(E2ETest, Scene017_TriangleStrip_NonBlackPixels) {
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.primitiveType = PrimitiveType::TRIANGLE_STRIP;
    cmd.drawParams.indexed = false;

    float aspect = 640.0f / 480.0f;
    mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
    mat4 view = lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mat4 model = mat4(1.0f);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cmd.projectionMatrix[i * 4 + j] = proj[i][j];
            cmd.viewMatrix[i * 4 + j] = view[i][j];
            cmd.modelMatrix[i * 4 + j] = model[i][j];
        }
    }

    m_pipeline->render(cmd);

    // Quad has red, green, blue, yellow colors - count red pixels as proxy for non-black
    int redCount = countRedPixelsFromBuffer();
    EXPECT_GT(redCount, 5000) << "Should have > 5000 red pixels";
}

TEST_F(E2ETest, Scene017_TriangleStrip_NoCrash) {
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.primitiveType = PrimitiveType::TRIANGLE_STRIP;
    cmd.drawParams.indexed = false;

    float aspect = 640.0f / 480.0f;
    mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
    mat4 view = lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    mat4 model = mat4(1.0f);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cmd.projectionMatrix[i * 4 + j] = proj[i][j];
            cmd.viewMatrix[i * 4 + j] = view[i][j];
            cmd.modelMatrix[i * 4 + j] = model[i][j];
        }
    }

    // Should not crash
    EXPECT_NO_THROW(m_pipeline->render(cmd));
}