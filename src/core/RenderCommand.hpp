// ============================================================================
// SoftGPU - RenderCommand.hpp
// 渲染命令结构体
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

#include "HardwareConfig.hpp"

namespace SoftGPU {

// ============================================================================
// DrawParams - 绘制参数
// ============================================================================

// 图元类型
enum class PrimitiveType : uint8_t {
    TRIANGLE_LIST = 0,
    TRIANGLE_STRIP = 1
};

struct DrawParams {
    uint32_t vertexCount = 0;      // 顶点数
    uint32_t firstVertex = 0;      // 起始顶点偏移
    uint32_t indexCount = 0;        // 索引数（0=无索引）
    uint32_t firstIndex = 0;       // 起始索引偏移
    bool indexed = false;           // 是否使用索引
    PrimitiveType primitiveType = PrimitiveType::TRIANGLE_LIST;  // 图元类型
    uint32_t primitiveRestartIndex = 0xFFFFFFFF;  // Primitive restart 索引
    bool primitiveRestartEnabled = false;         // 是否启用 primitive restart
};

// ============================================================================
// RenderCommand - 渲染命令
// ============================================================================
struct RenderCommand {
    // Vertex Buffer
    const float* vertexBufferData = nullptr;  // 指针（外部数据）
    size_t       vertexBufferSize = 0;         // float 数量

    // Index Buffer
    const uint32_t* indexBufferData = nullptr;
    size_t          indexBufferSize = 0;

    // Uniforms
    std::array<float, 16> modelMatrix{};
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};

    // Draw params
    DrawParams drawParams;

    // Hardware configuration registers
    HardwareConfig hwConfig = HardwareConfig::defaultConfig();

    // Clear color
    std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
};

}  // namespace SoftGPU
