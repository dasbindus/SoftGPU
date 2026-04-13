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
    // TEX_U/V: use color channels (r,g) as texture coordinates
    // (rasterizer interpolates vertex r/g as frag.color_r/g)
    m_interpreter.SetRegister(ShaderRegs::TEX_U, ctx.color_r);
    m_interpreter.SetRegister(ShaderRegs::TEX_V, ctx.color_g);

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
// Vertex Shader ISA bytecode - MVP Transform
// ============================================================================
//
// VBO layout (VERTEX_STRIDE=8 floats):
//   Byte 0-15:   position  (x, y, z, w)     → float[0..3]
//   Byte 16-31:  color      (r, g, b, a)     → float[4..7]
//
// Register allocation for MVP transform:
//   R4-R7  = vertex position (x,y,z,w)  [loaded by VLOAD]
//   R8-R11 = vertex color    (r,g,b,a)  [loaded by VLOAD]
//
// Matrix registers (set by SetUniforms / RunVertexProgram):
//   M: R8-R23  (col0=R8..R11, col1=R12..R15, col2=R16..R19, col3=R20..R23)
//   V: R24-R39 (col0=R24..R27, col1=R28..R31, col2=R32..R35, col3=R36..R39)
//   P: R40-R55 (col0=R40..R43, col1=R44..R47, col2=R48..R51, col3=R52..R55)
//
// Intermediate registers (MUL+ADD sequence):
//   R28-R31: model transform result t = M * v
//   R32-R35: view transform result u = V * t
//   R12-R15: final clip coordinates clip = P * u
//
// Note: Column-major matrix rows are STRIDED (e.g., M_row0 = {R8,R12,R16,R20}),
// so we use MUL+MAD sequence instead of DOT4 for matrix-vector multiply.
// ============================================================================

ShaderFunction ShaderCore::getDefaultVertexShader() {
    ShaderFunction shader;
    shader.arg_count = 0;
    shader.start_addr = 0;

    using namespace softgpu::isa::v2_5;

    // === Load vertex data ===
    // VLOAD R4, #0: loads position (x,y,z,w) into R4-R7
    shader.code.push_back(Instruction::MakeB(Opcode::VLOAD, 4, 0, 0, 0).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::VLOAD, 4, 0, 0, 0).word2);
    // ATTR R8, #4: load color.r from VBO float[4]
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 8, 0, 0, 4).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 8, 0, 0, 4).word2);
    // ATTR R9, #5: load color.g from VBO float[5]
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 9, 0, 0, 5).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 9, 0, 0, 5).word2);
    // ATTR R10, #6: load color.b from VBO float[6]
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 10, 0, 0, 6).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 10, 0, 0, 6).word2);
    // ATTR R11, #7: load color.a from VBO float[7]
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 11, 0, 0, 7).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::ATTR, 11, 0, 0, 7).word2);

    // =========================================================================
    // Model transform: t = M * v
    // M is column-major: col0=R8..R11, col1=R12..R15, col2=R16..R19, col3=R20..R23
    // t.x = M[0]*x + M[4]*y + M[8]*z + M[12]*w
    // Using MUL+MAD chain: t.x = ((M[0]*x + M[4]*y) + M[8]*z) + M[12]*w
    // =========================================================================

    // t.x = M[0]*x + M[4]*y + M[8]*z + M[12]*w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 28, 8,  4).word1);   // R28 = M[0]*x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 28, 12, 5).word1);   // R28 += M[4]*y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 28, 16, 6).word1);   // R28 += M[8]*z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 28, 20, 7).word1);   // R28 += M[12]*w

    // t.y = M[1]*x + M[5]*y + M[9]*z + M[13]*w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 29, 9,  4).word1);   // R29 = M[1]*x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 29, 13, 5).word1);   // R29 += M[5]*y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 29, 17, 6).word1);   // R29 += M[9]*z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 29, 21, 7).word1);   // R29 += M[13]*w

    // t.z = M[2]*x + M[6]*y + M[10]*z + M[14]*w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 30, 10, 4).word1);   // R30 = M[2]*x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 30, 14, 5).word1);   // R30 += M[6]*y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 30, 18, 6).word1);   // R30 += M[10]*z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 30, 22, 7).word1);   // R30 += M[14]*w

    // t.w = M[3]*x + M[7]*y + M[11]*z + M[15]*w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 31, 11, 4).word1);   // R31 = M[3]*x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 31, 15, 5).word1);   // R31 += M[7]*y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 31, 19, 6).word1);   // R31 += M[11]*z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 31, 23, 7).word1);   // R31 += M[15]*w
    // R28=t.x, R29=t.y, R30=t.z, R31=t.w

    // =========================================================================
    // View transform: u = V * t
    // V is column-major: col0=R24..R27, col1=R28..R31, col2=R32..R35, col3=R36..R39
    // Result u goes to R32-R35
    // =========================================================================

    // u.x = V[0]*t.x + V[4]*t.y + V[8]*t.z + V[12]*t.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 32, 24, 28).word1);   // R32 = V[0]*t.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 32, 28, 29).word1);   // R32 += V[4]*t.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 32, 32, 30).word1);   // R32 += V[8]*t.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 32, 36, 31).word1);   // R32 += V[12]*t.w

    // u.y = V[1]*t.x + V[5]*t.y + V[9]*t.z + V[13]*t.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 33, 25, 28).word1);   // R33 = V[1]*t.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 33, 29, 29).word1);   // R33 += V[5]*t.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 33, 33, 30).word1);   // R33 += V[9]*t.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 33, 37, 31).word1);   // R33 += V[13]*t.w

    // u.z = V[2]*t.x + V[6]*t.y + V[10]*t.z + V[14]*t.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 34, 26, 28).word1);   // R34 = V[2]*t.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 34, 30, 29).word1);   // R34 += V[6]*t.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 34, 34, 30).word1);   // R34 += V[10]*t.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 34, 38, 31).word1);   // R34 += V[14]*t.w

    // u.w = V[3]*t.x + V[7]*t.y + V[11]*t.z + V[15]*t.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 35, 27, 28).word1);   // R35 = V[3]*t.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 35, 31, 29).word1);   // R35 += V[7]*t.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 35, 35, 30).word1);   // R35 += V[11]*t.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 35, 39, 31).word1);   // R35 += V[15]*t.w
    // R32=u.x, R33=u.y, R34=u.z, R35=u.w

    // =========================================================================
    // Projection transform: clip = P * u
    // P is column-major: col0=R40..R43, col1=R44..R47, col2=R48..R51, col3=R52..R55
    // Result clip goes to R12-R15
    // =========================================================================

    // clip.x = P[0]*u.x + P[4]*u.y + P[8]*u.z + P[12]*u.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 12, 40, 32).word1);   // R12 = P[0]*u.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 12, 44, 33).word1);   // R12 += P[4]*u.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 12, 48, 34).word1);   // R12 += P[8]*u.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 12, 52, 35).word1);   // R12 += P[12]*u.w

    // clip.y = P[1]*u.x + P[5]*u.y + P[9]*u.z + P[13]*u.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 13, 41, 32).word1);   // R13 = P[1]*u.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 13, 45, 33).word1);   // R13 += P[5]*u.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 13, 49, 34).word1);   // R13 += P[9]*u.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 13, 53, 35).word1);   // R13 += P[13]*u.w

    // clip.z = P[2]*u.x + P[6]*u.y + P[10]*u.z + P[14]*u.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 14, 42, 32).word1);   // R14 = P[2]*u.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 14, 46, 33).word1);   // R14 += P[6]*u.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 14, 50, 34).word1);   // R14 += P[10]*u.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 14, 54, 35).word1);   // R14 += P[14]*u.w

    // clip.w = P[3]*u.x + P[7]*u.y + P[11]*u.z + P[15]*u.w
    shader.code.push_back(Instruction::MakeA(Opcode::MUL, 15, 43, 32).word1);   // R15 = P[3]*u.x
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 15, 47, 33).word1);   // R15 += P[7]*u.y
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 15, 51, 34).word1);   // R15 += P[11]*u.z
    shader.code.push_back(Instruction::MakeA(Opcode::MAD, 15, 55, 35).word1);   // R15 += P[15]*u.w
    // R12=clip.x, R13=clip.y, R14=clip.z, R15=clip.w

    // === Output clip coordinates ===
    shader.code.push_back(Instruction::MakeB(Opcode::OUTPUT_VS, 12, 0, 0, 0).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::OUTPUT_VS, 12, 0, 0, 0).word2);

    // === Store color to VATTR buffer for fragment shader ===
    // After VLOAD, R8-R11 = {r,g,b,a} (from VBO float indices 4,5,6,7)
    // VSTORE R8, #0: writes R8-R11 to vabuf_[0..3]
    // GetVAttrFloat(0, 0..3) reads vabuf_[0..3] = {r,g,b,a}
    shader.code.push_back(Instruction::MakeB(Opcode::VSTORE, 8, 0, 8, 0).word1);
    shader.code.push_back(Instruction::MakeB(Opcode::VSTORE, 8, 0, 8, 0).word2);

    // === HALT ===
    shader.code.push_back(Instruction::MakeD(Opcode::HALT).word1);

    return shader;
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
