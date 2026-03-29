// ============================================================================
// SoftGPU - FragmentShader.hpp
// 片段着色器
// PHASE2: Added TileBuffer output support
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include "stages/TileBuffer.hpp"
#include "pipeline/ShaderCore.hpp"
#include <memory>
#include <vector>

namespace SoftGPU {

// ============================================================================
// FragmentShader - 片段着色器
// 职责：逐 fragment 执行着色（Phase1: flat color / passthrough）
// PHASE2: 支持输出到 TileBuffer（per-tile LMEM）
// PHASE2: 集成 ShaderCore ISA 解释器
// ============================================================================
class FragmentShader : public IStage {
public:
    FragmentShader();

    // ========================================================================
    // PHASE1 兼容接口
    // ========================================================================
    void setInput(const std::vector<Fragment>& fragments);
    void setInputFromConnect(const std::vector<Fragment>& fragments);

    // ========================================================================
    // PHASE2 TileBuffer 接口
    // ========================================================================
    // 注入 TileBufferManager 引用（PHASE2 模式）
    void setTileBufferManager(TileBufferManager* manager);
    void setTileIndex(uint32_t idx);

    // 设置输入并执行 PHASE2 模式（写入 TileBuffer）
    void setInputAndExecuteTile(const std::vector<Fragment>& fragments,
                                 uint32_t tileX, uint32_t tileY);

    // ========================================================================
    // IStage 实现
    // ========================================================================
    const char* getName() const override { return "FragmentShader"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 供后续 Stage 获取着色后的 fragments（PHASE1 模式）
    const std::vector<Fragment>& getOutput() const { return m_outputFragments; }

    // 是否为 PHASE2 TileBuffer 模式
    bool isTileBufferMode() const { return m_tileBuffer != nullptr; }

private:
    // Pointer to previous stage's output (set by connectStages)
    const std::vector<Fragment>* m_inputFragmentsPtr = nullptr;
    // Version counter: 1 = set by connectStages, >= 2 = setInput() called after connect
    uint32_t m_inputVersion = 0;
    std::vector<Fragment>  m_inputFragments;   // copy fallback
    std::vector<Fragment>  m_outputFragments;
    PerformanceCounters   m_counters;

    // PHASE2 TileBuffer 模式
    TileBufferManager*    m_tileBuffer = nullptr;
    uint32_t              m_tileIndex = 0;

    // PHASE2: ShaderCore 执行引擎
    ShaderCore            m_shaderCore;

    // PHASE2: ISA shader 函数
    ShaderFunction        m_currentShader;

    // 内部：逐 fragment 着色
    Fragment shade(const Fragment& input);

    // 辅助：Fragment <-> FragmentContext 互转
    FragmentContext fragmentToContext(const Fragment& frag) const;
    Fragment contextToFragment(const FragmentContext& ctx) const;
};

}  // namespace SoftGPU
