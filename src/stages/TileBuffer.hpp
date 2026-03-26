// ============================================================================
// SoftGPU - TileBuffer.hpp
// TBR Tile Buffer - LMEM: 32×32 RGBA float + depth buffer
// PHASE2 NEW
// ============================================================================

#pragma once

#include "core/PipelineTypes.hpp"
#include "TilingStage.hpp"
#include <array>
#include <cstdint>

namespace SoftGPU {

// ============================================================================
// TileBuffer - Per-tile local memory (LMEM)
// 每个 tile 的 local memory 大小：
//   Color: 32×32 × 4 floats = 4096 floats = 16 KB
//   Depth: 32×32 × 1 float  = 1024 floats =  4 KB
//   Total per tile: ~20 KB
// ============================================================================
constexpr uint32_t TILE_BUFFER_COLOR_SIZE = TILE_SIZE * 4;  // RGBA floats
constexpr uint32_t TILE_BUFFER_DEPTH_SIZE = TILE_SIZE;       // Z floats

struct TileBuffer {
    std::array<float, TILE_BUFFER_COLOR_SIZE> color;  // RGBA
    std::array<float, TILE_BUFFER_DEPTH_SIZE> depth;  // Z

    TileBuffer() {
        clear();
    }

    void clear() {
        color.fill(0.0f);
        depth.fill(CLEAR_DEPTH);  // Initialize to far plane
    }
};

// ============================================================================
// TileBufferManager - 管理所有 300 个 tile 的 LMEM
// ============================================================================
class TileBufferManager {
public:
    TileBufferManager();

    // 获取指定 tile 的 buffer 引用
    TileBuffer& getTileBuffer(uint32_t tileIndex);
    TileBuffer& getTileBuffer(uint32_t tileX, uint32_t tileY) {
        return getTileBuffer(tileY * NUM_TILES_X + tileX);
    }
    const TileBuffer& getTileBuffer(uint32_t tileIndex) const;
    const TileBuffer& getTileBuffer(uint32_t tileX, uint32_t tileY) const {
        return getTileBuffer(tileY * NUM_TILES_X + tileX);
    }

    // 初始化某个 tile 的 buffer（clear）
    void initTile(uint32_t tileIndex);
    void initTile(uint32_t tileX, uint32_t tileY) { initTile(tileY * NUM_TILES_X + tileX); }

    // 初始化所有 tile（每帧开始）
    void initAllTiles();

    // Per-fragment 操作
    // Z-test 通过 → 写入 color + depth
    // Z-test 失败 → 丢弃
    bool depthTestAndWrite(uint32_t tileIndex,
                           uint32_t localX, uint32_t localY,
                           float z, const float color[4]);

    // GMEM 同步接口（由 MemorySubsystem 调用）
    // 从 GMEM 加载 tile 数据到 LMEM
    void loadFromGMEM(uint32_t tileIndex, const float* gmemColor, const float* gmemDepth);
    // 将 LMEM 数据存储到 GMEM
    void storeToGMEM(uint32_t tileIndex, float* outColor, float* outDepth) const;

    // 统计
    uint64_t getTileWriteCount() const { return m_tileWriteCount; }
    uint64_t getDepthTestCount() const { return m_depthTestCount; }
    uint64_t getDepthRejectCount() const { return m_depthRejectCount; }
    uint64_t getFragmentsShadedCount() const { return m_fragmentsShadedCount; }

    // 重置统计
    void resetStats();

private:
    std::array<TileBuffer, NUM_TILES> m_tileBuffers;
    uint64_t m_tileWriteCount = 0;
    uint64_t m_depthTestCount = 0;
    uint64_t m_depthRejectCount = 0;
    uint64_t m_fragmentsShadedCount = 0;
};

}  // namespace SoftGPU
