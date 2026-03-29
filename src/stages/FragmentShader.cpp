// ============================================================================
// SoftGPU - FragmentShader.cpp
// 片段着色器
// PHASE2: Added TileBuffer output support
// PHASE4: Integrated ShaderCore ISA interpreter for fragment execution
// ============================================================================

#include "FragmentShader.hpp"
#include "TilingStage.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace SoftGPU {

FragmentShader::FragmentShader() {
    // PHASE4: Initialize ShaderCore with default fragment shader
    m_shaderFunction = ShaderCore::getDefaultFragmentShader();
    m_shaderCore.loadShader(m_shaderFunction);
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
    uint64_t totalInstructions = 0;
    uint64_t totalCycles = 0;

    // Reset ShaderCore stats for this tile
    m_shaderCore.reset();

    // PHASE3: Batch processing - 8 fragments at a time (warp size)
    const size_t WARP_SIZE = 8;
    size_t numFragments = fragments.size();
    size_t batchCount = (numFragments + WARP_SIZE - 1) / WARP_SIZE;
    
    for (size_t batch = 0; batch < batchCount; ++batch) {
        size_t batchStart = batch * WARP_SIZE;
        size_t batchEnd = std::min(batchStart + WARP_SIZE, numFragments);
        size_t batchSize = batchEnd - batchStart;
        
        // Get stats before batch execution
        uint64_t prevCycles = m_shaderCore.getStats().cycles_spent;
        uint64_t prevInstructions = m_shaderCore.getStats().instructions_executed;
        
        // PHASE3: Use warp batch execution when we have 8 fragments
        if (batchSize == WARP_SIZE) {
            // Build warp batch
            std::array<FragmentContext, WARP_SIZE> warpContexts;
            for (size_t i = 0; i < WARP_SIZE; ++i) {
                const Fragment& frag = fragments[batchStart + i];
                warpContexts[i] = fragmentToContext(frag);
                warpContexts[i].tile_x = tileX;
                warpContexts[i].tile_y = tileY;
            }
            
            // Execute warp batch
            m_shaderCore.executeWarpBatch(warpContexts, m_shaderFunction);
            
            // Process results
            for (size_t i = 0; i < WARP_SIZE; ++i) {
                const Fragment& frag = fragments[batchStart + i];
                FragmentContext& ctx = warpContexts[i];
                
                if (ctx.killed) continue;
                
                Fragment shaded = contextToFragment(ctx);
                uint32_t localX = frag.x - tileX * TILE_WIDTH;
                uint32_t localY = frag.y - tileY * TILE_HEIGHT;
                float color[4] = {shaded.r, shaded.g, shaded.b, shaded.a};
                bool passed = m_tileBuffer->depthTestAndWrite(m_tileIndex, localX, localY, shaded.z, color);
                if (passed) fragmentsShaded++;
            }
        } else {
            // Partial batch - process individually
            for (size_t i = batchStart; i < batchEnd; ++i) {
                const auto& frag = fragments[i];
                FragmentContext ctx = fragmentToContext(frag);
                ctx.tile_x = tileX;
                ctx.tile_y = tileY;
                m_shaderCore.executeFragment(ctx, m_shaderFunction);
                
                if (ctx.killed) continue;
                
                Fragment shaded = contextToFragment(ctx);
                uint32_t localX = frag.x - tileX * TILE_WIDTH;
                uint32_t localY = frag.y - tileY * TILE_HEIGHT;
                float color[4] = {shaded.r, shaded.g, shaded.b, shaded.a};
                bool passed = m_tileBuffer->depthTestAndWrite(m_tileIndex, localX, localY, shaded.z, color);
                if (passed) fragmentsShaded++;
            }
        }
        
        // Accumulate delta stats
        totalInstructions += m_shaderCore.getStats().instructions_executed - prevInstructions;
        totalCycles += m_shaderCore.getStats().cycles_spent - prevCycles;
    }

    m_counters.invocation_count = static_cast<uint64_t>(fragments.size());
    m_counters.extra_count0 = fragmentsShaded;   // fragments_shade
    m_counters.extra_count1 = m_tileBuffer->getDepthTestCount();  // depth_tests
    m_counters.cycle_count = totalCycles;

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

    // Reset ShaderCore stats
    m_shaderCore.reset();
    uint64_t totalCycles = 0;

    for (const auto& frag : input) {
        m_outputFragments.push_back(shade(frag));
    }

    m_counters.invocation_count = static_cast<uint64_t>(m_outputFragments.size());
    m_counters.cycle_count = totalCycles;

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void FragmentShader::resetCounters() {
    m_counters = PerformanceCounters{};
    m_shaderCore.reset();
}

FragmentContext FragmentShader::fragmentToContext(const Fragment& frag) const {
    FragmentContext ctx;
    ctx.pos_x = static_cast<float>(frag.x);
    ctx.pos_y = static_cast<float>(frag.y);
    ctx.pos_z = frag.z;
    ctx.color_r = frag.r;
    ctx.color_g = frag.g;
    ctx.color_b = frag.b;
    ctx.color_a = frag.a;
    ctx.u = frag.u;
    ctx.v = frag.v;
    return ctx;
}

Fragment FragmentShader::contextToFragment(const FragmentContext& ctx) const {
    Fragment frag;
    frag.x = static_cast<uint32_t>(ctx.pos_x);
    frag.y = static_cast<uint32_t>(ctx.pos_y);
    frag.z = ctx.out_z;
    frag.r = ctx.out_r;
    frag.g = ctx.out_g;
    frag.b = ctx.out_b;
    frag.a = ctx.out_a;
    return frag;
}

Fragment FragmentShader::shade(const Fragment& input) {
    // PHASE4: Use ShaderCore ISA interpreter for fragment shading
    FragmentContext ctx = fragmentToContext(input);

    // Execute fragment through ShaderCore
    m_shaderCore.executeFragment(ctx, m_shaderFunction);

    // Check if fragment was killed by shader (e.g., discard)
    if (ctx.killed) {
        // Return a special "killed" fragment - depth test will reject it
        Fragment out = input;
        out.z = CLEAR_DEPTH;  // Move to far plane to ensure rejection
        return out;
    }

    // Convert output context back to Fragment
    return contextToFragment(ctx);
}

}  // namespace SoftGPU
