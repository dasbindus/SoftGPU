// ============================================================================
// SoftGPU - CommandProcessor.cpp
// ============================================================================

#include "CommandProcessor.hpp"

#include <chrono>

namespace SoftGPU {

CommandProcessor::CommandProcessor() {
    resetCounters();
}

void CommandProcessor::setCommand(const RenderCommand& cmd) {
    m_currentCommand = cmd;
}

void CommandProcessor::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    m_counters.invocation_count++;

    // 复制 vertex buffer 数据 (使用正确的 stride)
    size_t actualVertexSize = m_currentCommand.drawParams.vertexCount * VERTEX_STRIDE;
    m_vertexBuffer.resize(actualVertexSize);
    if (m_currentCommand.vertexBufferData && actualVertexSize > 0) {
        for (size_t i = 0; i < actualVertexSize; ++i) {
            m_vertexBuffer[i] = m_currentCommand.vertexBufferData[i];
        }
    }

    // 复制 index buffer 数据
    m_indexBuffer.resize(m_currentCommand.indexBufferSize);
    if (m_currentCommand.indexBufferData && m_currentCommand.indexBufferSize > 0) {
        for (size_t i = 0; i < m_currentCommand.indexBufferSize; ++i) {
            m_indexBuffer[i] = m_currentCommand.indexBufferData[i];
        }
    }

    // 复制 draw params
    m_drawParams = m_currentCommand.drawParams;

    // 构建 uniforms
    m_uniforms.viewMatrix = m_currentCommand.viewMatrix;
    m_uniforms.projectionMatrix = m_currentCommand.projectionMatrix;
    m_uniforms.modelMatrix = m_currentCommand.modelMatrix;

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void CommandProcessor::resetCounters() {
    m_counters = PerformanceCounters{};
}

}  // namespace SoftGPU
