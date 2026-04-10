// ============================================================================
// SoftGPU - VertexShader.cpp
// ============================================================================

#include "VertexShader.hpp"

#include <chrono>
#include <cmath>

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

namespace SoftGPU {

VertexShader::VertexShader() {
    resetCounters();
    // Default ATTR table: 0=pos(0), 1=normal(16), 2=color(16), 3=uv(24)
    m_attrTable = {0, 16, 16, 24};
}

void VertexShader::setInput(const std::vector<float>& vb,
                            const std::vector<uint32_t>& ib,
                            const Uniforms& uniforms) {
    m_vertexBuffer = vb;
    m_indexBuffer  = ib;
    m_uniforms     = uniforms;
}

void VertexShader::setVertexCount(size_t count) {
    m_vertexCount = count;
}

void VertexShader::SetProgram(const uint32_t* program, size_t word_count) {
    if (program && word_count > 0) {
        m_vsProgram.assign(program, program + word_count);
    } else {
        m_vsProgram.clear();
    }
}

void VertexShader::SetAttrLayout(const std::vector<size_t>& layout) {
    m_attrTable = layout;
    m_interpreter.SetAttrTable(layout);
}

void VertexShader::execute() {
    switch (m_execMode) {
    case VSExecutionMode::ISA:
        executeISA();
        break;
    case VSExecutionMode::CPP:
        executeCPPRef();
        break;
    case VSExecutionMode::Auto:
        if (HasProgram()) {
            executeISA();
        } else {
            executeCPPRef();
        }
        break;
    }
}

void VertexShader::executeISA() {
    auto start = std::chrono::high_resolution_clock::now();
    m_counters.invocation_count += m_vertexCount;

#if defined(__x86_64__)
    m_counters.cycle_count = __rdtsc();
#endif

    m_outputVertices.clear();
    m_outputVertices.reserve(m_vertexCount);

    if (m_vsProgram.empty()) {
        // No program — fallback to C++
        executeCPPRef();
    } else {
        // Set ATTR table and uniforms in interpreter (populates internal caches)
        m_interpreter.SetAttrTable(m_attrTable);
        m_interpreter.SetUniforms(
            m_uniforms.modelMatrix.data(),
            m_uniforms.viewMatrix.data(),
            m_uniforms.projectionMatrix.data()
        );
        m_interpreter.SetViewport(m_uniforms.viewportWidth, m_uniforms.viewportHeight);

        const size_t stride = VERTEX_STRIDE; // 8 floats per vertex

        for (size_t i = 0; i < m_vertexCount; ++i) {
            // Per-vertex VBO data pointer
            const float* vertex_data = m_vertexBuffer.data() + i * stride;
            size_t vertex_floats = stride; // floats per vertex

            // Reset interpreter per-vertex
            m_interpreter.Reset();
            m_interpreter.ResetVS();

            // Set VBO for this vertex (VLOAD reads from here)
            m_interpreter.SetVBO(vertex_data, vertex_floats);

            // NOTE: Uniforms are loaded in RunVertexProgram after Reset()
            // Run VS program until HALT
            m_interpreter.RunVertexProgram(m_vsProgram.data(), 1);

            // Collect output
            Vertex v;
            v.x = m_interpreter.GetVOutputFloat(0, 0);
            v.y = m_interpreter.GetVOutputFloat(0, 1);
            v.z = m_interpreter.GetVOutputFloat(0, 2);
            v.w = m_interpreter.GetVOutputFloat(0, 3);

            // Attributes from VATTR buffer
            v.r = m_interpreter.GetVAttrFloat(0, 0);
            v.g = m_interpreter.GetVAttrFloat(0, 1);
            v.b = m_interpreter.GetVAttrFloat(0, 2);
            v.a = m_interpreter.GetVAttrFloat(0, 3);

            // NDC (computed after perspective divide in PrimitiveAssembly)
            v.ndcX = 0.0f;
            v.ndcY = 0.0f;
            v.ndcZ = 0.0f;

            // Near-plane culling check
            v.culled = (v.w <= 0.0f);

            m_outputVertices.push_back(v);
        }
    }

#if defined(__x86_64__)
    m_counters.cycle_count = __rdtsc() - m_counters.cycle_count;
#endif

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void VertexShader::executeCPPRef() {
    m_counters.invocation_count += m_vertexCount;
    m_outputVertices.clear();
    m_outputVertices.reserve(m_vertexCount);

    const size_t stride = VERTEX_STRIDE; // 8 floats per vertex

    for (size_t i = 0; i < m_vertexCount; ++i) {
        const float* rawVertex = m_vertexBuffer.data() + i * stride;
        Vertex transformed = transformVertex(rawVertex);
        m_outputVertices.push_back(transformed);
    }
}

void VertexShader::resetCounters() {
    m_counters = PerformanceCounters{};
}

// Matrix indexing helper: element at (row, col) in column-major storage
// Column-major: array[col*4 + row]
// Row-major would be: array[row*4 + col]
static inline float M(const float* m, int row, int col) {
    return m[col * 4 + row];
}

Vertex VertexShader::transformVertex(const float* rawVertex) const {
    // rawVertex layout: x, y, z, w, r, g, b, a
    Vertex v;
    v.x = rawVertex[0];
    v.y = rawVertex[1];
    v.z = rawVertex[2];
    v.w = rawVertex[3];
    v.r = rawVertex[4];
    v.g = rawVertex[5];
    v.b = rawVertex[6];
    v.a = rawVertex[7];

    // GLM uses column-major storage: M[row, col] = m[col*4 + row]
    const float* m_mat = m_uniforms.modelMatrix.data();
    const float* v_mat = m_uniforms.viewMatrix.data();
    const float* p_mat = m_uniforms.projectionMatrix.data();

    // MVP = P * V * M
    // Vertex transformation: clip = MVP * v (as column vector)
    //
    // In column-major GLM:
    // clip.x = P[0]*V[0]*M[0]*vx + P[0]*V[0]*M[4]*vy + ... (full sum)
    //
    // More efficiently: compute clip = P * (V * (M * v))
    // Step 1: t = M * v
    float t0 = M(m_mat, 0, 0) * v.x + M(m_mat, 0, 1) * v.y +
               M(m_mat, 0, 2) * v.z + M(m_mat, 0, 3) * v.w;  // t.x
    float t1 = M(m_mat, 1, 0) * v.x + M(m_mat, 1, 1) * v.y +
               M(m_mat, 1, 2) * v.z + M(m_mat, 1, 3) * v.w;  // t.y
    float t2 = M(m_mat, 2, 0) * v.x + M(m_mat, 2, 1) * v.y +
               M(m_mat, 2, 2) * v.z + M(m_mat, 2, 3) * v.w;  // t.z
    float t3 = M(m_mat, 3, 0) * v.x + M(m_mat, 3, 1) * v.y +
               M(m_mat, 3, 2) * v.z + M(m_mat, 3, 3) * v.w;  // t.w

    // Step 2: u = V * t
    float u0 = M(v_mat, 0, 0) * t0 + M(v_mat, 0, 1) * t1 +
               M(v_mat, 0, 2) * t2 + M(v_mat, 0, 3) * t3;  // u.x
    float u1 = M(v_mat, 1, 0) * t0 + M(v_mat, 1, 1) * t1 +
               M(v_mat, 1, 2) * t2 + M(v_mat, 1, 3) * t3;  // u.y
    float u2 = M(v_mat, 2, 0) * t0 + M(v_mat, 2, 1) * t1 +
               M(v_mat, 2, 2) * t2 + M(v_mat, 2, 3) * t3;  // u.z
    float u3 = M(v_mat, 3, 0) * t0 + M(v_mat, 3, 1) * t1 +
               M(v_mat, 3, 2) * t2 + M(v_mat, 3, 3) * t3;  // u.w

    // Step 3: clip = P * u
    Vertex out;
    out.x = M(p_mat, 0, 0) * u0 + M(p_mat, 0, 1) * u1 +
            M(p_mat, 0, 2) * u2 + M(p_mat, 0, 3) * u3;  // clip.x
    out.y = M(p_mat, 1, 0) * u0 + M(p_mat, 1, 1) * u1 +
            M(p_mat, 1, 2) * u2 + M(p_mat, 1, 3) * u3;  // clip.y
    out.z = M(p_mat, 2, 0) * u0 + M(p_mat, 2, 1) * u1 +
            M(p_mat, 2, 2) * u2 + M(p_mat, 2, 3) * u3;  // clip.z
    out.w = M(p_mat, 3, 0) * u0 + M(p_mat, 3, 1) * u1 +
            M(p_mat, 3, 2) * u2 + M(p_mat, 3, 3) * u3;  // clip.w

    // Near-plane culling: clip space w must be positive
    if (out.w <= 0.0f) {
        out.culled = true;
        return out;
    }

    // Copy color (passthrough)
    out.r = v.r;
    out.g = v.g;
    out.b = v.b;
    out.a = v.a;

    // NDC (will be computed in PA after perspective divide)
    out.ndcX = 0.0f;
    out.ndcY = 0.0f;
    out.ndcZ = 0.0f;

    return out;
}

}  // namespace SoftGPU
