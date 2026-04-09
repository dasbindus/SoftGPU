#pragma once

#include "core/PipelineTypes.hpp"
#include <vector>
#include <cstddef>

namespace SoftGPU {

// ============================================================================
// VSOutputAssembler - Converts ISA flat buffers to std::vector<Vertex>
// ============================================================================
// Takes the raw output from the VS ISA interpreter:
//   - m_voutput_buf: clip positions (x,y,z,w per vertex)
//   - m_vattr_buf: attributes (r,g,b,a per vertex)
// And assembles them into a std::vector<Vertex> for PrimitiveAssembly.
// ============================================================================

struct VertexLayout {
    size_t stride_bytes = 32;       // VBO stride: x,y,z,w,r,g,b,a = 8 floats = 32 bytes
    size_t position_offset = 0;     // clip pos offset in VBO (bytes)
    size_t color_offset = 16;       // color offset in VBO (bytes)
};

class VSOutputAssembler {
public:
    VSOutputAssembler() = default;

    // Set input buffers
    void SetVOutputBuf(const float* data) { m_voutput_buf = data; }
    void SetAttrBuf(const float* data) { m_vattr_buf = data; }

    // Set vertex layout
    void SetVertexLayout(const VertexLayout& layout) { m_layout = layout; }

    // Assemble flat buffers into std::vector<Vertex>
    // voutput_buf: clip positions [vx0,vy0,vz0,vw0, vx1,vy1,vz1,vw1, ...]
    // vattr_buf:   attributes    [ar0,ag0,ab0,aa0, ar1,ag1,ab1,aa1, ...]
    // vertex_count: number of vertices to assemble
    std::vector<Vertex> Assemble(size_t vertex_count) const;

    // Get/Set vertex layout
    const VertexLayout& GetVertexLayout() const { return m_layout; }
    VertexLayout& GetVertexLayout() { return m_layout; }

private:
    const float* m_voutput_buf = nullptr;
    const float* m_vattr_buf = nullptr;
    VertexLayout m_layout;
};

} // namespace SoftGPU
