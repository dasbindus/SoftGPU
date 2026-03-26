// ============================================================================
// test_Framebuffer.cpp
// ============================================================================

#include <gtest/gtest.h>
#include "stages/Framebuffer.hpp"
#include "core/PipelineTypes.hpp"

namespace {

using namespace SoftGPU;

class FramebufferTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, Clear_SetsAllPixels) {
    Framebuffer fb;

    float clearColor[4] = {0.5f, 0.3f, 0.8f, 1.0f};
    fb.clear(clearColor);

    const float* color = fb.getColorBuffer();
    const float* depth = fb.getDepthBuffer();

    // Check a few pixels
    for (size_t i = 0; i < Framebuffer::PIXEL_COUNT; ++i) {
        EXPECT_NEAR(color[i * 4 + 0], 0.5f, 1e-5f);
        EXPECT_NEAR(color[i * 4 + 1], 0.3f, 1e-5f);
        EXPECT_NEAR(color[i * 4 + 2], 0.8f, 1e-5f);
        EXPECT_NEAR(color[i * 4 + 3], 1.0f, 1e-5f);
        EXPECT_NEAR(depth[i], CLEAR_DEPTH, 1e-5f);
    }
}

// ---------------------------------------------------------------------------
// Single Fragment Write
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, SingleFragment_Write) {
    Framebuffer fb;
    fb.clear();

    Fragment frag;
    frag.x = 100;
    frag.y = 200;
    frag.z = 0.5f;
    frag.r = 1.0f;
    frag.g = 0.0f;
    frag.b = 0.0f;
    frag.a = 1.0f;

    std::vector<Fragment> fragments = {frag};

    fb.setInput(fragments);
    fb.execute();

    const float* color = fb.getColorBuffer();
    const float* depth = fb.getDepthBuffer();

    size_t idx = 200 * Framebuffer::WIDTH + 100;

    EXPECT_NEAR(color[idx * 4 + 0], 1.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 1], 0.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 2], 0.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 3], 1.0f, 1e-5f);
    EXPECT_NEAR(depth[idx], 0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Depth Test Pass
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, DepthTest_Pass) {
    Framebuffer fb;
    fb.clear();

    // First fragment at z=0.5
    Fragment frag1;
    frag1.x = 50; frag1.y = 50;
    frag1.z = 0.5f;
    frag1.r = 1.0f; frag1.g = 0.0f; frag1.b = 0.0f; frag1.a = 1.0f;

    fb.setInput({frag1});
    fb.execute();

    // Second fragment at same pixel, z=0.3 (closer)
    Fragment frag2;
    frag2.x = 50; frag2.y = 50;
    frag2.z = 0.3f;
    frag2.r = 0.0f; frag2.g = 1.0f; frag2.b = 0.0f; frag2.a = 1.0f;

    fb.setInput({frag2});
    fb.execute();

    const float* color = fb.getColorBuffer();
    const float* depth = fb.getDepthBuffer();

    size_t idx = 50 * Framebuffer::WIDTH + 50;

    // Should have the second fragment's green color
    EXPECT_NEAR(color[idx * 4 + 0], 0.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 1], 1.0f, 1e-5f);
    EXPECT_NEAR(depth[idx], 0.3f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Depth Test Fail
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, DepthTest_Fail) {
    Framebuffer fb;
    fb.clear();

    // First fragment at z=0.3
    Fragment frag1;
    frag1.x = 75; frag1.y = 75;
    frag1.z = 0.3f;
    frag1.r = 1.0f; frag1.g = 0.0f; frag1.b = 0.0f; frag1.a = 1.0f;

    fb.setInput({frag1});
    fb.execute();

    // Second fragment at same pixel, z=0.5 (farther, should fail)
    Fragment frag2;
    frag2.x = 75; frag2.y = 75;
    frag2.z = 0.5f;
    frag2.r = 0.0f; frag2.g = 1.0f; frag2.b = 0.0f; frag2.a = 1.0f;

    fb.setInput({frag2});
    fb.execute();

    const float* color = fb.getColorBuffer();
    const float* depth = fb.getDepthBuffer();

    size_t idx = 75 * Framebuffer::WIDTH + 75;

    // Should still have the first fragment's red color
    EXPECT_NEAR(color[idx * 4 + 0], 1.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 1], 0.0f, 1e-5f);
    EXPECT_NEAR(depth[idx], 0.3f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Depth Test Disabled
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, DepthTest_Disabled) {
    Framebuffer fb;
    fb.clear();
    fb.setDepthTestEnabled(false);

    Fragment frag1;
    frag1.x = 80; frag1.y = 80;
    frag1.z = 0.3f;
    frag1.r = 1.0f; frag1.g = 0.0f; frag1.b = 0.0f; frag1.a = 1.0f;

    fb.setInput({frag1});
    fb.execute();

    Fragment frag2;
    frag2.x = 80; frag2.y = 80;
    frag2.z = 0.5f;
    frag2.r = 0.0f; frag2.g = 1.0f; frag2.b = 0.0f; frag2.a = 1.0f;

    fb.setInput({frag2});
    fb.execute();

    const float* color = fb.getColorBuffer();
    size_t idx = 80 * Framebuffer::WIDTH + 80;

    // Should have second fragment's color (depth test disabled)
    EXPECT_NEAR(color[idx * 4 + 0], 0.0f, 1e-5f);
    EXPECT_NEAR(color[idx * 4 + 1], 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Out of Bounds Fragment
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, OutOfBounds_Ignored) {
    Framebuffer fb;
    fb.clear();

    Fragment frag;
    frag.x = 9999;  // Out of bounds
    frag.y = 9999;
    frag.z = 0.5f;
    frag.r = 1.0f; frag.g = 0.0f; frag.b = 0.0f; frag.a = 1.0f;

    fb.setInput({frag});
    fb.execute();

    // Framebuffer should still have clear color
    const float* color = fb.getColorBuffer();
    EXPECT_NEAR(color[0], 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

TEST_F(FramebufferTest, Counters) {
    Framebuffer fb;
    fb.clear();

    Fragment frag;
    frag.x = 10; frag.y = 10;
    frag.z = 0.5f;
    frag.r = 1.0f; frag.g = 0.0f; frag.b = 0.0f; frag.a = 1.0f;

    fb.setInput({frag});
    fb.execute();

    const auto& counters = fb.getCounters();
    EXPECT_EQ(counters.invocation_count, 1u);
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
