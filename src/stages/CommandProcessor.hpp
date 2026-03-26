// ============================================================================
// SoftGPU - CommandProcessor.hpp
// 命令处理器
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/RenderCommand.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>

namespace SoftGPU {

// ============================================================================
// CommandProcessor - 命令处理器
// 职责：接收 DrawCall 参数，解析 VB / IB，准备渲染数据
// ============================================================================
class CommandProcessor : public IStage {
public:
    CommandProcessor();

    // 设置当前命令
    void setCommand(const RenderCommand& cmd);

    // IStage 实现
    const char* getName() const override { return "CommandProcessor"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 供后续 Stage 获取数据
    const std::vector<float>& getVertexBuffer() const { return m_vertexBuffer; }
    const std::vector<uint32_t>& getIndexBuffer() const { return m_indexBuffer; }
    const DrawParams& getDrawParams() const { return m_drawParams; }
    const Uniforms& getUniforms() const { return m_uniforms; }

private:
    RenderCommand     m_currentCommand;
    PerformanceCounters m_counters;

    std::vector<float>   m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    DrawParams           m_drawParams;
    Uniforms             m_uniforms;
};

}  // namespace SoftGPU
