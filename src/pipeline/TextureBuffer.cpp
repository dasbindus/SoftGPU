// ============================================================================
// SoftGPU - TextureBuffer.cpp
// 纹理采样缓冲区实现
// ============================================================================

#include "pipeline/TextureBuffer.hpp"

namespace SoftGPU {

// ============================================================================
// TextureBuffer
// ============================================================================
TextureBuffer::TextureBuffer() : m_width(0), m_height(0) {}

void TextureBuffer::setData(uint32_t width, uint32_t height, const uint8_t* rgba_data) {
    m_width = width;
    m_height = height;
    m_texels.resize(width * height * 4);
    if (rgba_data) {
        std::copy(rgba_data, rgba_data + width * height * 4, m_texels.begin());
    }
}

float4 TextureBuffer::sampleNearest(float u, float v) const {
    if (!valid()) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    
    // Clamp UV to [0,1]
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    
    // Convert to texel coordinates
    int tx = static_cast<int>(std::floor(u * static_cast<float>(m_width)));
    int ty = static_cast<int>(std::floor(v * static_cast<float>(m_height)));
    
    // Clamp to valid range
    tx = std::clamp(tx, 0, static_cast<int>(m_width) - 1);
    ty = std::clamp(ty, 0, static_cast<int>(m_height) - 1);
    
    // Get texel (RGBA)
    size_t idx = (ty * m_width + tx) * 4;
    uint8_t r = m_texels[idx + 0];
    uint8_t g = m_texels[idx + 1];
    uint8_t b = m_texels[idx + 2];
    uint8_t a = m_texels[idx + 3];
    
    // Normalize to [0,1]
    return float4(
        static_cast<float>(r) / 255.0f,
        static_cast<float>(g) / 255.0f,
        static_cast<float>(b) / 255.0f,
        static_cast<float>(a) / 255.0f
    );
}

} // namespace SoftGPU
