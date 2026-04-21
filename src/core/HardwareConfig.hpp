// ============================================================================
// SoftGPU - HardwareConfig.hpp
// 硬件配置寄存器
// ============================================================================

#pragma once

#include <cstdint>

namespace SoftGPU {

// ============================================================================
// DepthFunc - 深度测试函数
// ============================================================================
enum DepthFunc : uint8_t {
    DEPTH_NEVER = 0,
    DEPTH_LESS = 1,      // Z < Zbuffer (默认)
    DEPTH_EQUAL = 2,
    DEPTH_LEQUAL = 3,    // Z <= Zbuffer
    DEPTH_GREATER = 4,
    DEPTH_NOTEQUAL = 5,
    DEPTH_GEQUAL = 6,    // Z >= Zbuffer
    DEPTH_ALWAYS = 7
};

// ============================================================================
// CullFace - 剔除面
// ============================================================================
enum CullFace : uint8_t {
    CULL_NONE = 0,       // 不剔除任何面
    CULL_FRONT = 1,
    CULL_BACK = 2,       // 默认：剔除背面
    CULL_FRONT_AND_BACK = 3
};

// ============================================================================
// HardwareConfig - 硬件配置寄存器
// 所有硬件阶段的配置参数
// ============================================================================
struct HardwareConfig {
    // ========================================================================
    // Primitive Assembly 配置
    // ========================================================================
    struct {
        bool enable = false;              // 是否启用背面剔除
        bool cullBack = true;             // true=剔除背面，false=剔除正面
        bool frontFaceCCW = true;         // true=CCW为正面，false=CW为正面
    } primitiveAssembly;

    // ========================================================================
    // Rasterizer 配置
    // ========================================================================
    struct {
        bool MSAA_Enable = false;         // 是否启用 MSAA
        uint8_t sampleCount = 1;           // 采样数 (1, 2, 4, 8)
    } rasterizer;

    // ========================================================================
    // FragmentShader 配置
    // ========================================================================
    struct {
        bool earlyZ_Enable = true;         // 是否启用 Early-Z
    } fragmentShader;

    // ========================================================================
    // Framebuffer 配置
    // ========================================================================
    struct {
        bool depthTestEnable = true;       // 是否启用深度测试
        DepthFunc depthFunc = DEPTH_LESS; // 深度测试函数
        bool depthWriteEnable = true;      // 是否启用深度写入
        bool blendEnable = false;          // 是否启用混合
    } framebuffer;

    // ========================================================================
    // 默认配置（兼容现有行为）
    // ========================================================================
    static HardwareConfig defaultConfig() {
        HardwareConfig cfg;
        cfg.primitiveAssembly.enable = false;    // 默认禁用，兼容测试
        cfg.primitiveAssembly.cullBack = true;
        cfg.primitiveAssembly.frontFaceCCW = true;
        cfg.rasterizer.MSAA_Enable = false;
        cfg.rasterizer.sampleCount = 1;
        cfg.fragmentShader.earlyZ_Enable = true;
        cfg.framebuffer.depthTestEnable = true;
        cfg.framebuffer.depthFunc = DEPTH_LESS;
        cfg.framebuffer.depthWriteEnable = true;
        cfg.framebuffer.blendEnable = false;
        return cfg;
    }
};

}  // namespace SoftGPU
