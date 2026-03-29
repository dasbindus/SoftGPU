// ============================================================================
// SoftGPU - TileWriteBack.hpp
// TBR Tile WriteBack - GMEM ↔ LMEM synchronization
// PHASE2: Refactored - per-tile operations moved to RenderPipeline
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "stages/TileBuffer.hpp"
#include "stages/Framebuffer.hpp"
#include <vector>
#include <cstdint>

namespace SoftGPU {

// Forward declaration
class MemorySubsystem;
class TileWriteBack : public IStage {
public:
    static constexpr uint32_t TILE_W   = TILE_WIDTH;
    static constexpr uint32_t TILE_H   = TILE_HEIGHT;
    static constexpr uint32_t NUM_TILES_X = ::SoftGPU::NUM_TILES_X;
    static constexpr uint32_t NUM_TILES_Y = ::SoftGPU::NUM_TILES_Y;

    TileWriteBack();

    // 注入 framebuffer 引用（PHASE1 兼容）
    void setFramebuffer(Framebuffer* fb);

    // IStage 实现（execute 在 PHASE2 被 RenderPipeline 的 per-tile loop 替代）
    const char* getName() const override { return "TileWriteBack"; }
    void execute() override;  // Legacy: writes all tiles (for PHASE1 backward compat)
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // ========================================================================
    // PHASE2 GMEM ↔ LMEM 同步接口
    // ========================================================================

    // 从 GMEM 加载 tile 数据（由 RenderPipeline per-tile loop 调用）
    // 带宽消耗通过 memory 参数记录
    void loadTileFromGMEM(uint32_t tileIndex,
                          MemorySubsystem* memory,
                          TileBuffer& outTileBuffer);

    // 将 tile 数据存储到 GMEM（由 RenderPipeline per-tile loop 调用）
    // 带宽消耗通过 memory 参数记录
    void storeTileToGMEM(uint32_t tileIndex,
                         MemorySubsystem* memory,
                         const TileBuffer& tileBuffer);

    // 批量加载所有 tile 到传入的 TileBufferManager（PHASE1 兼容）
    void loadAllTilesToBuffer(TileBufferManager& manager);

    // 批量存储所有 tile 从传入的 TileBufferManager（PHASE1 兼容）
    // memory: MemorySubsystem 指针，用于记录带宽（可为 nullptr 用于 PHASE1 兼容）
    void storeAllTilesFromBuffer(const TileBufferManager& manager, MemorySubsystem* memory = nullptr);

    // ========================================================================
    // GMEM 数据访问（用于 Present / 导出 / 测试）
    // ========================================================================
    const float* getGMEMColor() const { return m_gmemColor.data(); }
    const float* getGMEMDepth() const { return m_gmemDepth.data(); }
    float* getGMEMColorData() { return m_gmemColor.data(); }
    float* getGMEMDepthData() { return m_gmemDepth.data(); }

    // 获取 GMEM tile 偏移量（供 MemorySubsystem 使用）
    size_t getTileColorOffset(uint32_t tileIndex) const;
    size_t getTileDepthOffset(uint32_t tileIndex) const;

    // 获取 GMEM 总大小
    static constexpr size_t getGMEMColorSize() { return GMEM_COLOR_SIZE; }
    static constexpr size_t getGMEMDepthSize() { return GMEM_DEPTH_SIZE; }

private:
    Framebuffer* m_framebuffer = nullptr;
    PerformanceCounters m_counters;

    // 模拟 GMEM（CPU 端）
    // 每个 tile 连续存储: NUM_TILES_X * NUM_TILES_Y * TILE_W * TILE_H * 4 floats
    static constexpr size_t GMEM_COLOR_SIZE =
        NUM_TILES_X * NUM_TILES_Y * TILE_W * TILE_H * 4;
    static constexpr size_t GMEM_DEPTH_SIZE =
        NUM_TILES_X * NUM_TILES_Y * TILE_W * TILE_H;

    std::vector<float> m_gmemColor;  // RGBA
    std::vector<float> m_gmemDepth;  // Depth

    // 内部：计算 tile 在 GMEM 中的偏移
    size_t getTileOffset(uint32_t tileIndex) const;
};

}  // namespace SoftGPU
