// ============================================================================
// SoftGPU - FragmentShader.cpp
// 片段着色器
// PHASE2: Added TileBuffer output support
// ============================================================================

#include "FragmentShader.hpp"
#include "TilingStage.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace SoftGPU {

FragmentShader::FragmentShader() {
    resetCounters();
}

void FragmentShader::setInput(const std::vector<Fragment>& fragments) {
    // For unit tests: always copy (handles temporary initializer lists)
    m_inputFragments = fragments;
    m_inputVersion = 2;  // Mark: setInput called after connectStages
    // Disable TileBuffer mode when using setInput (PHASE1 compat)
    m_tileBuffer = nullptr;
}

void FragmentShader::setInputFromConnect(const std::vector<Fragment>& fragments) {
    // For connectStages: store pointer but don't mark as "setInput after connect"
    m_inputFragmentsPtr = &fragments;
    m_inputFragments = fragments;
    m_inputVersion = 1;  // Mark: set by connectStages
    // Disable TileBuffer mode when using connectStages (PHASE1 compat)
    m_tileBuffer = nullptr;
}

void FragmentShader::setTileBufferManager(TileBufferManager* manager) {
    if (manager == nullptr) {
        fprintf(stderr, "[ERROR] FragmentShader: TileBufferManager cannot be null\n");
        return;
    }
    m_tileBuffer = manager;
}

void FragmentShader::setTileIndex(uint32_t idx) {
    m_tileIndex = idx;
}

void FragmentShader::setInputAndExecuteTile(const std::vector<Fragment>& fragments,
                                            uint32_t tileX, uint32_t tileY) {
    // PHASE2 mode: input directly from Rasterizer per-tile
    m_inputFragments = fragments;
    m_tileIndex = tileY * NUM_TILES_X + tileX;

    auto start = std::chrono::high_resolution_clock::now();

    uint64_t fragmentsShaded = 0;

    for (const auto& frag : fragments) {
        Fragment shaded = shade(frag);

        // Compute local coordinates within tile
        uint32_t localX = frag.x - tileX * TILE_WIDTH;
        uint32_t localY = frag.y - tileY * TILE_HEIGHT;

        // Write to TileBuffer (depth test happens inside)
        float color[4] = {shaded.r, shaded.g, shaded.b, shaded.a};
        bool passed = m_tileBuffer->depthTestAndWrite(m_tileIndex, localX, localY, shaded.z, color);

        if (passed) {
            fragmentsShaded++;
        }
    }

    m_counters.invocation_count = static_cast<uint64_t>(fragments.size());
    m_counters.extra_count0 = fragmentsShaded;   // fragments_shade
    m_counters.extra_count1 = m_tileBuffer->getDepthTestCount();  // depth_tests

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void FragmentShader::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    // Use pointer from connectStages only if setInput has not been called after connect
    const std::vector<Fragment>& input = (m_inputVersion == 1 && m_inputFragmentsPtr != nullptr)
        ? *m_inputFragmentsPtr
        : m_inputFragments;

    m_outputFragments.clear();
    m_outputFragments.reserve(input.size());

    for (const auto& frag : input) {
        m_outputFragments.push_back(shade(frag));
    }

    m_counters.invocation_count = static_cast<uint64_t>(m_outputFragments.size());

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void FragmentShader::resetCounters() {
    m_counters = PerformanceCounters{};
}

Fragment FragmentShader::shade(const Fragment& input) const {
    // Phase1: passthrough flat color
    // In future phases, this could apply textures, lighting, etc.
    Fragment out = input;
    // Clamp color to [0, 1]
    out.r = std::max(0.0f, std::min(1.0f, out.r));
    out.g = std::max(0.0f, std::min(1.0f, out.g));
    out.b = std::max(0.0f, std::min(1.0f, out.b));
    out.a = std::max(0.0f, std::min(1.0f, out.a));
    return out;
}

}  // namespace SoftGPU
