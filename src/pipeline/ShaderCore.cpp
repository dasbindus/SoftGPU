// ============================================================================
// SoftGPU - ShaderCore.cpp
// Shader Core 微架构实现
// v2.5 ISA
// ============================================================================

#include "ShaderCore.hpp"
#include "core/ShaderRegs.hpp"
#include "stages/TileBuffer.hpp"
#include "core/PipelineTypes.hpp"
#include "isa/interpreter_v2_5.hpp"
#include <cstring>
#include <cmath>
#include <sstream>
#include <iostream>
#include <cassert>

namespace SoftGPU {

// ============================================================================
// ShaderCore Implementation
// ============================================================================

ShaderCore::ShaderCore() {
    // 创建纹理缓冲区（使用简单的 gradient 纹理用于测试）
    for (int i = 0; i < 4; i++) {
        m_textures[i] = std::make_unique<TextureBuffer>();
    }
    // 设置内置测试纹理（8x8 渐变纹理）
    createBuiltinTestTexture();
    // 设置纹理缓冲区到 interpreter
    for (int i = 0; i < 4; i++) {
        m_interpreter.SetTextureBuffer(i, m_textures[i].get());
    }
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

    // v2.5 ISA: MOV = Format-C (MakeC), NOP = Format-D (MakeD)
    shader.code = {
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).word1,
        Instruction::MakeD(Opcode::HALT).word1
    };
    shader.start_addr = 0;

    return shader;
}

// Unified factory: replaces getFlatColorShader, getBarycentricColorShader, getDepthTestShader
// which all produced identical output (color passthrough + depth output).
ShaderFunction makeColorPassShader() {
    ShaderFunction shader;
    shader.code = {
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).word1,
        Instruction::MakeD(Opcode::HALT).word1
    };
    shader.start_addr = 0;
    return shader;
}

ShaderFunction ShaderCore::getMultiTriangleShader() {
    ShaderFunction shader;

    // v2.5 ISA: MAD 演示颜色插值
    shader.code = {
        // MAD TMP1 = TEX_U * COLOR_R + R0 = u * color_r
        Instruction::MakeA(Opcode::MAD, ShaderRegs::TMP1, ShaderRegs::TEX_U, ShaderRegs::COLOR_R).word1,
        // MAD TMP2 = TEX_V * COLOR_G + TMP1 = v * color_g + u * color_r
        Instruction::MakeA(Opcode::MAD, ShaderRegs::TMP2, ShaderRegs::TEX_V, ShaderRegs::COLOR_G).word1,
        // ADD OUT_R = TMP2 + COLOR_B = u*color_r + v*color_g + color_b
        Instruction::MakeA(Opcode::ADD, ShaderRegs::OUT_R, ShaderRegs::TMP2, ShaderRegs::COLOR_B).word1,
        // 颜色传递
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).word1,
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).word1,
        // Depth 输出
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).word1,
        Instruction::MakeD(Opcode::HALT).word1
    };
    shader.start_addr = 0;

    return shader;
}

ShaderFunction ShaderCore::compileShader(const std::string& glsl_source) {
    (void)glsl_source;
    assert(false && "compileShader not implemented — use built-in shader factories");
    return {};
}

void ShaderCore::createBuiltinTestTexture() {
    const uint32_t size = 8;
    std::vector<uint8_t> texData(size * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t r = static_cast<uint8_t>((x * 255) / (size - 1));
            uint8_t g = static_cast<uint8_t>((y * 255) / (size - 1));
            uint8_t b = static_cast<uint8_t>(((x + y) % 2) * 255);
            uint8_t a = 255;

            size_t idx = (y * size + x) * 4;
            texData[idx + 0] = r;
            texData[idx + 1] = g;
            texData[idx + 2] = b;
            texData[idx + 3] = a;
        }
    }

    if (m_textures[0]) {
        m_textures[0]->setData(size, size, texData.data());
    }
}

bool ShaderCore::setTextureFromPNG(int slot, const std::string& filename) {
    if (slot < 0 || slot >= 4) {
        return false;
    }
    if (!m_textures[slot]) {
        m_textures[slot] = std::make_unique<TextureBuffer>();
    }
    bool success = m_textures[slot]->loadFromPNG(filename);
    if (success) {
        m_interpreter.SetTextureBuffer(slot, m_textures[slot].get());
    }
    return success;
}

ShaderFunction ShaderCore::getTextureSamplingShader() {
    ShaderFunction shader;

    // v2.5 ISA:
    // TEX Rd, Ra(u), Rb(v) - texture sample, outputs RGBA to Rd..Rd+3
    // MOV Rd, Ra - copy register
    // HALT - terminate execution
    //
    // 注意：v2.5 TEX 使用硬编码 checkerboard 算法，
    // 不使用实际纹理数据（这是 v2.5 解释器的已知限制）
    shader.code = {
        // TEX: 采样纹理 0，输出 RGBA 到 R10-R13
        // v2.5 Format-A: opcode(8) | rd(7) | ra(7) | rb(7) | func(9)
        // TEX opcode=0x32, texid 参数在 v2.5 中被忽略
        Instruction::MakeA(Opcode::TEX, ShaderRegs::OUT_R, ShaderRegs::TEX_U, ShaderRegs::TEX_V).word1,
        // MOV: depth 输出
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).word1,
        // HALT: 结束
        Instruction::MakeD(Opcode::HALT).word1
    };
    shader.start_addr = 0;

    return shader;
}

void ShaderCore::setupFragmentInput(FragmentContext& ctx) {
    auto& interp = m_interpreter;

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
    auto& interp = m_interpreter;

    ctx.out_r = interp.GetRegister(ShaderRegs::OUT_R);
    ctx.out_g = interp.GetRegister(ShaderRegs::OUT_G);
    ctx.out_b = interp.GetRegister(ShaderRegs::OUT_B);
    ctx.out_a = interp.GetRegister(ShaderRegs::OUT_A);
    ctx.out_z = interp.GetRegister(ShaderRegs::OUT_Z);

    float killed_flag = interp.GetRegister(ShaderRegs::KILLED);
    ctx.killed = (killed_flag != 0.0f);
}

bool ShaderCore::stepShader() {
    // v2.5: 使用 Run(1) 执行单步
    m_interpreter.Run(1);
    const auto& st = m_interpreter.GetStats();
    m_stats.instructions_executed += st.instructions_executed;
    m_stats.cycles_spent += st.cycles;
    // stepShader returns false when shader execution is done (run_ == false)
    return st.instructions_executed == 0;
}

void ShaderCore::executeFragmentInternal(FragmentContext& ctx, const ShaderFunction& shader) {
    // 加载 shader 到 interpreter
    m_currentShader = shader;
    if (m_currentShader.empty()) {
        return;
    }

    m_interpreter.LoadProgram(m_currentShader.code.data(), m_currentShader.code.size(), 0);

    // 设置 fragment 输入
    m_interpreter.Reset();
    setupFragmentInput(ctx);

    // 执行 shader（v2.5 Run 方法管理内部 PC）
    // max_cycles = 1024 防止死循环
    m_interpreter.Run(1024);

    // 同步 v2.5 stats 到 ShaderCore stats
    const auto& st = m_interpreter.GetStats();
    m_stats.instructions_executed += st.instructions_executed;
    m_stats.cycles_spent += st.cycles;

    // 捕获输出
    captureFragmentOutput(ctx);

    m_stats.fragments_executed++;
}

void ShaderCore::executeFragment(FragmentContext& ctx, const ShaderFunction& shader) {
    if (shader.empty()) {
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

void ShaderCore::executeWarpBatch(std::array<FragmentContext, 8>& warpContexts,
                                   const ShaderFunction& shader) {
    if (shader.empty()) {
        for (auto& ctx : warpContexts) {
            ctx.out_r = ctx.color_r;
            ctx.out_g = ctx.color_g;
            ctx.out_b = ctx.color_b;
            ctx.out_a = ctx.color_a;
            ctx.out_z = ctx.pos_z;
            ctx.killed = false;
        }
        return;
    }

    m_currentShader = shader;

    for (auto& ctx : warpContexts) {
        executeFragment(ctx, shader);
    }

    m_stats.fragments_executed += 8;
}

void ShaderCore::syncMemoryWithTileBuffer(TileBufferManager*, uint32_t) {
    // v2.5 interpreter has its own memory; no sync needed
}

// ============================================================================
// 辅助函数实现
// ============================================================================

std::vector<uint32_t> makeFragmentShaderCode() {
    using softgpu::isa::v2_5::Instruction;
    using softgpu::isa::v2_5::Opcode;

    std::vector<uint32_t> code;

    // v2.5: MOV OUT_R, COLOR_R (Format-C)
    code.push_back(Instruction::MakeC(Opcode::MOV, 10, 4).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 11, 5).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 12, 6).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 13, 7).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 14, 3).word1);
    code.push_back(Instruction::MakeD(Opcode::HALT).word1);

    return code;
}

std::vector<uint32_t> makeTestShaderCode() {
    using softgpu::isa::v2_5::Instruction;
    using softgpu::isa::v2_5::Opcode;

    std::vector<uint32_t> code;

    // MAD TMP0, COLOR_R, R0, COLOR_R = COLOR_R
    code.push_back(Instruction::MakeA(Opcode::MAD, 16, 4, 4).word1);
    // ADD TMP0, TMP0, TMP0 = COLOR_R * 2
    code.push_back(Instruction::MakeA(Opcode::ADD, 16, 16, 16).word1);
    // MOV OUT_R, TMP0
    code.push_back(Instruction::MakeC(Opcode::MOV, 10, 16).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 11, 5).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 12, 6).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 13, 7).word1);
    code.push_back(Instruction::MakeC(Opcode::MOV, 14, 3).word1);
    code.push_back(Instruction::MakeD(Opcode::HALT).word1);

    return code;
}

} // namespace SoftGPU
