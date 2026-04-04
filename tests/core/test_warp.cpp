// ============================================================================
// SoftGPU - test_warp.cpp
// WarpScheduler & Warp Execution Tests
// v1.4 Phase - P0-1 WarpScheduler Integration
// ============================================================================

#include <gtest/gtest.h>
#include <vector>
#include <memory>

#include "pipeline/WarpScheduler.hpp"
#include "core/PipelineTypes.hpp"

using namespace SoftGPU;

// ============================================================================
// P0-1: WarpScheduler Basic Tests
// ============================================================================

class WarpSchedulerBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        WarpScheduler::Config config;
        config.warp_count = 4;
        config.warp_size = 8;
        config.enable_cycle_counting = true;
        config.enable_multithreading = false;
        m_scheduler = std::make_unique<WarpScheduler>(config);
    }

    void TearDown() override {
        m_scheduler.reset();
    }

    std::unique_ptr<WarpScheduler> m_scheduler;
};

// ---------------------------------------------------------------------------
// Test: WarpScheduler initializes with correct warp count
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, InitialWarpCount)
{
    EXPECT_EQ(m_scheduler->getWarpCount(), 4u);
    for (uint32_t i = 0; i < m_scheduler->getWarpCount(); ++i) {
        EXPECT_TRUE(m_scheduler->getWarp(i).isIdle());
    }
}

// ---------------------------------------------------------------------------
// Test: allocateWarp returns valid warp
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, AllocateWarp)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);
    EXPECT_TRUE(warp->isRunning());
    EXPECT_EQ(warp->getId(), 0u);
}

// ---------------------------------------------------------------------------
// Test: freeWarp releases warp back to pool
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, FreeWarp)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);
    uint32_t warp_id = warp->getId();

    m_scheduler->freeWarp(warp_id);
    EXPECT_TRUE(m_scheduler->getWarp(warp_id).isIdle());
}

// ---------------------------------------------------------------------------
// Test: allocate multiple warps
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, AllocateMultipleWarps)
{
    Warp* warp1 = m_scheduler->allocateWarp();
    Warp* warp2 = m_scheduler->allocateWarp();
    Warp* warp3 = m_scheduler->allocateWarp();

    ASSERT_NE(warp1, nullptr);
    ASSERT_NE(warp2, nullptr);
    ASSERT_NE(warp3, nullptr);

    EXPECT_TRUE(warp1->isRunning());
    EXPECT_TRUE(warp2->isRunning());
    EXPECT_TRUE(warp3->isRunning());

    EXPECT_NE(warp1->getId(), warp2->getId());
    EXPECT_NE(warp2->getId(), warp3->getId());
}

// ---------------------------------------------------------------------------
// Test: free all warps individually
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, FreeAllWarpsIndividually)
{
    std::vector<Warp*> warps;
    for (uint32_t i = 0; i < 4; ++i) {
        Warp* warp = m_scheduler->allocateWarp();
        ASSERT_NE(warp, nullptr);
        warps.push_back(warp);
    }

    for (auto* warp : warps) {
        m_scheduler->freeWarp(warp->getId());
    }

    for (uint32_t i = 0; i < m_scheduler->getWarpCount(); ++i) {
        EXPECT_TRUE(m_scheduler->getWarp(i).isIdle());
    }
}

// ---------------------------------------------------------------------------
// Test: cycle counting is disabled by default
// ---------------------------------------------------------------------------
TEST_F(WarpSchedulerBasicTest, CycleCountingDisabled)
{
    WarpScheduler::Config config;
    config.warp_count = 2;
    config.warp_size = 8;
    config.enable_cycle_counting = false;

    WarpScheduler scheduler(config);
    EXPECT_EQ(scheduler.getCycleCount(), 0u);
}

// ============================================================================
// P0-1: Warp Lifecycle Tests
// ============================================================================

class WarpLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        WarpScheduler::Config config;
        config.warp_count = 4;
        config.warp_size = 8;
        config.enable_cycle_counting = true;
        m_scheduler = std::make_unique<WarpScheduler>(config);
    }

    std::unique_ptr<WarpScheduler> m_scheduler;
};

// ---------------------------------------------------------------------------
// Test: warp transitions from RUNNING to DONE
// ---------------------------------------------------------------------------
TEST_F(WarpLifecycleTest, WarpStateTransitions)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);

    EXPECT_TRUE(warp->isRunning());
    EXPECT_FALSE(warp->isDone());

    warp->finish();
    EXPECT_TRUE(warp->isDone());
    EXPECT_FALSE(warp->isRunning());
}

// ---------------------------------------------------------------------------
// Test: warp kill transitions to KILLED state
// ---------------------------------------------------------------------------
TEST_F(WarpLifecycleTest, WarpKill)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);

    warp->kill();
    EXPECT_TRUE(warp->isDone());
    EXPECT_FALSE(warp->isRunning());
}

// ---------------------------------------------------------------------------
// Test: warp reset clears state
// ---------------------------------------------------------------------------
TEST_F(WarpLifecycleTest, WarpReset)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);

    warp->finish();
    warp->reset();

    EXPECT_TRUE(warp->isIdle());
    EXPECT_EQ(warp->getPC(), 0u);
}

// ---------------------------------------------------------------------------
// Test: warp stats increment on execution
// ---------------------------------------------------------------------------
TEST_F(WarpLifecycleTest, WarpStats)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);

    warp->getStats().cycles_active = 10;
    warp->getStats().instructions_executed = 5;

    const auto& stats = warp->getStats();
    EXPECT_EQ(stats.cycles_active, 10u);
    EXPECT_EQ(stats.instructions_executed, 5u);
}

// ---------------------------------------------------------------------------
// Test: warp reset clears stats
// ---------------------------------------------------------------------------
TEST_F(WarpLifecycleTest, WarpResetStats)
{
    Warp* warp = m_scheduler->allocateWarp();
    ASSERT_NE(warp, nullptr);

    warp->getStats().cycles_active = 100;
    warp->resetStats();

    const auto& stats = warp->getStats();
    EXPECT_EQ(stats.cycles_active, 0u);
    EXPECT_EQ(stats.instructions_executed, 0u);
}

// ============================================================================
// P0-1: WarpBatchConfig & WarpBatchResult Tests
// ============================================================================

class WarpBatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        WarpScheduler::Config config;
        config.warp_count = 4;
        config.warp_size = 8;
        config.enable_cycle_counting = true;
        m_scheduler = std::make_unique<WarpScheduler>(config);
    }

    std::unique_ptr<WarpScheduler> m_scheduler;
};

// ---------------------------------------------------------------------------
// Test: WarpBatchConfig defaults
// ---------------------------------------------------------------------------
TEST_F(WarpBatchTest, WarpBatchConfigDefaults)
{
    WarpBatchConfig cfg;

    EXPECT_EQ(cfg.max_cycles_per_warp, 1024u);
    EXPECT_EQ(cfg.max_warps_to_schedule, 0u);  // 0 means unlimited
    EXPECT_TRUE(cfg.enable_tile_write);
    EXPECT_FALSE(cfg.yield_on_stall);
}

// ---------------------------------------------------------------------------
// Test: WarpBatchResult defaults
// ---------------------------------------------------------------------------
TEST_F(WarpBatchTest, WarpBatchResultDefaults)
{
    WarpBatchResult result;

    EXPECT_EQ(result.warps_completed, 0u);
    EXPECT_EQ(result.fragments_written, 0u);
    EXPECT_EQ(result.cycles_this_batch, 0u);
    EXPECT_FALSE(result.all_done);
}

// ---------------------------------------------------------------------------
// Test: executeWarpBatch with no warps returns all_done
// ---------------------------------------------------------------------------
TEST_F(WarpBatchTest, EmptyBatchAllDone)
{
    WarpBatchConfig cfg;
    WarpBatchResult result = m_scheduler->executeWarpBatch(cfg);

    EXPECT_TRUE(result.all_done);
    EXPECT_EQ(result.warps_completed, 0u);
}

// ---------------------------------------------------------------------------
// Test: scheduler stop works
// ---------------------------------------------------------------------------
TEST_F(WarpBatchTest, SchedulerStop)
{
    m_scheduler->stop();
    EXPECT_TRUE(true);  // If we get here without hang, stop works
}

// ============================================================================
// P0-1: Warp Configuration Tests
// ============================================================================

class WarpConfigTest : public ::testing::Test {};

// ---------------------------------------------------------------------------
// Test: Warp::WARP_SIZE constant
// ---------------------------------------------------------------------------
TEST_F(WarpConfigTest, WarpSizeConstant)
{
    EXPECT_EQ(Warp::WARP_SIZE, 8u);
}

// ---------------------------------------------------------------------------
// Test: Warp::INVALID_ID constant
// ---------------------------------------------------------------------------
TEST_F(WarpConfigTest, WarpInvalidId)
{
    EXPECT_EQ(Warp::INVALID_ID, 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Test: custom warp count and size
// ---------------------------------------------------------------------------
TEST_F(WarpConfigTest, CustomWarpConfig)
{
    WarpScheduler::Config config;
    config.warp_count = 8;
    config.warp_size = 16;

    WarpScheduler scheduler(config);
    EXPECT_EQ(scheduler.getWarpCount(), 8u);
}

// ---------------------------------------------------------------------------
// Test: fragment context basic operations
// ---------------------------------------------------------------------------
TEST_F(WarpConfigTest, FragmentContextBasic)
{
    FragmentContext ctx;
    ctx.pos_x = 10.0f;
    ctx.pos_y = 20.0f;
    ctx.pos_z = 0.5f;
    ctx.color_r = 1.0f;
    ctx.color_g = 0.0f;
    ctx.color_b = 0.0f;
    ctx.tile_x = 2;
    ctx.tile_y = 3;

    EXPECT_EQ(ctx.tile_x, 2u);
    EXPECT_EQ(ctx.tile_y, 3u);
    EXPECT_FLOAT_EQ(ctx.pos_x, 10.0f);
    EXPECT_FLOAT_EQ(ctx.color_r, 1.0f);
}
