// ============================================================================
// SoftGPU - PrimitiveAssembly.hpp
// 图元组装器
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>

namespace SoftGPU {

// ============================================================================
// PrimitiveAssembly - 图元组装器
// 职责：组装三角形，执行 AABB 视锥剔除
// ============================================================================
class PrimitiveAssembly : public IStage {
public:
    PrimitiveAssembly();

    // 设置输入
    void setInput(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices,
                  bool indexed);

    // IStage 实现
    const char* getName() const override { return "PrimitiveAssembly"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 供后续 Stage 获取三角形列表
    const std::vector<Triangle>& getOutput() const { return m_outputTriangles; }

private:
    std::vector<Vertex>   m_inputVertices;
    std::vector<uint32_t> m_inputIndices;
    bool                   m_indexed;
    std::vector<Triangle>  m_outputTriangles;
    PerformanceCounters    m_counters;

    // 内部：视锥剔除（AABB）
    bool shouldCull(const Triangle& tri) const;

    // 内部：透视除法 + NDC 计算
    void computeNDC(Vertex& v) const;
};

}  // namespace SoftGPU
