// ============================================================================
// SoftGPU - FragmentShader.cpp
// 片段着色器
// PHASE2: ISA 解释器集成
// ============================================================================

#include "FragmentShader.hpp"
#include "TilingStage.hpp"
#include "pipeline/ShaderCore.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace SoftGPU {

FragmentShader::FragmentShader() {
    // PHASE2: 初始化 ShaderCore，加载默认 fragment shader
    m_currentShader = ShaderCore::getDefaultFragmentShader();
    m_shaderCore.loadShader(m_currentShader);
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
    // PHASE2: 调用 ISA 解释器执行 fragment shader
    FragmentContext ctx = fragmentToContext(input);

    // 执行 ISA 指令
    m_shaderCore.executeFragment(ctx, m_currentShader);

    // 处理被 kill 的 fragment
    if (ctx.killed) {
        Fragment out = input;
        out.z = CLEAR_DEPTH;  // 推到远平面，depth test 会拒绝
        return out;
    }

    // 转换回 Fragment
    return contextToFragment(ctx);
}

}  // namespace SoftGPU
