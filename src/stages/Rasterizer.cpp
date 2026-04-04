// ============================================================================
// SoftGPU - Rasterizer.cpp
// 光栅化器
// PHASE2: Added per-tile rasterization via setTrianglesForTile() + executePerTile()
// ============================================================================

#include "Rasterizer.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>

namespace SoftGPU {

Rasterizer::Rasterizer() {
    resetCounters();
}

void Rasterizer::setViewport(uint32_t width, uint32_t height) {
    m_viewportWidth  = width;
    m_viewportHeight = height;
}

void Rasterizer::setInput(const std::vector<Triangle>& triangles) {
    m_inputTriangles = triangles;
    m_inputVersion = 2;
    m_perTileMode = false;
}

void Rasterizer::setInputFromConnect(const std::vector<Triangle>& triangles) {
    m_inputTrianglesPtr = &triangles;
    m_inputTriangles = triangles;
    m_inputVersion = 1;
    m_perTileMode = false;
}

void Rasterizer::setTrianglesForTile(const std::vector<Triangle>& triangles,
                                      uint32_t tileX, uint32_t tileY) {
    m_inputTrianglesPerTile = triangles;
    m_tileX = tileX;
    m_tileY = tileY;
    m_perTileMode = true;
}

void Rasterizer::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    m_outputFragments.clear();

    // Use pointer from connectStages only if setInput has not been called after connect
    const std::vector<Triangle>& input = (m_inputVersion == 1 && m_inputTrianglesPtr != nullptr)
        ? *m_inputTrianglesPtr
        : m_inputTriangles;

    for (const auto& tri : input) {
        if (!tri.culled) {
            rasterizeTrianglePerTile(tri, 0, 0);
        }
    }

    m_counters.invocation_count = input.size();
    m_counters.extra_count1 = static_cast<uint64_t>(m_outputFragments.size());  // fragment_count

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void Rasterizer::executePerTile() {
    auto start = std::chrono::high_resolution_clock::now();

    m_outputFragments.clear();

    for (const auto& tri : m_inputTrianglesPerTile) {
        if (!tri.culled) {
            rasterizeTrianglePerTile(tri, m_tileX, m_tileY);
        }
    }

    m_counters.invocation_count = static_cast<uint64_t>(m_inputTrianglesPerTile.size());
    m_counters.extra_count1 = static_cast<uint64_t>(m_outputFragments.size());  // tile_fragments

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void Rasterizer::resetCounters() {
    m_counters = PerformanceCounters{};
}

// Private: rasterize triangle (delegates to per-tile version with tile 0,0)
void Rasterizer::rasterizeTriangle(const Triangle& tri) {
    rasterizeTrianglePerTile(tri, 0, 0);
}

// Private: core rasterization logic for per-tile mode
void Rasterizer::rasterizeTrianglePerTile(const Triangle& tri,
                                           uint32_t tileX, uint32_t tileY) {
    // Convert NDC to screen coordinates
    // Y-axis: NDC Y=-1 (bottom) -> screenY=0, NDC Y=+1 (top) -> screenY=H
    float sx0 = (tri.v[0].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy0 = (1.0f - tri.v[0].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);
    float sx1 = (tri.v[1].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy1 = (1.0f - tri.v[1].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);
    float sx2 = (tri.v[2].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy2 = (1.0f - tri.v[2].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);

    // Compute bounding box
    int xmin = static_cast<int>(std::floor(std::min({sx0, sx1, sx2})));
    int xmax = static_cast<int>(std::ceil(std::max({sx0, sx1, sx2})));
    int ymin = static_cast<int>(std::floor(std::min({sy0, sy1, sy2})));
    int ymax = static_cast<int>(std::ceil(std::max({sy0, sy1, sy2})));

    // Compute tile's screen region
    int tileScreenX = static_cast<int>(tileX) * TILE_WIDTH;
    int tileScreenY = static_cast<int>(tileY) * TILE_HEIGHT;
    int tileScreenX2 = tileScreenX + TILE_WIDTH;
    int tileScreenY2 = tileScreenY + TILE_HEIGHT;

    // Clamp bounding box to tile region
    // PHASE1 mode (tile 0,0): use full viewport clamping (original behavior)
    // PHASE2 mode (other tiles): use tile region clamping
    if (tileX == 0 && tileY == 0) {
        // PHASE1: clamp to viewport
        xmin = std::max(0, std::min(xmin, static_cast<int>(m_viewportWidth) - 1));
        xmax = std::max(0, std::min(xmax, static_cast<int>(m_viewportWidth) - 1));
        ymin = std::max(0, std::min(ymin, static_cast<int>(m_viewportHeight) - 1));
        ymax = std::max(0, std::min(ymax, static_cast<int>(m_viewportHeight) - 1));
    } else {
        // PHASE2: clamp to tile region
        xmin = std::max(tileScreenX, std::min(xmin, tileScreenX2 - 1));
        xmax = std::max(tileScreenX, std::min(xmax, tileScreenX2 - 1));
        ymin = std::max(tileScreenY, std::min(ymin, tileScreenY2 - 1));
        ymax = std::max(tileScreenY, std::min(ymax, tileScreenY2 - 1));
    }

    // Skip if bbox is outside tile or invalid
    if (xmin > xmax || ymin > ymax) return;

    // Compute signed area (positive = CCW)
    float area = edgeFunction(sx0, sy0, sx1, sy1, sx2, sy2);
    if (std::abs(area) < 1e-8f) return;  // Degenerate triangle

    // Scan all pixels in bounding box
    for (int py = ymin; py <= ymax; ++py) {
        for (int px = xmin; px <= xmax; ++px) {
            float pxF = static_cast<float>(px) + 0.5f;
            float pyF = static_cast<float>(py) + 0.5f;

            // Edge function tests (CCW winding, inside = all >= 0 or all <= 0)
            // Use epsilon for symmetric boundary handling to avoid precision issues
            constexpr float EPSILON = 1e-6f;
            float e0 = edgeFunction(pxF, pyF, sx0, sy0, sx1, sy1);
            float e1 = edgeFunction(pxF, pyF, sx1, sy1, sx2, sy2);
            float e2 = edgeFunction(pxF, pyF, sx2, sy2, sx0, sy0);

            // Fill rule: pixel is inside if all edges have the same sign (all >= 0 or all <= 0).
            // Use epsilon for numerical stability only — not as a third "inside" condition.
            // Clear interior: all edges positive (> EPS) or all negative (< -EPS)
            // Boundary: fall through to epsilon tolerance check
            constexpr float EPS = 1e-6f;
            bool inside = (e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                          (e0 <= 0 && e1 <= 0 && e2 <= 0);
            // Epsilon tolerance: pixels very close to the edge (all |e| <= EPS) are also inside
            if (!inside) {
                inside = (std::abs(e0) <= EPS && std::abs(e1) <= EPS && std::abs(e2) <= EPS);
            }

            if (inside) {
                // Create fragment
                Fragment frag;
                // Screen coordinates
                frag.x = static_cast<uint32_t>(px);
                frag.y = static_cast<uint32_t>(py);

                // Interpolate attributes using barycentric coordinates
                interpolateAttributes(tri, pxF, pyF, area, frag);

                m_outputFragments.push_back(frag);
            }
        }
    }
}

float Rasterizer::edgeFunction(float px, float py,
                               float ax, float ay,
                               float bx, float by) const {
    // Cross product (P - A) x (B - A)
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void Rasterizer::interpolateAttributes(const Triangle& tri, float baryX, float baryY,
                                       float area, Fragment& frag) const {
    // Barycentric weights from edge functions
    // w0 = E(P, V1, V2) / area, etc.
    // Y-axis: NDC Y=-1 (bottom) -> screenY=0, NDC Y=+1 (top) -> screenY=H
    float sx0 = (tri.v[0].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy0 = (1.0f - tri.v[0].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);
    float sx1 = (tri.v[1].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy1 = (1.0f - tri.v[1].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);
    float sx2 = (tri.v[2].ndcX + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth);
    float sy2 = (1.0f - tri.v[2].ndcY) * 0.5f * static_cast<float>(m_viewportHeight);

    float w0 = edgeFunction(baryX, baryY, sx1, sy1, sx2, sy2) / area;
    float w1 = edgeFunction(baryX, baryY, sx2, sy2, sx0, sy0) / area;
    float w2 = edgeFunction(baryX, baryY, sx0, sy0, sx1, sy1) / area;

    // Interpolate depth (NDC Z)
    frag.z = w0 * tri.v[0].ndcZ + w1 * tri.v[1].ndcZ + w2 * tri.v[2].ndcZ;

    // Interpolate color
    frag.r = w0 * tri.v[0].r + w1 * tri.v[1].r + w2 * tri.v[2].r;
    frag.g = w0 * tri.v[0].g + w1 * tri.v[1].g + w2 * tri.v[2].g;
    frag.b = w0 * tri.v[0].b + w1 * tri.v[1].b + w2 * tri.v[2].b;
    frag.a = w0 * tri.v[0].a + w1 * tri.v[1].a + w2 * tri.v[2].a;

    // Clamp color to [0,1] range
    frag.r = std::max(0.0f, std::min(1.0f, frag.r));
    frag.g = std::max(0.0f, std::min(1.0f, frag.g));
    frag.b = std::max(0.0f, std::min(1.0f, frag.b));

    // Texture coordinates (unused in Phase1)
    frag.u = 0.0f;
    frag.v = 0.0f;
}

}  // namespace SoftGPU
