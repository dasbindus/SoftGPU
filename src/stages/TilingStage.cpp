// ============================================================================
// SoftGPU - TilingStage.cpp
// TBR Tiling Stage - Binning algorithm implementation
// PHASE2 NEW
// ============================================================================

#include "TilingStage.hpp"

#include <chrono>
#include <algorithm>
#include <cmath>

namespace SoftGPU {

TilingStage::TilingStage() {
    resetCounters();
}

void TilingStage::setInput(const std::vector<Triangle>& triangles) {
    m_inputTriangles = triangles;
}

void TilingStage::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    // Clear all bins
    for (auto& bin : m_tileBins) {
        bin.triangleIndices.clear();
    }
    m_tilesAffected = 0;

    uint64_t trianglesBinned = 0;

    // Process each triangle
    for (uint32_t triIdx = 0; triIdx < m_inputTriangles.size(); ++triIdx) {
        const Triangle& tri = m_inputTriangles[triIdx];

        if (tri.culled) continue;

        // Compute tile bounding box for this triangle
        int32_t minTileX, maxTileX, minTileY, maxTileY;
        computeBbox(tri, minTileX, maxTileX, minTileY, maxTileY);

        // Clamp to valid range
        if (minTileX < 0) minTileX = 0;
        if (minTileY < 0) minTileY = 0;
        if (maxTileX >= static_cast<int32_t>(NUM_TILES_X)) maxTileX = NUM_TILES_X - 1;
        if (maxTileY >= static_cast<int32_t>(NUM_TILES_Y)) maxTileY = NUM_TILES_Y - 1;

        // Skip if invalid bbox (degenerate triangle)
        if (minTileX > maxTileX || minTileY > maxTileY) continue;

        // Add this triangle to all covered tiles
        bool addedToAnyTile = false;
        for (int32_t ty = minTileY; ty <= maxTileY; ++ty) {
            for (int32_t tx = minTileX; tx <= maxTileX; ++tx) {
                uint32_t tileIndex = static_cast<uint32_t>(ty) * NUM_TILES_X + static_cast<uint32_t>(tx);
                m_tileBins[tileIndex].triangleIndices.push_back(triIdx);
                addedToAnyTile = true;
            }
        }

        if (addedToAnyTile) {
            trianglesBinned++;
        }
    }

    // Count affected tiles (tiles with at least one triangle)
    for (const auto& bin : m_tileBins) {
        if (!bin.triangleIndices.empty()) {
            m_tilesAffected++;
        }
    }

    m_counters.invocation_count = static_cast<uint64_t>(m_inputTriangles.size());
    m_counters.extra_count0 = trianglesBinned;    // triangles_binned
    m_counters.extra_count1 = m_tilesAffected;    // tiles_affected

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void TilingStage::resetCounters() {
    m_counters = PerformanceCounters{};
    m_tilesAffected = 0;
}

const TileBin& TilingStage::getTileBin(uint32_t tileIndex) const {
    return m_tileBins[tileIndex];
}

void TilingStage::computeBbox(const Triangle& tri,
                               int32_t& minTileX, int32_t& maxTileX,
                               int32_t& minTileY, int32_t& maxTileY) const {
    // Use already-computed NDC coordinates (from PrimitiveAssembly perspective divide)
    // NDC: x,y,z in [-1, 1] range after perspective divide
    float minX = std::min({tri.v[0].ndcX, tri.v[1].ndcX, tri.v[2].ndcX});
    float maxX = std::max({tri.v[0].ndcX, tri.v[1].ndcX, tri.v[2].ndcX});
    float minY = std::min({tri.v[0].ndcY, tri.v[1].ndcY, tri.v[2].ndcY});
    float maxY = std::max({tri.v[0].ndcY, tri.v[1].ndcY, tri.v[2].ndcY});

    // Clamp to [-1, 1] range (triangle may extend beyond viewport in NDC)
    minX = std::max(-1.0f, minX);
    maxX = std::min(1.0f, maxX);
    minY = std::max(-1.0f, minY);
    maxY = std::min(1.0f, maxY);

    // Handle degenerate triangle
    if (minX > maxX || minY > maxY) {
        minTileX = 0; maxTileX = -1;  // Invalid bbox
        minTileY = 0; maxTileY = -1;
        return;
    }

    // NDC to screen coordinates
    // Y-axis: NDC Y=-1 (bottom) -> screenY=0, NDC Y=+1 (top) -> screenY=H
    // When Y is NOT inverted: screenY = (ndcY + 1) * 0.5 * H
    // But we use: screenY = (1 - ndcY) * 0.5 * H (Y inverted)
    // So: NDC maxY (top) -> screenMinY (top, small), NDC minY (bottom) -> screenMaxY (bottom, large)
    float screenMinX = (minX + 1.0f) * 0.5f * static_cast<float>(FRAMEBUFFER_WIDTH);
    float screenMaxX = (maxX + 1.0f) * 0.5f * static_cast<float>(FRAMEBUFFER_WIDTH);
    float screenMinY = (1.0f - maxY) * 0.5f * static_cast<float>(FRAMEBUFFER_HEIGHT);  // NDC maxY -> screen top
    float screenMaxY = (1.0f - minY) * 0.5f * static_cast<float>(FRAMEBUFFER_HEIGHT);  // NDC minY -> screen bottom

    // NDC to tile grid
    minTileX = static_cast<int32_t>(std::floor(screenMinX / static_cast<float>(TILE_WIDTH)));
    maxTileX = static_cast<int32_t>(std::floor(screenMaxX / static_cast<float>(TILE_WIDTH)));
    minTileY = static_cast<int32_t>(std::floor(screenMinY / static_cast<float>(TILE_HEIGHT)));
    maxTileY = static_cast<int32_t>(std::floor(screenMaxY / static_cast<float>(TILE_HEIGHT)));
}

bool TilingStage::ndcToTile(float ndcX, float ndcY,
                            int32_t& tileX, int32_t& tileY) const {
    // NDC to screen (Y-axis flip)
    float screenX = (ndcX + 1.0f) * 0.5f * static_cast<float>(FRAMEBUFFER_WIDTH);
    float screenY = (1.0f - ndcY) * 0.5f * static_cast<float>(FRAMEBUFFER_HEIGHT);

    // Screen to tile
    tileX = static_cast<int32_t>(std::floor(screenX / static_cast<float>(TILE_WIDTH)));
    tileY = static_cast<int32_t>(std::floor(screenY / static_cast<float>(TILE_HEIGHT)));

    // Check bounds
    return (tileX >= 0 && tileX < static_cast<int32_t>(NUM_TILES_X) &&
            tileY >= 0 && tileY < static_cast<int32_t>(NUM_TILES_Y));
}

}  // namespace SoftGPU
