// ============================================================================
// SoftGPU - Rasterizer.hpp
// 光栅化器
// PHASE2: Added setInputPerTile() for per-tile triangle list
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>

namespace SoftGPU {

// ============================================================================
// Rasterizer - 光栅化器
// 职责：DDA 光栅化，输出 fragment 列表
// PHASE2: 支持两种模式：
//   - 全量模式（PHASE1 兼容）：setInput() + execute()
//   - Per-tile 模式：setInputPerTile() + executePerTile()
// ============================================================================
class Rasterizer : public IStage {
public:
    Rasterizer();

    // 设置视口尺寸
    void setViewport(uint32_t width, uint32_t height);

    // ========================================================================
    // PHASE1 兼容接口（全量 triangles）
    // ========================================================================
    void setInput(const std::vector<Triangle>& triangles);
    void setInputFromConnect(const std::vector<Triangle>& triangles);

    // ========================================================================
    // PHASE2 Per-tile 接口
    // ========================================================================
    // 设置 per-tile 输入三角形（来自 TilingStage）
    void setInputPerTile(const std::vector<Triangle>& triangles,
                         uint32_t tileX, uint32_t tileY);

    // 执行 per-tile 光栅化（输出到 m_outputFragments）
    void executePerTile();

    // 清空输出
    void clearOutput() { m_outputFragments.clear(); }

    // ========================================================================
    // IStage 实现
    // ========================================================================
    const char* getName() const override { return "Rasterizer"; }
    void execute() override;  // PHASE1 兼容：全量光栅化
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 获取输出 fragments
    const std::vector<Fragment>& getOutput() const { return m_outputFragments; }

private:
    uint32_t           m_viewportWidth  = FRAMEBUFFER_WIDTH;
    uint32_t           m_viewportHeight = FRAMEBUFFER_HEIGHT;

    // PHASE1 兼容
    const std::vector<Triangle>* m_inputTrianglesPtr = nullptr;
    uint32_t m_inputVersion = 0;  // 1 = connectStages, >=2 = setInput after connect
    std::vector<Triangle>   m_inputTriangles;

    // PHASE2 Per-tile 模式
    std::vector<Triangle>  m_inputTrianglesPerTile;
    uint32_t m_tileX = 0;
    uint32_t m_tileY = 0;
    bool m_perTileMode = false;

    std::vector<Fragment>  m_outputFragments;
    PerformanceCounters     m_counters;

    // 内部：光栅化单个三角形
    void rasterizeTriangle(const Triangle& tri);

    // 内部：光栅化单个三角形（per-tile 版本）
    void rasterizeTrianglePerTile(const Triangle& tri, uint32_t tileX, uint32_t tileY);

    // 内部：计算 edge function (returns signed area * 2)
    float edgeFunction(float px, float py,
                       float ax, float ay,
                       float bx, float by) const;

    // 内部：重心坐标插值
    void interpolateAttributes(const Triangle& tri, float baryX, float baryY,
                               float area, Fragment& frag) const;
};

}  // namespace SoftGPU
