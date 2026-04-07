// ============================================================================
// test_TileBuffer.cpp
// TileBufferManager 单元测试 - LMEM 操作
// ============================================================================

#include <gtest/gtest.h>
#include "stages/TileBuffer.hpp"
#include "core/PipelineTypes.hpp"
#include <array>
#include <cstring>

namespace {

using namespace SoftGPU;

// ============================================================================
// TileBufferManager Tests
// ============================================================================

class TileBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_manager.resetStats();
    }
    void TearDown() override {}

    TileBufferManager m_manager;
};

// ---------------------------------------------------------------------------
// 初始化测试
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, InitAllTiles_ClearsDepthToFarPlane) {
    m_manager.initAllTiles();

    // 检查多个 tile 都是清澈状态
    for (uint32_t i = 0; i < NUM_TILES; ++i) {
        const auto& tile = m_manager.getTileBuffer(i);
        for (uint32_t j = 0; j < TILE_SIZE; ++j) {
            EXPECT_EQ(tile.depth[j], CLEAR_DEPTH)
                << "Tile " << i << ", index " << j << " should be CLEAR_DEPTH";
        }
    }
}

TEST_F(TileBufferTest, InitTile_ClearsSpecificTile) {
    // 先写入一些数据
    TileBuffer& tile0 = m_manager.getTileBuffer(0);
    tile0.depth.fill(0.5f);
    tile0.color.fill(1.0f);

    // 只清除 tile 0
    m_manager.initTile(0);

    // Tile 0 应该被清除
    const auto& cleared = m_manager.getTileBuffer(0);
    for (uint32_t j = 0; j < TILE_SIZE; ++j) {
        EXPECT_EQ(cleared.depth[j], CLEAR_DEPTH);
    }

    // 其他 tile 不应受影响（它们是默认构造的，也有 CLEAR_DEPTH）
    for (uint32_t i = 1; i < NUM_TILES; ++i) {
        const auto& tile = m_manager.getTileBuffer(i);
        for (uint32_t j = 0; j < TILE_SIZE; ++j) {
            EXPECT_EQ(tile.depth[j], CLEAR_DEPTH);
        }
    }
}

// ---------------------------------------------------------------------------
// depthTestAndWrite - 深度测试通过
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, DepthTestAndWrite_PassWhenCloser) {
    // 初始化 tile
    m_manager.initTile(0);

    // 第一次写入 - 应该通过
    float z1 = 0.5f;
    float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    bool result = m_manager.depthTestAndWrite(0, 0, 0, z1, color1);

    EXPECT_TRUE(result);
    EXPECT_EQ(1u, m_manager.getDepthTestCount());
    EXPECT_EQ(0u, m_manager.getDepthRejectCount());

    // 检查数据是否写入
    const auto& tile = m_manager.getTileBuffer(0);
    EXPECT_EQ(z1, tile.depth[0]);
    EXPECT_EQ(color1[0], tile.color[0]);
    EXPECT_EQ(color1[1], tile.color[1]);
    EXPECT_EQ(color1[2], tile.color[2]);
    EXPECT_EQ(color1[3], tile.color[3]);
}

TEST_F(TileBufferTest, DepthTestAndWrite_ReplaceWhenCloser) {
    m_manager.initTile(0);

    // 第一次写入
    float z1 = 0.8f;
    float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    m_manager.depthTestAndWrite(0, 5, 5, z1, color1);

    // 第二次写入更近的深度 - 应该通过并替换
    float z2 = 0.3f;
    float color2[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    bool result = m_manager.depthTestAndWrite(0, 5, 5, z2, color2);

    EXPECT_TRUE(result);

    const auto& tile = m_manager.getTileBuffer(0);
    EXPECT_EQ(z2, tile.depth[5 * TILE_WIDTH + 5]);
    EXPECT_EQ(color2[0], tile.color[(5 * TILE_WIDTH + 5) * 4 + 0]);
    EXPECT_EQ(color2[1], tile.color[(5 * TILE_WIDTH + 5) * 4 + 1]);
}

// ---------------------------------------------------------------------------
// depthTestAndWrite - 深度测试失败
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, DepthTestAndWrite_RejectWhenFarther) {
    m_manager.initTile(0);

    // 第一次写入较近的深度
    float z1 = 0.3f;
    float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    m_manager.depthTestAndWrite(0, 10, 10, z1, color1);

    // 第二次写入较远的深度 - 应该被拒绝
    float z2 = 0.8f;
    float color2[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    bool result = m_manager.depthTestAndWrite(0, 10, 10, z2, color2);

    EXPECT_FALSE(result);
    EXPECT_EQ(2u, m_manager.getDepthTestCount());
    EXPECT_EQ(1u, m_manager.getDepthRejectCount());

    // 数据应该保持第一次写入的值
    const auto& tile = m_manager.getTileBuffer(0);
    EXPECT_EQ(z1, tile.depth[10 * TILE_WIDTH + 10]);
}

TEST_F(TileBufferTest, DepthTestAndWrite_RejectEqualDepth) {
    m_manager.initTile(0);

    float z = 0.5f;
    float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    float color2[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    m_manager.depthTestAndWrite(0, 3, 3, z, color1);
    bool result = m_manager.depthTestAndWrite(0, 3, 3, z, color2);

    // 相等深度应该被拒绝 (z < oldZ 不满足)
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, m_manager.getDepthRejectCount());
}

// ---------------------------------------------------------------------------
// depthTestAndWrite - 边界条件
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, DepthTestAndWrite_TileBoundary) {
    m_manager.initTile(0);

    // 写入 tile 边角像素
    float z = 0.5f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // 左上角
    EXPECT_TRUE(m_manager.depthTestAndWrite(0, 0, 0, z, color));

    // 右上角
    EXPECT_TRUE(m_manager.depthTestAndWrite(0, TILE_WIDTH - 1, 0, z, color));

    // 左下角
    EXPECT_TRUE(m_manager.depthTestAndWrite(0, 0, TILE_HEIGHT - 1, z, color));

    // 右下角
    EXPECT_TRUE(m_manager.depthTestAndWrite(0, TILE_WIDTH - 1, TILE_HEIGHT - 1, z, color));
}

TEST_F(TileBufferTest, DepthTestAndWrite_OutOfBounds_ReturnsFalse) {
    m_manager.initTile(0);

    float z = 0.5f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // 越界 X
    EXPECT_FALSE(m_manager.depthTestAndWrite(0, TILE_WIDTH, 0, z, color));
    EXPECT_FALSE(m_manager.depthTestAndWrite(0, 100, 0, z, color));

    // 越界 Y
    EXPECT_FALSE(m_manager.depthTestAndWrite(0, 0, TILE_HEIGHT, z, color));
    EXPECT_FALSE(m_manager.depthTestAndWrite(0, 0, 100, z, color));

    // 两者都越界
    EXPECT_FALSE(m_manager.depthTestAndWrite(0, TILE_WIDTH, TILE_HEIGHT, z, color));
}

// ---------------------------------------------------------------------------
// depthTestAndWrite - 多个 tile
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, DepthTestAndWrite_MultipleTiles_Independent) {
    m_manager.initAllTiles();

    float z = 0.5f;
    float color[4] = {0.5f, 0.5f, 0.5f, 1.0f};

    // 写入不同 tile 的相同相对位置
    m_manager.depthTestAndWrite(0, 5, 5, z, color);
    m_manager.depthTestAndWrite(1, 5, 5, z, color);
    m_manager.depthTestAndWrite(299, 5, 5, z, color);  // 最后一个 tile

    // 每个 tile 应该有独立的数据
    const auto& tile0 = m_manager.getTileBuffer(0);
    const auto& tile1 = m_manager.getTileBuffer(1);
    const auto& tile299 = m_manager.getTileBuffer(299);

    size_t idx = 5 * TILE_WIDTH + 5;

    EXPECT_EQ(z, tile0.depth[idx]);
    EXPECT_EQ(z, tile1.depth[idx]);
    EXPECT_EQ(z, tile299.depth[idx]);
}

// ---------------------------------------------------------------------------
// GMEM 同步测试
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, LoadFromGMEM_CopiesDataCorrectly) {
    m_manager.initTile(0);

    // 构造模拟 GMEM 数据
    std::array<float, TILE_SIZE * 4> gmemColor;
    std::array<float, TILE_SIZE> gmemDepth;

    for (size_t i = 0; i < TILE_SIZE; ++i) {
        gmemDepth[i] = static_cast<float>(i) * 0.001f;
        for (size_t c = 0; c < 4; ++c) {
            gmemColor[i * 4 + c] = static_cast<float>(i + c) * 0.01f;
        }
    }

    // 从 GMEM 加载到 tile 0
    m_manager.loadFromGMEM(0, gmemColor.data(), gmemDepth.data());

    // 验证数据
    const auto& tile = m_manager.getTileBuffer(0);
    for (size_t i = 0; i < TILE_SIZE; ++i) {
        EXPECT_EQ(gmemDepth[i], tile.depth[i])
            << "Depth mismatch at index " << i;
        for (size_t c = 0; c < 4; ++c) {
            EXPECT_EQ(gmemColor[i * 4 + c], tile.color[i * 4 + c])
                << "Color mismatch at index " << i << ", channel " << c;
        }
    }
}

TEST_F(TileBufferTest, StoreToGMEM_CopiesDataCorrectly) {
    m_manager.initTile(0);

    // 写入一些数据
    TileBuffer& tile = m_manager.getTileBuffer(0);
    for (size_t i = 0; i < TILE_SIZE; ++i) {
        tile.depth[i] = static_cast<float>(i) * 0.001f;
        for (size_t c = 0; c < 4; ++c) {
            tile.color[i * 4 + c] = static_cast<float>(i + c) * 0.01f;
        }
    }

    // 存储到 GMEM
    std::array<float, TILE_SIZE * 4> outColor;
    std::array<float, TILE_SIZE> outDepth;
    outColor.fill(0.0f);
    outDepth.fill(0.0f);

    m_manager.storeToGMEM(0, outColor.data(), outDepth.data());

    // 验证数据
    for (size_t i = 0; i < TILE_SIZE; ++i) {
        EXPECT_EQ(tile.depth[i], outDepth[i])
            << "Depth mismatch at index " << i;
        for (size_t c = 0; c < 4; ++c) {
            EXPECT_EQ(tile.color[i * 4 + c], outColor[i * 4 + c])
                << "Color mismatch at index " << i << ", channel " << c;
        }
    }
}

TEST_F(TileBufferTest, LoadStoreGMEM_RoundTrip) {
    // 初始化并写入一些数据
    m_manager.initTile(42);
    TileBuffer& tile = m_manager.getTileBuffer(42);
    for (size_t i = 0; i < TILE_SIZE; ++i) {
        tile.depth[i] = static_cast<float>(i * 7 % 1000) * 0.001f;
        for (size_t c = 0; c < 4; ++c) {
            tile.color[i * 4 + c] = static_cast<float>((i + c) * 3 % 1000) * 0.001f;
        }
    }

    // 存储到 GMEM
    std::array<float, TILE_SIZE * 4> gmemColor;
    std::array<float, TILE_SIZE> gmemDepth;
    m_manager.storeToGMEM(42, gmemColor.data(), gmemDepth.data());

    // 清除 tile
    m_manager.initTile(42);

    // 从 GMEM 加载
    m_manager.loadFromGMEM(42, gmemColor.data(), gmemDepth.data());

    // 验证数据一致
    const auto& reloaded = m_manager.getTileBuffer(42);
    for (size_t i = 0; i < TILE_SIZE; ++i) {
        EXPECT_EQ(tile.depth[i], reloaded.depth[i])
            << "Depth mismatch at index " << i;
        for (size_t c = 0; c < 4; ++c) {
            EXPECT_EQ(tile.color[i * 4 + c], reloaded.color[i * 4 + c])
                << "Color mismatch at index " << i << ", channel " << c;
        }
    }
}

// ---------------------------------------------------------------------------
// 统计测试
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, Stats_IncrementOnDepthTest) {
    m_manager.initTile(0);

    EXPECT_EQ(0u, m_manager.getDepthTestCount());
    EXPECT_EQ(0u, m_manager.getDepthRejectCount());

    float z = 0.5f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // 通过
    m_manager.depthTestAndWrite(0, 0, 0, z, color);
    EXPECT_EQ(1u, m_manager.getDepthTestCount());
    EXPECT_EQ(0u, m_manager.getDepthRejectCount());

    // 拒绝
    m_manager.depthTestAndWrite(0, 0, 0, z + 0.1f, color);
    EXPECT_EQ(2u, m_manager.getDepthTestCount());
    EXPECT_EQ(1u, m_manager.getDepthRejectCount());
}

TEST_F(TileBufferTest, ResetStats_ClearsAllCounters) {
    m_manager.initTile(0);

    float z = 0.5f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // 执行一些操作
    m_manager.depthTestAndWrite(0, 0, 0, z, color);
    m_manager.depthTestAndWrite(0, 1, 1, z, color);
    m_manager.depthTestAndWrite(0, 0, 0, z + 0.1f, color);

    EXPECT_EQ(3u, m_manager.getDepthTestCount());
    EXPECT_EQ(1u, m_manager.getDepthRejectCount());

    // 重置
    m_manager.resetStats();

    EXPECT_EQ(0u, m_manager.getDepthTestCount());
    EXPECT_EQ(0u, m_manager.getDepthRejectCount());
    EXPECT_EQ(0u, m_manager.getTileWriteCount());
    EXPECT_EQ(0u, m_manager.getFragmentsShadedCount());
}

// ---------------------------------------------------------------------------
// TileBuffer 结构体测试
// ---------------------------------------------------------------------------

TEST_F(TileBufferTest, TileBuffer_Clear_SetsCorrectValues) {
    TileBuffer buf;

    // 构造函数应该调用 clear()
    for (uint32_t i = 0; i < TILE_SIZE; ++i) {
        EXPECT_EQ(CLEAR_DEPTH, buf.depth[i]);
    }
    for (uint32_t i = 0; i < TILE_SIZE * 4; ++i) {
        EXPECT_EQ(0.0f, buf.color[i]);
    }
}

TEST_F(TileBufferTest, TileBuffer_ManualClear) {
    TileBuffer buf;

    // 修改数据
    buf.depth.fill(0.5f);
    buf.color.fill(1.0f);

    // 调用 clear
    buf.clear();

    // 验证清除
    for (uint32_t i = 0; i < TILE_SIZE; ++i) {
        EXPECT_EQ(CLEAR_DEPTH, buf.depth[i]);
    }
    for (uint32_t i = 0; i < TILE_SIZE * 4; ++i) {
        EXPECT_EQ(0.0f, buf.color[i]);
    }
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}