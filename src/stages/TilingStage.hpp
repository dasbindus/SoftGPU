// ============================================================================
// SoftGPU - TilingStage.hpp
// TBR Tiling Stage - Binning algorithm, per-tile triangle list
// PHASE2 NEW
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>
#include <array>
#include <cstdint>

namespace SoftGPU {

// ============================================================================
// Tile 派生常量（基于 PipelineTypes.hpp 中的 TILE_WIDTH/HEIGHT）
// ============================================================================
constexpr uint32_t TILE_SIZE   = TILE_WIDTH * TILE_HEIGHT;  // 1024
constexpr uint32_t NUM_TILES_X = (FRAMEBUFFER_WIDTH + TILE_WIDTH - 1) / TILE_WIDTH;   // 20
constexpr uint32_t NUM_TILES_Y = (FRAMEBUFFER_HEIGHT + TILE_HEIGHT - 1) / TILE_HEIGHT; // 15
constexpr uint32_t NUM_TILES   = NUM_TILES_X * NUM_TILES_Y;  // 300

// ============================================================================
// TileBin - Per-tile triangle index list
// ============================================================================
struct TileBin {
    std::vector<uint32_t> triangleIndices;  // indices into the original triangle array
};

// ============================================================================
// TilingStage - Binning 算法
// 输入：PrimitiveAssembly 输出的 triangles（NDC space）
// 输出：per-tile triangle list（tileBins[300]）
// ============================================================================
class TilingStage : public IStage {
public:
    TilingStage();

    // 设置输入（来自 PrimitiveAssembly）
    void setInput(const std::vector<Triangle>& triangles);

    // IStage 实现
    const char* getName() const override { return "TilingStage"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 获取某个 tile 的 bin（triangle index list）
    const TileBin& getTileBin(uint32_t tileIndex) const;
    const TileBin& getTileBin(uint32_t tileX, uint32_t tileY) const {
        return getTileBin(tileY * NUM_TILES_X + tileX);
    }

    // 获取受影响的 tile 数量
    uint32_t getNumAffectedTiles() const { return m_tilesAffected; }

    // 获取输入 triangles 引用（用于 Rasterizer per-tile 获取数据）
    const std::vector<Triangle>& getInputTriangles() const { return m_inputTriangles; }

private:
    std::vector<Triangle> m_inputTriangles;
    std::array<TileBin, NUM_TILES> m_tileBins;  // fixed 300 bins
    uint32_t m_tilesAffected = 0;
    PerformanceCounters m_counters;

    // 内部：计算三角形 bounding box（返回覆盖的 tile 范围）
    void computeBbox(const Triangle& tri,
                     int32_t& minTileX, int32_t& maxTileX,
                     int32_t& minTileY, int32_t& maxTileY) const;

    // 内部：NDC → tile grid 坐标
    bool ndcToTile(float ndcX, float ndcY,
                   int32_t& tileX, int32_t& tileY) const;
};

}  // namespace SoftGPU
