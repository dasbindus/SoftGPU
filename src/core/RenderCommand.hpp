// ============================================================================
// SoftGPU - RenderCommand.hpp
// 渲染命令结构体
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace SoftGPU {

// ============================================================================
// DrawParams - 绘制参数
// ============================================================================
struct DrawParams {
    uint32_t vertexCount = 0;      // 顶点数
    uint32_t firstVertex = 0;      // 起始顶点偏移
    uint32_t indexCount = 0;        // 索引数（0=无索引）
    uint32_t firstIndex = 0;       // 起始索引偏移
    bool indexed = false;           // 是否使用索引

    // 渲染状态
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    bool cullEnabled = false;      // Phase1 关闭 cull
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

    // Clear color
    std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
};

}  // namespace SoftGPU
