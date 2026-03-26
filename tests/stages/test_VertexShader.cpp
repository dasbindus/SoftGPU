// ============================================================================
// test_VertexShader.cpp
// ============================================================================

#include <gtest/gtest.h>
#include "stages/VertexShader.hpp"
#include "core/PipelineTypes.hpp"
#include "core/RenderCommand.hpp"

namespace {

using namespace SoftGPU;

class VertexShaderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Identity MVP Transform
// ---------------------------------------------------------------------------

TEST_F(VertexShaderTest, IdentityMVP_Passthrough) {
    VertexShader vs;

    // 3 vertices: red, green, blue
    std::vector<float> vb = {
        // v0: position (0, 0, 0), color red
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        // v1: position (1, 0, 0), color green
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        // v2: position (0, 1, 0), color blue
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
    };

    std::vector<uint32_t> ib;
    Uniforms uniforms;
    uniforms.modelMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1  // identity
    };
    uniforms.viewMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1  // identity
    };
    uniforms.projectionMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1  // identity
    };

    vs.setInput(vb, ib, uniforms);
    vs.setVertexCount(3);
    vs.execute();

    const auto& output = vs.getOutput();
    ASSERT_EQ(output.size(), 3u);

    // With identity matrices, clip space = model space
    // v0
    EXPECT_NEAR(output[0].x, 0.0f, 1e-5f);
    EXPECT_NEAR(output[0].y, 0.0f, 1e-5f);
    EXPECT_NEAR(output[0].z, 0.0f, 1e-5f);
    EXPECT_NEAR(output[0].w, 1.0f, 1e-5f);
    EXPECT_NEAR(output[0].r, 1.0f, 1e-5f);
    EXPECT_NEAR(output[0].g, 0.0f, 1e-5f);
    EXPECT_NEAR(output[0].b, 0.0f, 1e-5f);

    // v1
    EXPECT_NEAR(output[1].x, 1.0f, 1e-5f);
    EXPECT_NEAR(output[1].y, 0.0f, 1e-5f);
    EXPECT_NEAR(output[1].z, 0.0f, 1e-5f);
    EXPECT_NEAR(output[1].w, 1.0f, 1e-5f);

    // v2
    EXPECT_NEAR(output[2].x, 0.0f, 1e-5f);
    EXPECT_NEAR(output[2].y, 1.0f, 1e-5f);
    EXPECT_NEAR(output[2].z, 0.0f, 1e-5f);
    EXPECT_NEAR(output[2].w, 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Translation Model Matrix
// ---------------------------------------------------------------------------

TEST_F(VertexShaderTest, TranslationModelMatrix) {
    VertexShader vs;

    // 2 vertices, 8 floats each (pos + color)
    std::vector<float> vb = {
        // v0: position (0, 0, 0), color placeholder
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        // v1: position (1, 0, 0), color placeholder
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    std::vector<uint32_t> ib;
    Uniforms uniforms;
    // Translation by (10, 20, 30) in column-major (GLM format):
    // m[col*4 + row] => array = [col0x,col0y,col0z,col0w, col1x,...]
    // Translation col 3 = (10,20,30,1): indices 12,13,14,15
    uniforms.modelMatrix = {
        1,0,0,0,    // col 0: (1,0,0,0) => m[0]=1, m[1]=0, m[2]=0, m[3]=0
        0,1,0,0,    // col 1: (0,1,0,0) => m[4]=0, m[5]=1, m[6]=0, m[7]=0
        0,0,1,0,    // col 2: (0,0,1,0) => m[8]=0, m[9]=0, m[10]=1, m[11]=0
        10,20,30,1  // col 3: (10,20,30,1) => m[12]=10, m[13]=20, m[14]=30, m[15]=1
    };
    uniforms.viewMatrix = {
        1,0,0,0,   // col 0
        0,1,0,0,   // col 1
        0,0,1,0,   // col 2
        0,0,0,1    // col 3
    };
    uniforms.projectionMatrix = {
        1,0,0,0,   // col 0
        0,1,0,0,   // col 1
        0,0,1,0,   // col 2
        0,0,0,1    // col 3
    };

    vs.setInput(vb, ib, uniforms);
    vs.setVertexCount(2);
    vs.execute();

    const auto& output = vs.getOutput();
    ASSERT_EQ(output.size(), 2u);

    // After translation by (10, 20, 30), v0 at (0,0,0) should become (10, 20, 30)
    EXPECT_NEAR(output[0].x, 10.0f, 1e-4f);
    EXPECT_NEAR(output[0].y, 20.0f, 1e-4f);
    EXPECT_NEAR(output[0].z, 30.0f, 1e-4f);

    // v1 at (1,0,0) should become (11, 20, 30)
    EXPECT_NEAR(output[1].x, 11.0f, 1e-4f);
    EXPECT_NEAR(output[1].y, 20.0f, 1e-4f);
    EXPECT_NEAR(output[1].z, 30.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

TEST_F(VertexShaderTest, Counters) {
    VertexShader vs;

    std::vector<float> vb(12 * 5, 0.0f);  // 5 vertices
    std::vector<uint32_t> ib;
    Uniforms uniforms = {};

    vs.setInput(vb, ib, uniforms);
    vs.setVertexCount(5);
    vs.execute();

    const auto& counters = vs.getCounters();
    EXPECT_EQ(counters.invocation_count, 5u);
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
