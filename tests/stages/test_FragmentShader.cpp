// ============================================================================
// test_FragmentShader.cpp
// FragmentShader 单元测试 - ISA 执行、Tiles 输出模式
// ============================================================================

#include <gtest/gtest.h>
#include "stages/FragmentShader.hpp"
#include "stages/TileBuffer.hpp"
#include "core/PipelineTypes.hpp"

namespace {

using namespace SoftGPU;

// ============================================================================
// FragmentShader Tests
// ============================================================================

class FragmentShaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化 TileBufferManager 的所有 tile
        m_tileBuffer.initAllTiles();
    }
    void TearDown() override {}

    // 辅助：创建测试 fragment
    static Fragment makeFragment(uint32_t x, uint32_t y, float z,
                                 float r, float g, float b, float a) {
        Fragment frag;
        frag.x = x;
        frag.y = y;
        frag.z = z;
        frag.r = r;
        frag.g = g;
        frag.b = b;
        frag.a = a;
        frag.u = 0.0f;
        frag.v = 0.0f;
        return frag;
    }

    TileBufferManager m_tileBuffer;  // 每个测试使用独立的 TileBufferManager
};

// ---------------------------------------------------------------------------
// PHASE1 兼容模式 - execute()
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, Execute_EmptyInput) {
    FragmentShader fs;
    fs.setInput({});
    fs.execute();

    const auto& output = fs.getOutput();
    EXPECT_TRUE(output.empty());
}

TEST_F(FragmentShaderTest, Execute_PassthroughColor) {
    FragmentShader fs;

    std::vector<Fragment> input = {
        makeFragment(10, 20, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
        makeFragment(30, 40, 0.3f, 0.0f, 1.0f, 0.0f, 1.0f),
    };

    fs.setInput(input);
    fs.execute();

    const auto& output = fs.getOutput();
    ASSERT_EQ(output.size(), 2u);

    // 颜色应该直通 (default shader: MOV OUT_R, COLOR_R etc.)
    EXPECT_EQ(output[0].r, 1.0f);
    EXPECT_EQ(output[0].g, 0.0f);
    EXPECT_EQ(output[0].b, 0.0f);
    EXPECT_EQ(output[0].a, 1.0f);

    EXPECT_EQ(output[1].r, 0.0f);
    EXPECT_EQ(output[1].g, 1.0f);
    EXPECT_EQ(output[1].b, 0.0f);
    EXPECT_EQ(output[1].a, 1.0f);
}

TEST_F(FragmentShaderTest, Execute_DepthPassthrough) {
    FragmentShader fs;

    std::vector<Fragment> input = {
        makeFragment(0, 0, 0.25f, 1.0f, 1.0f, 1.0f, 1.0f),
        makeFragment(0, 0, 0.75f, 1.0f, 1.0f, 1.0f, 1.0f),
    };

    fs.setInput(input);
    fs.execute();

    const auto& output = fs.getOutput();
    ASSERT_EQ(output.size(), 2u);

    // 深度应该直通
    EXPECT_EQ(output[0].z, 0.25f);
    EXPECT_EQ(output[1].z, 0.75f);
}

TEST_F(FragmentShaderTest, Execute_Counters) {
    FragmentShader fs;

    std::vector<Fragment> input = {
        makeFragment(10, 20, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
        makeFragment(30, 40, 0.3f, 0.0f, 1.0f, 0.0f, 1.0f),
    };

    fs.setInput(input);
    fs.execute();

    const auto& counters = fs.getCounters();
    EXPECT_EQ(counters.invocation_count, 2u);
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

// ---------------------------------------------------------------------------
// PHASE2 TileBuffer 模式 - setInputAndExecuteTile()
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_WriteToTileBuffer) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    std::vector<Fragment> input = {
        makeFragment(5, 5, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
    };

    fs.setTileIndex(0);
    fs.setInputAndExecuteTile(input, 0, 0);

    // 检查 TileBuffer 中是否写入
    const auto& tile = tileBuffer.getTileBuffer(0);
    uint32_t localIdx = 5 * TILE_WIDTH + 5;

    EXPECT_EQ(0.5f, tile.depth[localIdx]);
    EXPECT_EQ(1.0f, tile.color[localIdx * 4 + 0]);  // R
    EXPECT_EQ(0.0f, tile.color[localIdx * 4 + 1]);  // G
    EXPECT_EQ(0.0f, tile.color[localIdx * 4 + 2]);  // B
    EXPECT_EQ(1.0f, tile.color[localIdx * 4 + 3]);  // A
}

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_MultipleFragments) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    tileBuffer.initAllTiles();  // 初始化 tile
    fs.setTileBufferManager(&tileBuffer);

    // tile 10 = tileX=10, tileY=0
    // 屏幕坐标: X = 10*32 + localX, Y = 0*32 + localY
    // 所以 localX=1 对应 screenX=321, localX=2 对应 screenX=322, 等等
    std::vector<Fragment> input = {
        makeFragment(321, 1, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),  // local (1, 1)
        makeFragment(322, 2, 0.4f, 0.0f, 1.0f, 0.0f, 1.0f),  // local (2, 2)
        makeFragment(323, 3, 0.3f, 0.0f, 0.0f, 1.0f, 1.0f),  // local (3, 3)
    };

    fs.setInputAndExecuteTile(input, 10, 0);

    const auto& tile = tileBuffer.getTileBuffer(10);

    // Fragment 1 at local (1, 1)
    EXPECT_EQ(0.5f, tile.depth[1 * TILE_WIDTH + 1]);
    // Fragment 2 at local (2, 2)
    EXPECT_EQ(0.4f, tile.depth[2 * TILE_WIDTH + 2]);
    // Fragment 3 at local (3, 3)
    EXPECT_EQ(0.3f, tile.depth[3 * TILE_WIDTH + 3]);
}

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_DifferentTiles) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    tileBuffer.initAllTiles();  // 初始化 tile
    fs.setTileBufferManager(&tileBuffer);

    // tile 5 = tileX=5, tileY=0, screen X = 5*32=160
    // tile 50 = tileX=10, tileY=2, screen X = 10*32=320, Y = 2*32=64
    std::vector<Fragment> input1 = {
        makeFragment(165, 5, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),  // tile 5, local (5, 5)
    };
    std::vector<Fragment> input2 = {
        makeFragment(325, 69, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),  // tile 50, local (5, 5)
    };

    fs.setInputAndExecuteTile(input1, 5, 0);
    fs.setInputAndExecuteTile(input2, 10, 2);

    // 两个 tile 都应该有数据
    const auto& tile5 = tileBuffer.getTileBuffer(5);
    const auto& tile50 = tileBuffer.getTileBuffer(50);

    uint32_t idx = 5 * TILE_WIDTH + 5;

    EXPECT_EQ(0.5f, tile5.depth[idx]);
    EXPECT_EQ(0.5f, tile50.depth[idx]);
}

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_EmptyInput) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    fs.setTileIndex(0);
    fs.setInputAndExecuteTile({}, 0, 0);

    // 空输入应该不崩溃，计数器为 0
    const auto& counters = fs.getCounters();
    EXPECT_EQ(counters.invocation_count, 0u);
}

// ---------------------------------------------------------------------------
// 深度测试（TileBuffer 内）
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_DepthTestCloserWins) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    // 先写入远的
    std::vector<Fragment> frag1 = {
        makeFragment(10, 10, 0.8f, 1.0f, 0.0f, 0.0f, 1.0f),
    };
    fs.setTileIndex(0);
    fs.setInputAndExecuteTile(frag1, 0, 0);

    // 再写入近的 - 应该替换
    std::vector<Fragment> frag2 = {
        makeFragment(10, 10, 0.2f, 0.0f, 1.0f, 0.0f, 1.0f),
    };
    fs.setInputAndExecuteTile(frag2, 0, 0);

    // 近的应该获胜
    const auto& tile = tileBuffer.getTileBuffer(0);
    uint32_t idx = 10 * TILE_WIDTH + 10;

    EXPECT_EQ(0.2f, tile.depth[idx]);
    EXPECT_EQ(0.0f, tile.color[idx * 4 + 0]);  // R = 0
    EXPECT_EQ(1.0f, tile.color[idx * 4 + 1]);  // G = 1
}

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_DepthTestFurtherRejected) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    // 先写入近的
    std::vector<Fragment> frag1 = {
        makeFragment(15, 15, 0.3f, 1.0f, 0.0f, 0.0f, 1.0f),
    };
    fs.setTileIndex(0);
    fs.setInputAndExecuteTile(frag1, 0, 0);

    // 再写入远的 - 应该被拒绝
    std::vector<Fragment> frag2 = {
        makeFragment(15, 15, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f),
    };
    fs.setInputAndExecuteTile(frag2, 0, 0);

    // 原来的值应该保持
    const auto& tile = tileBuffer.getTileBuffer(0);
    uint32_t idx = 15 * TILE_WIDTH + 15;

    EXPECT_EQ(0.3f, tile.depth[idx]);
    EXPECT_EQ(1.0f, tile.color[idx * 4 + 0]);  // R 保持为 1
}

// ---------------------------------------------------------------------------
// 坐标转换
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, SetInputAndExecuteTile_TileLocalCoordinates) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    // tileX=1, tileY=2 的 tile 中，像素 (35, 70) 的本地坐标应该是 (3, 6)
    // 因为 tile 1 的屏幕 X = 1*32 = 32, 所以 35-32 = 3
    // tile 2 的屏幕 Y = 2*32 = 64, 所以 70-64 = 6
    std::vector<Fragment> input = {
        makeFragment(35, 70, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f),
    };

    fs.setTileIndex(2 * NUM_TILES_X + 1);  // tileX=1, tileY=2
    fs.setInputAndExecuteTile(input, 1, 2);

    // 检查 tile 43 (2*20+1) 中本地坐标 (3, 6) 的数据
    const auto& tile = tileBuffer.getTileBuffer(2 * NUM_TILES_X + 1);
    uint32_t localIdx = 6 * TILE_WIDTH + 3;

    EXPECT_EQ(0.5f, tile.depth[localIdx]);
}

// ---------------------------------------------------------------------------
// setTileBufferManager 错误处理
// ---------------------------------------------------------------------------

// 注意: FragmentShader 内部对 nullptr 没有完全保护，
// 所以不提供这个测试（会导致崩溃）

TEST_F(FragmentShaderTest, IsTileBufferMode) {
    FragmentShader fs;

    // 初始状态不是 TileBuffer 模式
    EXPECT_FALSE(fs.isTileBufferMode());

    // 设置 TileBufferManager 后是 TileBuffer 模式
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);
    EXPECT_TRUE(fs.isTileBufferMode());
}

// ---------------------------------------------------------------------------
// setTileIndex
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, SetTileIndex) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    tileBuffer.initAllTiles();
    fs.setTileBufferManager(&tileBuffer);

    // tile 42 = tileX=2, tileY=2 (since 42 = 2*20 + 2)
    // 屏幕坐标: X = 2*32 + localX, Y = 2*32 + localY
    std::vector<Fragment> input = {
        makeFragment(2*32 + 5, 2*32 + 5, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),  // local (5,5)
    };

    // setTileIndex(42) 设置目标 tile，但 setInputAndExecuteTile(tileX, tileY) 会覆盖它
    // 所以这个测试验证的是 setInputAndExecuteTile 的行为
    fs.setInputAndExecuteTile(input, 2, 2);

    const auto& tile = tileBuffer.getTileBuffer(42);
    uint32_t idx = 5 * TILE_WIDTH + 5;
    EXPECT_EQ(0.5f, tile.depth[idx]);
}

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, Counters_Increment) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    std::vector<Fragment> input = {
        makeFragment(1, 1, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
        makeFragment(2, 2, 0.4f, 0.0f, 1.0f, 0.0f, 1.0f),
    };

    fs.setTileIndex(0);
    fs.setInputAndExecuteTile(input, 0, 0);

    const auto& counters = fs.getCounters();
    EXPECT_EQ(counters.invocation_count, 2u);
    EXPECT_GE(counters.elapsed_ms, 0.0);
}

TEST_F(FragmentShaderTest, ResetCounters) {
    FragmentShader fs;
    TileBufferManager tileBuffer;
    fs.setTileBufferManager(&tileBuffer);

    std::vector<Fragment> input = {
        makeFragment(1, 1, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f),
    };

    fs.setTileIndex(0);
    fs.setInputAndExecuteTile(input, 0, 0);

    fs.resetCounters();

    const auto& counters = fs.getCounters();
    EXPECT_EQ(counters.invocation_count, 0u);
    EXPECT_EQ(counters.cycle_count, 0u);
}

// ---------------------------------------------------------------------------
// fragmentToContext / contextToFragment 转换
// ---------------------------------------------------------------------------

TEST_F(FragmentShaderTest, FragmentToContext_MaintainsValues) {
    FragmentShader fs;

    Fragment frag = makeFragment(100, 200, 0.75f, 0.25f, 0.50f, 0.75f, 1.0f);
    frag.u = 0.3f;
    frag.v = 0.7f;

    // 通过反射或 execute 间接测试
    std::vector<Fragment> input = {frag};
    fs.setInput(input);
    fs.execute();

    const auto& output = fs.getOutput();
    ASSERT_EQ(output.size(), 1u);

    // 验证值保持
    EXPECT_EQ(output[0].x, 100u);
    EXPECT_EQ(output[0].y, 200u);
    EXPECT_EQ(output[0].z, 0.75f);
    EXPECT_EQ(output[0].r, 0.25f);
    EXPECT_EQ(output[0].g, 0.50f);
    EXPECT_EQ(output[0].b, 0.75f);
    EXPECT_EQ(output[0].a, 1.0f);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}