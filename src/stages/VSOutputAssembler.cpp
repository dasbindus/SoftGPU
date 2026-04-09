#include "VSOutputAssembler.hpp"

namespace SoftGPU {

std::vector<Vertex> VSOutputAssembler::Assemble(size_t vertex_count) const {
    std::vector<Vertex> vertices;
    vertices.reserve(vertex_count);

    for (size_t i = 0; i < vertex_count; ++i) {
        Vertex v;

        // Position from VOUTPUT buffer
        size_t pos_base = i * 4;
        v.x = m_voutput_buf ? m_voutput_buf[pos_base + 0] : 0.0f;
        v.y = m_voutput_buf ? m_voutput_buf[pos_base + 1] : 0.0f;
        v.z = m_voutput_buf ? m_voutput_buf[pos_base + 2] : 0.0f;
        v.w = m_voutput_buf ? m_voutput_buf[pos_base + 3] : 1.0f;

        // Attributes from VATTR buffer
        size_t attr_base = i * 4;
        v.r = m_vattr_buf ? m_vattr_buf[attr_base + 0] : 1.0f;
        v.g = m_vattr_buf ? m_vattr_buf[attr_base + 1] : 1.0f;
        v.b = m_vattr_buf ? m_vattr_buf[attr_base + 2] : 1.0f;
        v.a = m_vattr_buf ? m_vattr_buf[attr_base + 3] : 1.0f;

        // NDC (computed after perspective divide in PrimitiveAssembly)
        v.ndcX = 0.0f;
        v.ndcY = 0.0f;
        v.ndcZ = 0.0f;

        // Near-plane culling check
        v.culled = (v.w <= 0.0f);

        vertices.push_back(v);
    }

    return vertices;
}

} // namespace SoftGPU
