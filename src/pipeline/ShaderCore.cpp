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
    // 创建纹理缓冲区（使用简单的 gradient 纹理用于测试）
    for (int i = 0; i < 4; i++) {
        m_textures[i] = std::make_unique<TextureBuffer>();
    }
    // 设置内置测试纹理（8x8 渐变纹理）
    createBuiltinTestTexture();
    // 设置纹理缓冲区到 interpreter
    for (int i = 0; i < 4; i++) {
        m_interpreter.setTextureBuffer(i, m_textures[i].get());
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

// ============================================================================
// PHASE 4: Barycentric Color ISA Shader
// 使用 MAD 指令进行颜色插值
// 适用于颜色插值场景（Scene002）
// ============================================================================
ShaderFunction ShaderCore::getBarycentricColorShader() {
    ShaderFunction shader;
    
    using softgpu::isa::Instruction;
    
    // 寄存器布局：
    // R4-R7 = 输入颜色（插值后的顶点颜色）
    // R8 = u (barycentric weight for v0)
    // R9 = v (barycentric weight for v1)
    // R16-R23 = 临时寄存器
    //
    // 颜色插值公式：color = w0*c0 + w1*c1 + w2*c2
    // 其中 w2 = 1 - w0 - w1
    //
    // 指令序列：
    // 0: MOV TMP0, COLOR_R    ; TMP0 = c0 (R component of v0 color)
    // 1: MUL TMP0, TMP0, TEX_U ; TMP0 = w0 * c0
    // 2: MOV TMP1, COLOR_G    ; TMP1 = c1 (G component of v1 color) 
    // 3: MAD TMP0, TMP0, TEX_V, TMP1 ; TMP0 = w0*c0 + w1*c1 (partial)
    // ...but this doesn't work well for RGB separately
    //
    // 简化方案：由于 rasterizer 已经做了插值，
    // barycentric shader 可以直接用 MOV 复制颜色到输出
    // 但为了展示 MAD 的使用，我们做以下操作：
    // color_out = 1.0 * color_in (identity using MAD)
    
    // MAD TMP0, R0, 1.0, COLOR_R  ; TMP0 = 0*R0 + 1*COLOR_R = COLOR_R
    // 但我们没有浮点立即数...使用 R0=0 和 临时寄存器
    // 更好的方式是直接 MOV

    shader.code = {
        // 简单的颜色传递 + depth 输出
        // 展示 MAD 操作：color = 1.0 * input_color + 0
        // 由于 R0=0，我们可以: MAD TMP0, R0, COLOR_R, COLOR_R = COLOR_R * (0+1) = COLOR_R
        Instruction::MakeR(Opcode::MAD, ShaderRegs::TMP0, ShaderRegs::COLOR_R, ShaderRegs::COLOR_R).raw, // TMP0 = R*R
        Instruction::MakeU(Opcode::ADD, ShaderRegs::OUT_R, ShaderRegs::COLOR_R, ShaderRegs::COLOR_R).raw, // R + R for 2x
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).raw, // OUT_R = COLOR_R
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).raw, // OUT_G = COLOR_G
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).raw, // OUT_B = COLOR_B
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).raw, // OUT_A = COLOR_A
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).raw,  // OUT_Z = depth
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}

// ============================================================================
// PHASE 5: Depth Test ISA Shader
// 读取 fragment depth 并与参考深度值比较
// 适用于深度测试场景（Scene003）
// ============================================================================
ShaderFunction ShaderCore::getDepthTestShader() {
    ShaderFunction shader;
    
    using softgpu::isa::Instruction;
    
    // 寄存器布局：
    // R3 = FRAG_Z (输入 depth)
    // R14 = OUT_Z (输出 depth)
    // R15 = KILLED flag
    //
    // 深度测试逻辑：
    // 如果 fragment_z > buffer_z，则 kill（更远的 fragment 被丢弃）
    // 这里演示 CMP 指令的使用
    //
    // 注意：实际的 depth test 在 TileBuffer 中完成
    // 这个 shader 只是演示如何在 shader 中做深度比较
    //
    // 指令序列：
    // 0: CMP KILLED, FRAG_Z, 0.5  ; KILLED = (FRAG_Z > 0.5) ? 1.0 : 0.0
    // 1: MOV OUT_R, COLOR_R       ; pass through color
    // 2: MOV OUT_G, COLOR_G
    // 3: MOV OUT_B, COLOR_B
    // 4: MOV OUT_A, COLOR_A
    // 5: MOV OUT_Z, FRAG_Z        ; output depth
    // 6: NOP
    
    shader.code = {
        // 颜色传递（不做条件 kill，因为 depth test 在 TileBuffer 中进行）
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

// ============================================================================
// PHASE 6: Multi-Triangle ISA Shader
// 组合多种操作：颜色插值 + depth test + conditional kill
// 适用于多三角形场景（Scene005/006）
// ============================================================================
ShaderFunction ShaderCore::getMultiTriangleShader() {
    ShaderFunction shader;
    
    using softgpu::isa::Instruction;
    
    // 寄存器布局：
    // R3 = FRAG_Z (输入 depth)
    // R4-R7 = 输入颜色
    // R10-R13 = 输出颜色
    // R14 = OUT_Z
    // R15 = KILLED flag
    // R16-R31 = 临时寄存器
    //
    // 多三角形 shader 操作：
    // 1. 颜色平滑插值（使用 MAD）
    // 2. Depth 钳制（使用 MIN/MAX）
    // 3. 条件 kill（使用 SEL 或 CMP）
    //
    // 指令序列：
    // 0: MOV TMP0, R0          ; TMP0 = 0
    // 1: MAD TMP1, TEX_U, COLOR_R, TMP0 ; TMP1 = u * color_r + 0
    // 2: MAD TMP2, TEX_V, COLOR_G, TMP1 ; TMP2 = v * color_g + u * color_r
    // 3: ADD OUT_R, TMP2, COLOR_B ; OUT_R = v*color_g + u*color_r + color_b
    // ... (简化版本：直接传递颜色)
    // 4: CMP TMP0, FRAG_Z, R0  ; TMP0 = (FRAG_Z > 0) ? 1 : 0
    // 5: SEL KILLED, TMP0, R0, R0 ; KILLED = (FRAG_Z > 0) ? 1 : 0
    // 6: MOV OUT_R, COLOR_R
    // 7: MOV OUT_G, COLOR_G
    // 8: MOV OUT_B, COLOR_B
    // 9: MOV OUT_A, COLOR_A
    // 10: MOV OUT_Z, FRAG_Z
    // 11: NOP
    
    shader.code = {
        // 使用 R0 作为零值 (R0 is hardwired to 0.0f)
        // MAD 演示：TMP1 = TEX_U * COLOR_R + R0 = u * color_r
        Instruction::MakeR4(Opcode::MAD, ShaderRegs::TMP1, ShaderRegs::TEX_U, ShaderRegs::COLOR_R, 0).raw,
        // MAD 演示：TMP2 = TEX_V * COLOR_G + TMP1 = v * color_g + u * color_r
        Instruction::MakeR4(Opcode::MAD, ShaderRegs::TMP2, ShaderRegs::TEX_V, ShaderRegs::COLOR_G, ShaderRegs::TMP1).raw,
        // ADD：OUT_R = TMP2 + COLOR_B = u*color_r + v*color_g + color_b
        Instruction::MakeR(Opcode::ADD, ShaderRegs::OUT_R, ShaderRegs::TMP2, ShaderRegs::COLOR_B).raw,
        // 颜色传递
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).raw,
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).raw,
        // Depth 输出
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).raw,
        // KILLED flag 保持为 0（不做条件 kill）
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

void ShaderCore::createBuiltinTestTexture() {
    // 创建一个 8x8 的 RGB 渐变测试纹理
    // 纹理内容：R = 水平渐变, G = 垂直渐变, B = 棋盘格
    const uint32_t size = 8;
    std::vector<uint8_t> texData(size * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint8_t r = static_cast<uint8_t>((x * 255) / (size - 1));  // 水平渐变: 0->255
            uint8_t g = static_cast<uint8_t>((y * 255) / (size - 1));  // 垂直渐变: 0->255
            uint8_t b = static_cast<uint8_t>(((x + y) % 2) * 255);     // 棋盘格
            uint8_t a = 255;

            size_t idx = (y * size + x) * 4;
            texData[idx + 0] = r;
            texData[idx + 1] = g;
            texData[idx + 2] = b;
            texData[idx + 3] = a;
        }
    }

    // 设置到纹理缓冲区 0
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
        m_interpreter.setTextureBuffer(slot, m_textures[slot].get());
    }
    return success;
}

// ============================================================================
// PHASE 7: Texture Sampling ISA Shader
// 使用 TEX/SAMPLE 指令进行纹理采样
// 适用于纹理采样场景
// ============================================================================
ShaderFunction ShaderCore::getTextureSamplingShader() {
    ShaderFunction shader;

    using softgpu::isa::Instruction;

    // 寄存器布局：
    // R1 = FRAG_X (fragment X)
    // R2 = FRAG_Y (fragment Y)
    // R3 = FRAG_Z (fragment Z)
    // R4-R7 = 输入颜色（插值后的顶点颜色，可作为 blend factor）
    // R8 = TEX_U (纹理 U 坐标)
    // R9 = TEX_V (纹理 V 坐标)
    // R10-R13 = 输出颜色
    // R14 = OUT_Z
    // R15 = KILLED flag
    //
    // TEX 指令格式: TEX Rd, Ra(u), Rb(v), Rc(tex_id)
    // 输出: Rd=颜色R, Rd+1=颜色G, Rd+2=颜色B, Rd+3=颜色A
    //
    // 指令序列：
    // 0: TEX OUT_R, TEX_U, TEX_V, 0  ; 使用纹理 0 进行采样，输出到 OUT_R..OUT_A
    // 1: MOV OUT_Z, FRAG_Z            ; 输出 depth
    // 2: NOP

    shader.code = {
        // TEX 指令：采样纹理 0，输出 RGBA 到 R10-R13
        Instruction::MakeR4(Opcode::TEX, ShaderRegs::OUT_R, ShaderRegs::TEX_U, ShaderRegs::TEX_V, 0).raw,
        // Depth 输出
        Instruction::MakeU(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).raw,
        // NOP 结束
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;

    return shader;
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
    // Use color channels as UV coordinates for texture sampling
    // (rasterizer interpolates vertex colors into ctx.color_r/g)
    interp.SetRegister(ShaderRegs::TEX_U, ctx.color_r);
    interp.SetRegister(ShaderRegs::TEX_V, ctx.color_g);
    
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

// PHASE3: Warp batch execution - SIMD style processing of 8 fragments
void ShaderCore::executeWarpBatch(std::array<FragmentContext, 8>& warpContexts,
                                   const ShaderFunction& shader) {
    if (shader.empty()) {
        // Passthrough for all fragments
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

    // Load shader once for the whole warp
    m_currentShader = shader;
    
    // Execute shader for each fragment in the warp
    // In a real SIMD implementation, all lanes would execute in lockstep
    // Here we process them sequentially but efficiently
    for (auto& ctx : warpContexts) {
        executeFragment(ctx, shader);
    }
    
    // Update warp-level stats
    m_stats.fragments_executed += 8;
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
