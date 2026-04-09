// ============================================================================
// SoftGPU - VertexShader.hpp
// 顶点着色器
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include "VSOutputAssembler.hpp"
#include "../isa/Interpreter.hpp"
#include <vector>

namespace SoftGPU {

// ============================================================================
// VSExecutionMode - 双路径执行模式
// ============================================================================
enum class VSExecutionMode {
    Auto,   // 优先 ISA，无 ISA program 时 fallback C++
    ISA,    // 强制 ISA 路径
    CPP     // 强制 C++ 路径（调试用）
};

// ============================================================================
// VertexShader - 顶点着色器
// 职责：MVP 变换，处理顶点
// 支持 C++ 路径和 ISA bytecode 路径
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

    // === ISA bytecode 接口 ===

    // 设置 VS 程序（ISA bytecode）
    void SetProgram(const uint32_t* program, size_t word_count);

    // 设置执行模式
    void SetExecutionMode(VSExecutionMode mode) { m_execMode = mode; }
    VSExecutionMode GetExecutionMode() const { return m_execMode; }

    // 设置 ATTR 布局表（attr_id → byte_offset）
    void SetAttrLayout(const std::vector<size_t>& layout);

    // 检查是否有 ISA 程序
    bool HasProgram() const { return !m_vsProgram.empty(); }

private:
    // ISA 路径执行
    void executeISA();

    // C++ 路径执行（参考实现）
    void executeCPPRef();

    // 加载 uniforms 到 interpreter 寄存器
    void loadUniformsToRegisters(softgpu::isa::Interpreter& interp);

    std::vector<float>    m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;
    Uniforms              m_uniforms;
    size_t                m_vertexCount = 0;

    std::vector<Vertex>   m_outputVertices;
    PerformanceCounters   m_counters;

    // ISA 相关
    VSExecutionMode m_execMode = VSExecutionMode::Auto;
    std::vector<uint32_t> m_vsProgram;        // ISA bytecode
    std::vector<size_t>   m_attrTable;        // ATTR table: attr_id → byte_offset
    softgpu::isa::Interpreter m_interpreter;  // ISA 解释器
    VSOutputAssembler     m_assembler;        // 输出汇编器

    // 内部：执行 MVP 变换（C++ 路径）
    Vertex transformVertex(const float* rawVertex) const;
};

}  // namespace SoftGPU
