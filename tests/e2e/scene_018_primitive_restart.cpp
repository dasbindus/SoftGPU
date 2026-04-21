// ============================================================================
// scene_018_primitive_restart.cpp - E2E Scene 018: Primitive Restart
//
// Test: E2E-SCENE-018
// Target: Golden reference test for Primitive Restart rendering
//
// This test renders using indexed draw with primitive restart and compares
// against golden reference.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using glm::mat4;
using glm::perspective;
using glm::lookAt;
using glm::identity;
using glm::radians;

// ============================================================================
// Test: Primitive Restart renders correctly
// ============================================================================
TEST_F(E2ETest, Scene018_PrimitiveRestart_GoldenReference) {
    // 4 vertices for two separate triangles
    std::vector<float> vertices = {
        // x, y, z, w, r, g, b, a (8 floats per vertex)
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // 0: bottom-left (red)
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // 1: bottom-right (green)
         0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  // 2: top (blue)
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,  // 3: second triangle vertex
    };

    // 9 indices: 3 triangles but middle one has restart markers
    std::vector<uint32_t> indices = {
        0, 1, 2,      // first triangle
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,  // restart
        1, 3, 2       // third triangle (second triangle skipped)
    };

    // Build render command with primitive restart
    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.indexed = true;
    cmd.drawParams.indexCount = static_cast<uint32_t>(indices.size());
    cmd.drawParams.primitiveRestartEnabled = true;
    cmd.drawParams.primitiveRestartIndex = 0xFFFFFFFF;

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

    // Set index buffer
    cmd.indexBufferData = indices.data();
    cmd.indexBufferSize = static_cast<size_t>(indices.size());

    // Render
    m_pipeline->render(cmd);

    // Dump PPM
    std::string ppmPath = dumpPPM("scene018_primitive_restart.ppm");

    // Verify PPM was created
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded()) << "PPM file should load: " << ppmPath;

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene018_primitive_restart.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.02f);
    EXPECT_TRUE(match) << "Scene018: Primitive Restart should match golden reference";
}

TEST_F(E2ETest, Scene018_PrimitiveRestart_NonBlackPixels) {
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
         0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        1, 3, 2
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.indexed = true;
    cmd.drawParams.indexCount = static_cast<uint32_t>(indices.size());
    cmd.drawParams.primitiveRestartEnabled = true;
    cmd.drawParams.primitiveRestartIndex = 0xFFFFFFFF;
    cmd.indexBufferData = indices.data();
    cmd.indexBufferSize = static_cast<size_t>(indices.size());

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

    // Triangles have red, green, blue colors - count blue pixels as proxy for non-black
    int blueCount = countBluePixelsFromBuffer();
    EXPECT_GT(blueCount, 2000) << "Should have > 2000 blue pixels";
}

TEST_F(E2ETest, Scene018_PrimitiveRestart_NoCrash) {
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,
         0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
        1, 3, 2
    };

    RenderCommand cmd;
    cmd.vertexBufferData = vertices.data();
    cmd.vertexBufferSize = static_cast<size_t>(vertices.size());
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertices.size() / 8);
    cmd.drawParams.indexed = true;
    cmd.drawParams.indexCount = static_cast<uint32_t>(indices.size());
    cmd.drawParams.primitiveRestartEnabled = true;
    cmd.drawParams.primitiveRestartIndex = 0xFFFFFFFF;
    cmd.indexBufferData = indices.data();
    cmd.indexBufferSize = static_cast<size_t>(indices.size());

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