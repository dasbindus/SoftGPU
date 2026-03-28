// ============================================================================
// SoftGPU - ShaderCore.cpp
// Shader Core 微架构实现
// ============================================================================

#include "ShaderCore.hpp"
#include "stages/TileBuffer.hpp"
#include "core/PipelineTypes.hpp"
#include "isa/ISA.hpp"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iostream>

namespace SoftGPU {

// ============================================================================
// 常量
// ============================================================================

namespace ShaderRegs {
    // 输入寄存器 (R1-R9)
    constexpr uint8_t FRAG_X = 1;
    constexpr uint8_t FRAG_Y = 2;
    constexpr uint8_t FRAG_Z = 3;
    constexpr uint8_t COLOR_R = 4;
    constexpr uint8_t COLOR_G = 5;
    constexpr uint8_t COLOR_B = 6;
    constexpr uint8_t COLOR_A = 7;
    constexpr uint8_t TEX_U = 8;
    constexpr uint8_t TEX_V = 9;
    
    // 输出寄存器 (R10-R15)
    constexpr uint8_t OUT_R = 10;
    constexpr uint8_t OUT_G = 11;
    constexpr uint8_t OUT_B = 12;
    constexpr uint8_t OUT_A = 13;
    constexpr uint8_t OUT_Z = 14;
    constexpr uint8_t KILLED = 15;
    
    // 临时寄存器 (R16-R31)
    constexpr uint8_t TMP0 = 16;
    constexpr uint8_t TMP1 = 17;
    constexpr uint8_t TMP2 = 18;
    
    // 着色器参数基址
    constexpr uint8_t ARG_BASE = 1;
}

// ============================================================================
// ShaderCore Implementation
// ============================================================================

ShaderCore::ShaderCore() {
    reset();
}

ShaderCore::~ShaderCore() {
}

void ShaderCore::loadShader(const ShaderFunction& shader) {
    m_currentShader = shader;
}

ShaderFunction ShaderCore::getDefaultFragmentShader() {
    ShaderFunction shader;
    shader.arg_count = 1;  // uniform block pointer
    
    // Fragment shader 指令序列
    // 功能：颜色插值 + depth 输出
    // 寄存器布局见 ShaderCore.hpp
    
    using softgpu::isa::Instruction;
    
    // 指令序列：
    // 0: MOV OUT_R, COLOR_R    ; R10 = R4
    // 1: MOV OUT_G, COLOR_G    ; R11 = R5
    // 2: MOV OUT_B, COLOR_B    ; R12 = R6
    // 3: MOV OUT_A, COLOR_A    ; R13 = R7
    // 4: MOV OUT_Z, FRAG_Z     ; R14 = R3 (depth)
    // 5: NOP                   ; end marker
    
    shader.code = {
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).raw,
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}

ShaderFunction ShaderCore::getFlatColorShader(float r, float g, float b, float a) {
    ShaderFunction shader;
    
    using softgpu::isa::Instruction;
    
    // 指令序列：
    // 0: LDC OUT_R, cbuf, 0    ; R10 = constant[0] (R)
    // 1: LDC OUT_G, cbuf, 1    ; R11 = constant[1] (G)
    // 2: LDC OUT_B, cbuf, 2    ; R12 = constant[2] (B)
    // 3: LDC OUT_A, cbuf, 3    ; R13 = constant[3] (A)
    // 4: MOV OUT_Z, FRAG_Z     ; R14 = depth
    // 5: NOP
    
    shader.code = {
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).raw,
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}

ShaderFunction ShaderCore::compileShader(const std::string& glsl_source) {
    // TODO: 实现 GLSL 编译器
    // 占位实现：返回默认 shader
    (void)glsl_source;
    return getDefaultFragmentShader();
}

void ShaderCore::setupFragmentInput(FragmentContext& ctx) {
    Interpreter& interp = m_interpreter;
    
    // 清零临时寄存器
    for (uint8_t i = ShaderRegs::TMP0; i < 32; ++i) {
        interp.SetRegister(i, 0.0f);
    }
    
    // 设置输入寄存器
    interp.SetRegister(ShaderRegs::FRAG_X, ctx.pos_x);
    interp.SetRegister(ShaderRegs::FRAG_Y, ctx.pos_y);
    interp.SetRegister(ShaderRegs::FRAG_Z, ctx.pos_z);
    interp.SetRegister(ShaderRegs::COLOR_R, ctx.color_r);
    interp.SetRegister(ShaderRegs::COLOR_G, ctx.color_g);
    interp.SetRegister(ShaderRegs::COLOR_B, ctx.color_b);
    interp.SetRegister(ShaderRegs::COLOR_A, ctx.color_a);
    interp.SetRegister(ShaderRegs::TEX_U, ctx.u);
    interp.SetRegister(ShaderRegs::TEX_V, ctx.v);
    
    // 初始化输出寄存器
    interp.SetRegister(ShaderRegs::OUT_R, 0.0f);
    interp.SetRegister(ShaderRegs::OUT_G, 0.0f);
    interp.SetRegister(ShaderRegs::OUT_B, 0.0f);
    interp.SetRegister(ShaderRegs::OUT_A, 1.0f);
    interp.SetRegister(ShaderRegs::OUT_Z, ctx.pos_z);
    interp.SetRegister(ShaderRegs::KILLED, 0.0f);
}

void ShaderCore::captureFragmentOutput(FragmentContext& ctx) {
    Interpreter& interp = m_interpreter;
    
    ctx.out_r = interp.GetRegister(ShaderRegs::OUT_R);
    ctx.out_g = interp.GetRegister(ShaderRegs::OUT_G);
    ctx.out_b = interp.GetRegister(ShaderRegs::OUT_B);
    ctx.out_a = interp.GetRegister(ShaderRegs::OUT_A);
    ctx.out_z = interp.GetRegister(ShaderRegs::OUT_Z);
    
    float killed_flag = interp.GetRegister(ShaderRegs::KILLED);
    ctx.killed = (killed_flag != 0.0f);
}

bool ShaderCore::stepShader() {
    // 获取当前 PC
    uint32_t pc = m_interpreter.GetPC();
    
    // 检查是否超出代码范围
    if (m_currentShader.empty() || pc / 4 >= m_currentShader.code.size()) {
        return false;
    }
    
    // 获取指令
    uint32_t instr_word = m_currentShader.code[pc / 4];
    Instruction inst(instr_word);
    
    // 解码并执行
    Opcode op = inst.GetOpcode();
    if (op == Opcode::INVALID) {
        return false;
    }
    
    // 执行指令
    m_interpreter.ExecuteInstruction(inst);
    m_stats.instructions_executed++;
    
    // 检查是否是终止指令
    if (op == Opcode::NOP) {
        return false;
    }
    
    // 更新周期计数
    m_stats.cycles_spent++;
    
    return true;
}

void ShaderCore::executeFragmentInternal(FragmentContext& ctx, const ShaderFunction& shader) {
    // 加载 shader
    m_currentShader = shader;
    
    // 设置 fragment 输入
    setupFragmentInput(ctx);
    
    // 重置 interpreter PC
    m_interpreter.Reset();
    // Note: Reset() clears registers, need to setup input again
    setupFragmentInput(ctx);
    
    // 执行 shader 指令序列
    uint32_t max_instructions = 1024;  // 防止死循环
    uint32_t instr_count = 0;
    
    while (instr_count < max_instructions) {
        uint32_t pc = m_interpreter.GetPC();
        
        // 检查是否超出代码范围
        if (pc / 4 >= m_currentShader.code.size()) {
            break;
        }
        
        // 获取指令
        uint32_t instr_word = m_currentShader.code[pc / 4];
        Instruction inst(instr_word);
        
        // 解码并执行
        Opcode op = inst.GetOpcode();
        if (op == Opcode::INVALID) {
            break;
        }
        
        // 执行指令
        m_interpreter.ExecuteInstruction(inst);
        m_stats.instructions_executed++;
        m_stats.cycles_spent++;
        instr_count++;
        
        // 检查是否是终止指令
        if (op == Opcode::NOP || op == Opcode::RET) {
            break;
        }
    }
    
    // 捕获输出
    captureFragmentOutput(ctx);
    
    m_stats.fragments_executed++;
}

void ShaderCore::executeFragment(FragmentContext& ctx, const ShaderFunction& shader) {
    if (shader.empty()) {
        // 没有 shader，使用 passthrough
        ctx.out_r = ctx.color_r;
        ctx.out_g = ctx.color_g;
        ctx.out_b = ctx.color_b;
        ctx.out_a = ctx.color_a;
        ctx.out_z = ctx.pos_z;
        ctx.killed = false;
        return;
    }
    
    executeFragmentInternal(ctx, shader);
}

void ShaderCore::executeFragmentBatch(std::vector<FragmentContext>& fragments,
                                       const ShaderFunction& shader) {
    for (auto& frag : fragments) {
        executeFragment(frag, shader);
    }
}

void ShaderCore::syncMemoryWithTileBuffer(TileBufferManager* tile_buffer, uint32_t tile_idx) {
    // 这个函数用于在需要时同步 memory_
    // 当前实现中 interpreter 有自己的 memory，不需要同步
    (void)tile_buffer;
    (void)tile_idx;
}

// ============================================================================
// 辅助函数实现
// ============================================================================

std::vector<uint32_t> makeFragmentShaderCode() {
    using softgpu::isa::Instruction;
    using softgpu::isa::Opcode;
    
    std::vector<uint32_t> code;
    
    // Simple passthrough shader
    // MOV OUT_R, COLOR_R
    code.push_back(Instruction::MakeU(Opcode::MOV, 10, 4).raw);
    // MOV OUT_G, COLOR_G
    code.push_back(Instruction::MakeU(Opcode::MOV, 11, 5).raw);
    // MOV OUT_B, COLOR_B
    code.push_back(Instruction::MakeU(Opcode::MOV, 12, 6).raw);
    // MOV OUT_A, COLOR_A
    code.push_back(Instruction::MakeU(Opcode::MOV, 13, 7).raw);
    // MOV OUT_Z, FRAG_Z
    code.push_back(Instruction::MakeU(Opcode::MOV, 14, 3).raw);
    // NOP
    code.push_back(Instruction::MakeNOP().raw);
    
    return code;
}

std::vector<uint32_t> makeTestShaderCode() {
    using softgpu::isa::Instruction;
    using softgpu::isa::Opcode;
    
    std::vector<uint32_t> code;
    
    // Test shader with some arithmetic
    // R16 = COLOR_R * 2.0
    // MAD OUT_R, COLOR_R, 2.0, 0  (Rd=Ra*Rb+Rc with Rc=0)
    // Actually MAD takes 4 registers, let's use simpler instructions
    
    // MOV TMP0, COLOR_R    ; TMP0 = R4
    code.push_back(Instruction::MakeU(Opcode::MOV, 16, 4).raw);
    // ADD TMP0, TMP0, TMP0 ; TMP0 = TMP0 + TMP0 = R4 * 2
    code.push_back(Instruction::MakeR(Opcode::ADD, 16, 16, 16).raw);
    // MOV OUT_R, TMP0      ; OUT_R = TMP0
    code.push_back(Instruction::MakeU(Opcode::MOV, 10, 16).raw);
    // MOV OUT_G, COLOR_G
    code.push_back(Instruction::MakeU(Opcode::MOV, 11, 5).raw);
    // MOV OUT_B, COLOR_B
    code.push_back(Instruction::MakeU(Opcode::MOV, 12, 6).raw);
    // MOV OUT_A, COLOR_A
    code.push_back(Instruction::MakeU(Opcode::MOV, 13, 7).raw);
    // MOV OUT_Z, FRAG_Z
    code.push_back(Instruction::MakeU(Opcode::MOV, 14, 3).raw);
    // NOP
    code.push_back(Instruction::MakeNOP().raw);
    
    return code;
}

} // namespace SoftGPU
