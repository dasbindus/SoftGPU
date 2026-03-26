// ============================================================================
// SoftGPU - VertexShader.hpp
// 顶点着色器
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>

namespace SoftGPU {

// ============================================================================
// VertexShader - 顶点着色器
// 职责：MVP 变换，处理顶点
// ============================================================================
class VertexShader : public IStage {
public:
    VertexShader();

    // 设置输入数据
    void setInput(const std::vector<float>& vb,
                  const std::vector<uint32_t>& ib,
                  const Uniforms& uniforms);

    // 设置要处理的顶点数量
    void setVertexCount(size_t count);

    // IStage 实现
    const char* getName() const override { return "VertexShader"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 供后续 Stage 获取变换后的顶点
    const std::vector<Vertex>& getOutput() const { return m_outputVertices; }

private:
    std::vector<float>   m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    Uniforms              m_uniforms;
    size_t                m_vertexCount = 0;

    std::vector<Vertex>   m_outputVertices;
    PerformanceCounters   m_counters;

    // 内部：执行 MVP 变换
    Vertex transformVertex(const float* rawVertex) const;
};

}  // namespace SoftGPU
