// ============================================================================
// SoftGPU - ShaderCore.hpp
// Shader Core 微架构
// Phase 2: 将 ISA 解释器集成到渲染管线
// ============================================================================

#pragma once

#include "core/PipelineTypes.hpp"
#include "stages/TileBuffer.hpp"
#include "isa/interpreter_v2_5.hpp"
#include "pipeline/TextureBuffer.hpp"
#include <memory>
#include <vector>
#include <array>
#include <functional>

namespace SoftGPU {

using softgpu::isa::v2_5::Instruction;
using softgpu::isa::v2_5::Opcode;
using softgpu::isa::v2_5::Interpreter;

// ============================================================================
// ShaderFunction - 编译后的着色器函数
// ============================================================================
struct ShaderFunction {
    std::vector<uint32_t> code;      // 指令序列 (32-bit words)
    uint32_t start_addr = 0;         // 入口地址
    uint32_t local_size = 0;         // 局部变量大小 (in registers)
    uint32_t arg_count = 0;          // 参数个数
    
    bool empty() const { return code.empty(); }
    size_t size() const { return code.size(); }
};

// ============================================================================
// FragmentContext - Fragment 执行上下文
// ============================================================================
class FragmentContext {
public:
    // 输入属性
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;
    float color_r = 0.0f;
    float color_g = 0.0f;
    float color_b = 0.0f;
    float color_a = 1.0f;
    float u = 0.0f;
    float v = 0.0f;
    
    // 输出颜色
    float out_r = 0.0f;
    float out_g = 0.0f;
    float out_b = 0.0f;
    float out_a = 1.0f;
    float out_z = 1.0f;
    
    // 执行状态
    bool killed = false;  // 被 discard 语句杀死
    uint32_t tile_x = 0;
    uint32_t tile_y = 0;
    
    void reset() {
        pos_x = pos_y = pos_z = 0.0f;
        color_r = color_g = color_b = 0.0f;
        color_a = out_r = out_g = out_b = 0.0f;
        out_a = 1.0f;
        out_z = 1.0f;
        u = v = 0.0f;
        killed = false;
    }
};

// ============================================================================
// ShaderCore - Shader 执行微架构
// ============================================================================
class ShaderCore {
public:
    // 统计信息
    struct Stats {
        uint64_t fragments_executed = 0;
        uint64_t instructions_executed = 0;
        uint64_t cycles_spent = 0;
        uint64_t warps_scheduled = 0;
        
        void reset() {
            fragments_executed = 0;
            instructions_executed = 0;
            cycles_spent = 0;
            warps_scheduled = 0;
        }
    };

    ShaderCore();
    ~ShaderCore();
    
    // ========================================================================
    // Fragment Shader 执行
    // ========================================================================
    
    // 执行单个 fragment（使用默认寄存器布局）
    // 寄存器分配：
    //   R0  = zero (0.0f)
    //   R1  = fragment.x (输入)
    //   R2  = fragment.y (输入)
    //   R3  = fragment.z (输入)
    //   R4  = fragment.color_r (输入)
    //   R5  = fragment.color_g (输入)
    //   R6  = fragment.color_b (输入)
    //   R7  = fragment.color_a (输入)
    //   R8  = fragment.u (输入)
    //   R9  = fragment.v (输入)
    //   R10 = output.color_r
    //   R11 = output.color_g
    //   R12 = output.color_b
    //   R13 = output.color_a
    //   R14 = output.depth
    //   R15 = killed flag
    //   R16-R31 = 临时寄存器
    void executeFragment(FragmentContext& ctx, const ShaderFunction& shader);
    
    // 执行多个 fragment（批处理）
    void executeFragmentBatch(std::vector<FragmentContext>& fragments, 
                              const ShaderFunction& shader);
    
    // PHASE3: 执行 8 个 fragment 的 warp batch（SIMD 风格批量执行）
    void executeWarpBatch(std::array<FragmentContext, 8>& warpContexts,
                          const ShaderFunction& shader);
    
    // ========================================================================
    // Shader 加载
    // ========================================================================
    
    // 从指令序列加载 shader
    void loadShader(const ShaderFunction& shader);
    
    // 编译 GLSL-like 着色器为 ISA 指令序列（占位，待后续实现）
    ShaderFunction compileShader(const std::string& glsl_source);
    
    // ========================================================================
    // 内置 shader 函数
    // ========================================================================
    
    // 获取默认的 fragment shader（简单插值 + depth test）
    static ShaderFunction getDefaultFragmentShader();
    
    // ----------------------------------------------------------------
    // PHASE 3: Flat Color ISA Shader
    // 最简单的 shader，只输出固定颜色（从输入寄存器复制）
    // 寄存器：R4-R7(输入颜色) -> R10-R13(输出颜色), R3(输入depth) -> R14(输出depth)
    // ----------------------------------------------------------------
    static ShaderFunction getFlatColorShader(float r, float g, float b, float a);
    
    // ----------------------------------------------------------------
    // PHASE 4: Barycentric Color ISA Shader
    // 使用 MAD 指令进行颜色插值（用于 Scene002 RGB 插值场景）
    // 使用 R8=权重u, R9=权重v, R16-R23=临时寄存器
    // ----------------------------------------------------------------
    static ShaderFunction getBarycentricColorShader();
    
    // ----------------------------------------------------------------
    // PHASE 5: Depth Test ISA Shader
    // 读取 fragment depth 并与 TileBuffer 中的深度值比较
    // 使用 CMP 指令进行深度比较，设置 killed flag
    // ----------------------------------------------------------------
    static ShaderFunction getDepthTestShader();
    
    // ----------------------------------------------------------------
    // PHASE 6: Multi-Triangle ISA Shader
    // 组合多种操作：颜色插值 + depth test + kill
    // 用于 Scene005/006 多三角形场景
    // ----------------------------------------------------------------
    static ShaderFunction getMultiTriangleShader();

    // ----------------------------------------------------------------
    // PHASE 7: Texture Sampling ISA Shader
    // 使用 TEX 指令进行 2D 纹理采样
    // TEX Rd, Ra(u), Rb(v), Rc(tex_id) - 输出 RGBA 到 Rd..Rd+3
    // ----------------------------------------------------------------
    static ShaderFunction getTextureSamplingShader();

    // 从 PNG 文件加载纹理到指定 slot
    bool setTextureFromPNG(int slot, const std::string& filename);

    // ========================================================================
    // 状态查询
    // ========================================================================
    
    const Stats& getStats() const { return m_stats; }
    Stats& getStats() { return m_stats; }
    
    uint64_t getCycleCount() const { return m_stats.cycles_spent; }
    double getIPC() const { 
        return m_stats.cycles_spent > 0 ? 
            (double)m_stats.instructions_executed / m_stats.cycles_spent : 0.0; 
    }
    
    // ========================================================================
    // 配置
    // ========================================================================
    
    void setVerbose(bool v) { m_verbose = v; }
    bool isVerbose() const { return m_verbose; }
    
    // ========================================================================
    // 状态重置
    // ========================================================================
    
    void reset() {
        m_stats.reset();
        m_currentShader = ShaderFunction();  // Reconstruct to reset
    }

private:
    // 内部状态
    Interpreter m_interpreter;  // softgpu::isa::v2_5::Interpreter
    ShaderFunction m_currentShader;
    Stats m_stats;
    bool m_verbose = false;
    
    // P1-3: 纹理缓冲区（最多 4 个纹理）
    std::array<std::unique_ptr<TextureBuffer>, 4> m_textures;

    // 创建内置测试纹理
    void createBuiltinTestTexture();

    // Fragment 执行核心
    void executeFragmentInternal(FragmentContext& ctx, const ShaderFunction& shader);
    
    // 设置 fragment 输入到寄存器
    void setupFragmentInput(FragmentContext& ctx);
    
    // 读取寄存器输出
    void captureFragmentOutput(FragmentContext& ctx);
    
    // 单步执行 shader 指令
    bool stepShader();
    
    // 同步 interpreter 内存与 TileBuffer
    void syncMemoryWithTileBuffer(TileBufferManager* tile_buffer, uint32_t tile_idx);
};

// ============================================================================
// 辅助函数
// ============================================================================

// 创建简单的 fragment shader 代码
std::vector<uint32_t> makeFragmentShaderCode();

// 测试 shader 代码
std::vector<uint32_t> makeTestShaderCode();

} // namespace SoftGPU
