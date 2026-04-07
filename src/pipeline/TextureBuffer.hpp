// ============================================================================
// SoftGPU - TextureBuffer.hpp
// 纹理采样缓冲区
// Phase 1: NEAREST 采样（框架）
// Phase 2: 实现真实纹理采样
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

namespace SoftGPU {

// ============================================================================
// float4 - 简单的 RGBA 颜色类型
// ============================================================================
struct float4 {
    float r, g, b, a;
    
    float4() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    float4(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}
};

// ============================================================================
// TextureBuffer - 2D 纹理采样器
// ============================================================================
class TextureBuffer {
public:
    TextureBuffer();
    
    // 设置纹理数据（RGBA uint8）
    void setData(uint32_t width, uint32_t height, const uint8_t* rgba_data);
    
    // NEAREST 采样：u,v in [0,1]
    float4 sampleNearest(float u, float v) const;
    
    // 获取纹理尺寸
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    bool valid() const { return m_width > 0 && m_height > 0 && !m_texels.empty(); }

    // 从 PNG 文件加载纹理数据
    bool loadFromPNG(const std::string& filename);
    
private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::vector<uint8_t> m_texels;  // RGBA uint8
};

} // namespace SoftGPU
