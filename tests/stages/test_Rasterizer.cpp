// ============================================================================
// test_Rasterizer.cpp
// ============================================================================

#include <gtest/gtest.h>
#include "stages/Rasterizer.hpp"
#include "core/PipelineTypes.hpp"

namespace {

using namespace SoftGPU;

class RasterizerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Unit Triangle Rasterization
// ---------------------------------------------------------------------------

TEST_F(RasterizerTest, UnitTriangle_CoversExpectedPixels) {
    Rasterizer rs;
    rs.setViewport(640, 480);

    // Simple unit triangle in NDC
    Triangle tri;
    tri.v[0] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    tri.v[1] = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    tri.v[2] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    tri.culled = false;

    std::vector<Triangle> triangles = {tri};

    rs.setInput(triangles);
    rs.execute();

    const auto& output = rs.getOutput();
    // Should have some fragments (area = 0.5 in NDC, mapped to screen)
    EXPECT_GT(output.size(), 0u);

    // All fragments should have valid screen coordinates
    for (const auto& frag : output) {
        EXPECT_LT(frag.x, 640u);
        EXPECT_LT(frag.y, 480u);
        // Color should be reasonable
        EXPECT_GE(frag.r, 0.0f);
        EXPECT_LE(frag.r, 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Full Screen Triangle
// ---------------------------------------------------------------------------

TEST_F(RasterizerTest, LargeTriangle_ManyFragments) {
    Rasterizer rs;
    rs.setViewport(640, 480);

    // Large triangle covering most of the screen
    Triangle tri;
    tri.v[0] = {-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f};
    tri.v[1] = { 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,  1.0f, -1.0f, 0.0f};
    tri.v[2] = { 0.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  0.0f,  1.0f, 0.0f};
    tri.culled = false;

    std::vector<Triangle> triangles = {tri};

    rs.setInput(triangles);
    rs.execute();

    const auto& output = rs.getOutput();
    // Large triangle should produce many fragments
    EXPECT_GT(output.size(), 1000u);
}

// ---------------------------------------------------------------------------
// Culled Triangle - No Fragments
// ---------------------------------------------------------------------------

TEST_F(RasterizerTest, CulledTriangle_NoFragments) {
    Rasterizer rs;
    rs.setViewport(640, 480);

    Triangle tri;
    tri.culled = true;  // Pre-culled

    std::vector<Triangle> triangles = {tri};

    rs.setInput(triangles);
    rs.execute();

    const auto& output = rs.getOutput();
    EXPECT_EQ(output.size(), 0u);
}

// ---------------------------------------------------------------------------
// Multiple Triangles
// ---------------------------------------------------------------------------

TEST_F(RasterizerTest, MultipleTriangles_SeparateFragments) {
    Rasterizer rs;
    rs.setViewport(640, 480);

    Triangle tri1;
    tri1.v[0] = {-0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f};
    tri1.v[1] = { 0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,  0.0f,  0.0f, 0.0f};
    tri1.v[2] = {-0.5f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.5f,  0.0f, 0.0f};
    tri1.culled = false;

    Triangle tri2;
    tri2.v[0] = {0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.0f};
    tri2.v[1] = {0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f,  0.0f, 0.0f};
    tri2.v[2] = {0.0f,  0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f,  0.0f, 0.0f};
    tri2.culled = false;

    std::vector<Triangle> triangles = {tri1, tri2};

    rs.setInput(triangles);
    rs.execute();

    const auto& output = rs.getOutput();
    EXPECT_GT(output.size(), 0u);

    const auto& counters = rs.getCounters();
    EXPECT_EQ(counters.invocation_count, 2u);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

TEST_F(RasterizerTest, Counters) {
    Rasterizer rs;
    rs.setViewport(640, 480);

    Triangle tri;
    tri.v[0] = {0.0f, 0.0f, 0.0f, 1.0f, 1,0,0,1, 0.0f, 0.0f, 0.0f};
    tri.v[1] = {1.0f, 0.0f, 0.0f, 1.0f, 0,1,0,1, 1.0f, 0.0f, 0.0f};
    tri.v[2] = {0.0f, 1.0f, 0.0f, 1.0f, 0,0,1,1, 0.0f, 1.0f, 0.0f};
    tri.culled = false;

    std::vector<Triangle> triangles = {tri};

    rs.setInput(triangles);
    rs.execute();

    const auto& counters = rs.getCounters();
    EXPECT_EQ(counters.invocation_count, 1u);
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
