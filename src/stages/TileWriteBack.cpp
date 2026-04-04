// ============================================================================
// SoftGPU - TileWriteBack.cpp
// TBR Tile WriteBack - GMEM ↔ LMEM synchronization
// PHASE2: Refactored - per-tile operations moved to RenderPipeline
// ============================================================================

#include "TileWriteBack.hpp"

#include "TileBuffer.hpp"
#include "core/MemorySubsystem.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace SoftGPU {

TileWriteBack::TileWriteBack() : m_framebuffer(nullptr) {
    m_gmemColor.resize(GMEM_COLOR_SIZE, 0.0f);
    m_gmemDepth.resize(GMEM_DEPTH_SIZE, CLEAR_DEPTH);
    resetCounters();
}

void TileWriteBack::setFramebuffer(Framebuffer* fb) {
    m_framebuffer = fb;
}

void TileWriteBack::resetCounters() {
    m_counters = PerformanceCounters{};
}

void TileWriteBack::execute() {
    // Legacy PHASE1 behavior: write all tiles to GMEM
    // In PHASE2, this is called only when running in PHASE1 backward-compat mode
    auto start = std::chrono::high_resolution_clock::now();

    uint64_t tilesWritten = 0;

    for (uint32_t ty = 0; ty < NUM_TILES_Y; ++ty) {
        for (uint32_t tx = 0; tx < NUM_TILES_X; ++tx) {
            uint32_t tileIndex = ty * NUM_TILES_X + tx;
            size_t tileOffset = getTileOffset(tileIndex);

            // For PHASE1 compat: we don't have TileBuffer here,
            // so we just mark the tiles as "written" (no actual data)
            tilesWritten++;
            (void)tileOffset;  // suppress unused warning
        }
    }

    m_counters.invocation_count = tilesWritten;

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

size_t TileWriteBack::getTileOffset(uint32_t tileIndex) const {
    // Tile index to linear offset (in pixels, not floats)
    return static_cast<size_t>(tileIndex) * TILE_SIZE;
}

size_t TileWriteBack::getTileColorOffset(uint32_t tileIndex) const {
    // Float offset for RGBA color: tileOffset * 4 floats
    return getTileOffset(tileIndex) * 4;
}

size_t TileWriteBack::getTileDepthOffset(uint32_t tileIndex) const {
    // Float offset for depth: tileOffset * 1 float
    return getTileOffset(tileIndex);
}

void TileWriteBack::loadTileFromGMEM(uint32_t tileIndex,
                                     MemorySubsystem* memory,
                                     TileBuffer& outTileBuffer) {
    size_t colorOffset = getTileColorOffset(tileIndex);
    size_t depthOffset = getTileDepthOffset(tileIndex);

    // Record bandwidth for color load: TILE_SIZE * 4 floats
    // Only perform memcpy if bandwidth allows
    bool colorAllowed = true;
    if (memory) {
        colorAllowed = memory->addAccess(TILE_SIZE * 4 * sizeof(float), MemoryAccessType::LoadTile);
    }

    // Copy color data only if bandwidth allowed
    if (colorAllowed) {
        std::memcpy(outTileBuffer.color.data(),
                    m_gmemColor.data() + colorOffset,
                    TILE_SIZE * 4 * sizeof(float));
    }

    // Record bandwidth for depth load: TILE_SIZE * 1 float
    // Only perform memcpy if bandwidth allows
    bool depthAllowed = true;
    if (memory) {
        depthAllowed = memory->addAccess(TILE_SIZE * sizeof(float), MemoryAccessType::LoadTile);
    }

    // Copy depth data only if bandwidth allowed
    if (depthAllowed) {
        std::memcpy(outTileBuffer.depth.data(),
                    m_gmemDepth.data() + depthOffset,
                    TILE_SIZE * sizeof(float));
    }
}

void TileWriteBack::storeTileToGMEM(uint32_t tileIndex,
                                    MemorySubsystem* memory,
                                    const TileBuffer& tileBuffer) {
    size_t colorOffset = getTileColorOffset(tileIndex);
    size_t depthOffset = getTileDepthOffset(tileIndex);

    // Record bandwidth for color store: TILE_SIZE * 4 floats
    // Only perform memcpy if bandwidth allows
    bool colorAllowed = true;
    if (memory) {
        colorAllowed = memory->addAccess(TILE_SIZE * 4 * sizeof(float), MemoryAccessType::StoreTile);
    }

    // Copy color data only if bandwidth allowed
    if (colorAllowed) {
        std::memcpy(m_gmemColor.data() + colorOffset,
                    tileBuffer.color.data(),
                    TILE_SIZE * 4 * sizeof(float));
    }

    // Record bandwidth for depth store: TILE_SIZE * 1 float
    // Only perform memcpy if bandwidth allows
    bool depthAllowed = true;
    if (memory) {
        depthAllowed = memory->addAccess(TILE_SIZE * sizeof(float), MemoryAccessType::StoreTile);
    }

    // Copy depth data only if bandwidth allowed
    if (depthAllowed) {
        std::memcpy(m_gmemDepth.data() + depthOffset,
                    tileBuffer.depth.data(),
                    TILE_SIZE * sizeof(float));
    }
}

void TileWriteBack::loadAllTilesToBuffer(TileBufferManager& manager) {
    for (uint32_t tileIndex = 0; tileIndex < NUM_TILES; ++tileIndex) {
        auto& tile = manager.getTileBuffer(tileIndex);
        size_t colorOffset = getTileColorOffset(tileIndex);
        size_t depthOffset = getTileDepthOffset(tileIndex);

        // Load color using memcpy
        std::memcpy(tile.color.data(), m_gmemColor.data() + colorOffset, TILE_SIZE * 4 * sizeof(float));

        // Load depth using memcpy
        std::memcpy(tile.depth.data(), m_gmemDepth.data() + depthOffset, TILE_SIZE * sizeof(float));
    }
}

void TileWriteBack::storeAllTilesFromBuffer(const TileBufferManager& manager, MemorySubsystem* memory) {
    for (uint32_t tileIndex = 0; tileIndex < NUM_TILES; ++tileIndex) {
        const auto& tile = manager.getTileBuffer(tileIndex);
        size_t colorOffset = getTileColorOffset(tileIndex);
        size_t depthOffset = getTileDepthOffset(tileIndex);

        // P0-3: Check bandwidth atomically for entire tile (color + depth)
        size_t colorBytes = TILE_SIZE * 4 * sizeof(float);
        size_t depthBytes = TILE_SIZE * sizeof(float);
        size_t totalBytes = colorBytes + depthBytes;

        if (memory && !memory->getBucket().tryConsume(totalBytes)) {
            // Skip entire tile (both color and depth) - bandwidth exhausted
        } else {
            // Atomic: both color and depth
            if (memory) memory->recordWrite(totalBytes);
            std::memcpy(m_gmemColor.data() + colorOffset, tile.color.data(), colorBytes);
            std::memcpy(m_gmemDepth.data() + depthOffset, tile.depth.data(), depthBytes);
        }
    }
}

}  // namespace SoftGPU
