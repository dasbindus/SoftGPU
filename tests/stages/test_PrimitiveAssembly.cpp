// ============================================================================
// test_PrimitiveAssembly.cpp
// ============================================================================

#include <gtest/gtest.h>
#include "stages/PrimitiveAssembly.hpp"
#include "core/PipelineTypes.hpp"
#include "core/HardwareConfig.hpp"

namespace {

using namespace SoftGPU;

class PrimitiveAssemblyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Single Triangle Passthrough
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, SingleTriangle_NDCInside) {
    PrimitiveAssembly pa;

    // 3 vertices already in NDC-ready clip space (w=1 so NDC = clip)
    std::vector<Vertex> vertices = {
        {0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f},  // top
        {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f},  // bottom left
        {0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.0f},  // bottom right
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);  // false = non-indexed
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_FALSE(output[0].culled);
}

// ---------------------------------------------------------------------------
// AABB Frustum Culling - Fully Outside Left
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, Culled_OutsideLeft) {
    PrimitiveAssembly pa;

    // Triangle completely to the left of frustum (NDC x > 1.0 means all vertices on left side)
    std::vector<Vertex> vertices = {
        {-2.0f, 0.0f, 0.0f, 1.0f, 1,0,0,1, -2.0f, 0.0f, 0.0f},
        {-2.5f, 0.0f, 0.0f, 1.0f, 0,1,0,1, -2.5f, 0.0f, 0.0f},
        {-2.0f, 0.5f, 0.0f, 1.0f, 0,0,1,1, -2.0f, 0.5f, 0.0f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);  // false = non-indexed
    pa.execute();

    const auto& output = pa.getOutput();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_TRUE(output[0].culled);
}

// ---------------------------------------------------------------------------
// AABB Frustum Culling - Fully Inside
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, Pass_Inside) {
    PrimitiveAssembly pa;

    std::vector<Vertex> vertices = {
        {0.0f, 0.0f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f, 1.0f, 0,1,0,1, 0.5f, 0.0f, -0.5f},
        {0.0f, 0.5f, -0.5f, 1.0f, 0,0,1,1, 0.0f, 0.5f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);  // false = non-indexed
    pa.execute();

    const auto& output = pa.getOutput();
    ASSERT_EQ(output.size(), 1u);
    EXPECT_FALSE(output[0].culled);
}

// ---------------------------------------------------------------------------
// Back-face Culling - CCW Front-face, Cull Back (Default OpenGL-like)
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, BackfaceCull_CCWFront_CullBack) {
    PrimitiveAssembly pa;

    // Config: CCW=front, cull back
    HardwareConfig config;
    config.primitiveAssembly.enable = true;
    config.primitiveAssembly.cullBack = true;
    config.primitiveAssembly.frontFaceCCW = true;
    pa.setConfig(config);
    pa.setViewport(640, 480);

    // CCW triangle in NDC (Y-up). After NDC->screen (Y-down flip),
    // CCW becomes CW in screen space.
    // CW in screen = back face with cullBack=true, should be culled.
    std::vector<Vertex> vertices = {
        {0.0f, 0.5f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f, 1.0f, 0,1,0,1, -0.5f, -0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f, 1.0f, 0,0,1,1, 0.5f, -0.5f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_TRUE(output[0].culled);  // NDC CCW -> screen CW -> back -> culled
}

// ---------------------------------------------------------------------------
// Back-face Culling - CW Triangle with CCW Front-face
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, BackfaceCull_CWTriangle_CullBack) {
    PrimitiveAssembly pa;

    HardwareConfig config;
    config.primitiveAssembly.enable = true;
    config.primitiveAssembly.cullBack = true;
    config.primitiveAssembly.frontFaceCCW = true;
    pa.setConfig(config);
    pa.setViewport(640, 480);

    // CW triangle in NDC (Y-up). After NDC->screen (Y-down flip),
    // CW becomes CCW in screen space.
    // CCW in screen = front face with cullBack=true, should NOT be culled.
    std::vector<Vertex> vertices = {
        {0.0f, 0.5f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f, 1.0f, 0,1,0,1, 0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f, 1.0f, 0,0,1,1, -0.5f, -0.5f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_FALSE(output[0].culled);  // NDC CW -> screen CCW -> front -> pass
}

// ---------------------------------------------------------------------------
// Back-face Culling - Degenerate Triangle (Zero Area)
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, BackfaceCull_DegenerateTriangle) {
    PrimitiveAssembly pa;

    HardwareConfig config;
    config.primitiveAssembly.enable = true;
    config.primitiveAssembly.cullBack = true;
    config.primitiveAssembly.frontFaceCCW = true;
    pa.setConfig(config);
    pa.setViewport(640, 480);

    // Degenerate: all points collinear (area = 0)
    std::vector<Vertex> vertices = {
        {0.0f, 0.0f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f, 1.0f, 0,1,0,1, 0.5f, 0.0f, -0.5f},
        {1.0f, 0.0f, -0.5f, 1.0f, 0,0,1,1, 1.0f, 0.0f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_TRUE(output[0].culled);  // area = 0, should be culled
}

// ---------------------------------------------------------------------------
// Back-face Culling - Cull Front Mode
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, BackfaceCull_CullFront) {
    PrimitiveAssembly pa;

    HardwareConfig config;
    config.primitiveAssembly.enable = true;
    config.primitiveAssembly.cullBack = false;  // cull front instead
    config.primitiveAssembly.frontFaceCCW = true;
    pa.setConfig(config);
    pa.setViewport(640, 480);

    // CW triangle in NDC -> CCW in screen.
    // With cullFront mode, CCW (front) is culled.
    std::vector<Vertex> vertices = {
        {0.0f, 0.5f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f, 1.0f, 0,1,0,1, 0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f, 1.0f, 0,0,1,1, -0.5f, -0.5f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_TRUE(output[0].culled);  // NDC CW -> screen CCW -> front -> cullFront = culled
}

// ---------------------------------------------------------------------------
// Back-face Culling - Disabled Should Not Cull Any
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, BackfaceCull_Disabled) {
    PrimitiveAssembly pa;

    HardwareConfig config;
    config.primitiveAssembly.enable = false;  // disabled
    config.primitiveAssembly.cullBack = true;
    pa.setConfig(config);
    pa.setViewport(640, 480);

    // CW triangle would normally be culled, but culling is disabled
    std::vector<Vertex> vertices = {
        {0.0f, 0.5f, -0.5f, 1.0f, 1,0,0,1, 0.0f, 0.5f, -0.5f},
        {0.5f, -0.5f, -0.5f, 1.0f, 0,1,0,1, 0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f, 1.0f, 0,0,1,1, -0.5f, -0.5f, -0.5f},
    };
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 1u);
    EXPECT_FALSE(output[0].culled);  // culling disabled, should pass
}

// ---------------------------------------------------------------------------
// Indexed Draw
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, IndexedDraw) {
    PrimitiveAssembly pa;

    // 4 vertices forming a quad
    std::vector<Vertex> vertices = {
        {-1.0f, -1.0f, 0.0f, 1.0f, 1,0,0,1, -1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f, 1.0f, 0,1,0,1,  1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f, 1.0f, 0,0,1,1,  1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f, 1.0f, 1,1,0,1, -1.0f,  1.0f, 0.0f},
    };

    // 2 triangles via indices
    std::vector<uint32_t> indices = {
        0, 1, 2,  // first triangle
        0, 2, 3   // second triangle
    };

    pa.setInput(vertices, indices, true);  // true = indexed
    pa.execute();

    const auto& output = pa.getOutput();
    EXPECT_EQ(output.size(), 2u);
    EXPECT_FALSE(output[0].culled);
    EXPECT_FALSE(output[1].culled);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

TEST_F(PrimitiveAssemblyTest, Counters) {
    PrimitiveAssembly pa;

    std::vector<Vertex> vertices(3);  // 3 vertices = 1 triangle
    std::vector<uint32_t> indices;

    pa.setInput(vertices, indices, false);  // false = non-indexed
    pa.execute();

    const auto& counters = pa.getCounters();
    EXPECT_EQ(counters.invocation_count, 1u);  // 1 triangle processed
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
