// ============================================================================
// SoftGPU - PipelineTypes.hpp
// 管线数据类型定义
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace SoftGPU {

// ============================================================================
// 常量
// ============================================================================
constexpr uint32_t FRAMEBUFFER_WIDTH  = 640;
constexpr uint32_t FRAMEBUFFER_HEIGHT = 480;
constexpr uint32_t TILE_WIDTH         = 32;
constexpr uint32_t TILE_HEIGHT        = 32;
constexpr uint32_t VERTEX_STRIDE      = 8;    // floats per vertex (pos 4 + color 4)
constexpr float    CLEAR_DEPTH        = 1.0f; // 远平面

// ============================================================================
// Vertex - 顶点（流水线内使用）
// ============================================================================
struct Vertex {
    float x, y, z, w;   // Position (clip space after transform)
    float r, g, b, a;   // Color

    // NDC coordinates (computed after perspective divide)
    float ndcX, ndcY, ndcZ;

    // Screen coordinates (computed after viewport transform in PrimitiveAssembly)
    float screenX, screenY;

    // Near-plane culling flag (set by VertexShader if w <= 0)
    bool culled = false;
};

// ============================================================================
// Triangle - 三角形图元
// ============================================================================
struct Triangle {
    Vertex v[3];     // 三个顶点（NDC space）
    bool culled = false;
};

// ============================================================================
// ClipResult - 裁剪结果（用于近平面裁剪）
// ============================================================================
struct ClipResult {
    std::array<Vertex, 4> verts;  // 裁剪后最多 4 个顶点
    int count = 0;                  // 顶点数 (1-4)
};

// ============================================================================
// Fragment - 片段
// ============================================================================
struct Fragment {
    uint32_t x, y;       // 屏幕坐标（像素）
    float z;             // 深度值
    float r, g, b, a;    // 插值颜色
    float u, v;          // 纹理坐标（Phase1 不用）
};

// ============================================================================
// Uniforms - 着色器 Uniform
// ============================================================================
struct Uniforms {
    std::array<float, 16> modelMatrix{};      // 4x4 column-major
    std::array<float, 16> viewMatrix{};
    std::array<float, 16> projectionMatrix{};
    float viewportWidth  = 640.0f;
    float viewportHeight = 480.0f;
};

// ============================================================================
// PerformanceCounters - 性能计数器
// ============================================================================
struct PerformanceCounters {
    uint64_t cycle_count = 0;       // 周期计数
    uint64_t invocation_count = 0;   // 调用次数
    uint64_t extra_count0 = 0;       // 额外计数器0（如 culled_count）
    uint64_t extra_count1 = 0;       // 额外计数器1（如 fragment_count）
    double   elapsed_ms = 0.0;       // 耗时(ms)
};

}  // namespace SoftGPU
