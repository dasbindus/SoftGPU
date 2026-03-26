// ============================================================================
// SoftGPU - TileBuffer.cpp
// TBR Tile Buffer - LMEM implementation
// PHASE2 NEW
// ============================================================================

#include "TileBuffer.hpp"

#include <cstring>

namespace SoftGPU {

TileBufferManager::TileBufferManager() {
}

TileBuffer& TileBufferManager::getTileBuffer(uint32_t tileIndex) {
    return m_tileBuffers[tileIndex];
}

const TileBuffer& TileBufferManager::getTileBuffer(uint32_t tileIndex) const {
    return m_tileBuffers[tileIndex];
}

void TileBufferManager::initTile(uint32_t tileIndex) {
    m_tileBuffers[tileIndex].clear();
}

void TileBufferManager::initAllTiles() {
    for (auto& tb : m_tileBuffers) {
        tb.clear();
    }
}

bool TileBufferManager::depthTestAndWrite(uint32_t tileIndex,
                                          uint32_t localX, uint32_t localY,
                                          float z, const float color[4]) {
    // Validate coordinates
    if (localX >= TILE_WIDTH || localY >= TILE_HEIGHT) {
        return false;
    }

    uint32_t localIndex = localY * TILE_WIDTH + localX;
    TileBuffer& tile = m_tileBuffers[tileIndex];

    m_depthTestCount++;

    // Z-test: smaller Z = closer to camera
    if (z < tile.depth[localIndex]) {
        // Pass: update depth and color
        tile.depth[localIndex] = z;
        tile.color[localIndex * 4 + 0] = color[0];
        tile.color[localIndex * 4 + 1] = color[1];
        tile.color[localIndex * 4 + 2] = color[2];
        tile.color[localIndex * 4 + 3] = color[3];
        return true;
    } else {
        // Fail: discard
        m_depthRejectCount++;
        return false;
    }
}

void TileBufferManager::loadFromGMEM(uint32_t tileIndex,
                                     const float* gmemColor,
                                     const float* gmemDepth) {
    TileBuffer& tile = m_tileBuffers[tileIndex];

    // Load color: TILE_SIZE * 4 floats
    std::memcpy(tile.color.data(), gmemColor, TILE_SIZE * 4 * sizeof(float));

    // Load depth: TILE_SIZE floats
    std::memcpy(tile.depth.data(), gmemDepth, TILE_SIZE * sizeof(float));
}

void TileBufferManager::storeToGMEM(uint32_t tileIndex,
                                    float* outColor,
                                    float* outDepth) const {
    const TileBuffer& tile = m_tileBuffers[tileIndex];

    // Store color: TILE_SIZE * 4 floats
    std::memcpy(outColor, tile.color.data(), TILE_SIZE * 4 * sizeof(float));

    // Store depth: TILE_SIZE floats
    std::memcpy(outDepth, tile.depth.data(), TILE_SIZE * sizeof(float));

    // Note: tile_write_count is incremented by RenderPipeline after storeToGMEM
}

void TileBufferManager::resetStats() {
    m_tileWriteCount = 0;
    m_depthTestCount = 0;
    m_depthRejectCount = 0;
    m_fragmentsShadedCount = 0;
}

}  // namespace SoftGPU
