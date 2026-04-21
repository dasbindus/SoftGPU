// ============================================================================
// SoftGPU - PrimitiveAssembly.cpp
// ============================================================================

#include "PrimitiveAssembly.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>

namespace SoftGPU {

PrimitiveAssembly::PrimitiveAssembly() : m_indexed(false) {
    resetCounters();
}

void PrimitiveAssembly::setInput(const std::vector<Vertex>& vertices,
                                 const std::vector<uint32_t>& indices,
                                 bool indexed) {
    m_inputVertices = vertices;
    m_inputIndices  = indices;
    m_indexed = indexed;
}

void PrimitiveAssembly::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    m_outputTriangles.clear();

    // 根据 indexed 标志决定组装方式
    if (!m_indexed) {
        // 非索引绘制：使用顶点顺序组装三角形（每3个顶点一个三角形）
        size_t triCount = m_inputVertices.size() / 3;
        for (size_t i = 0; i < triCount; ++i) {
            Triangle tri;
            tri.v[0] = m_inputVertices[i * 3 + 0];
            tri.v[1] = m_inputVertices[i * 3 + 1];
            tri.v[2] = m_inputVertices[i * 3 + 2];

            // 检查顶点是否被 VertexShader 标记为 culled（near-plane 剔除）
            tri.culled = tri.v[0].culled || tri.v[1].culled || tri.v[2].culled;
            m_outputTriangles.push_back(tri);
        }
    } else {
        // 索引绘制：使用索引缓冲
        size_t triCount = m_inputIndices.size() / 3;
        for (size_t i = 0; i < triCount; ++i) {
            Triangle tri;
            tri.v[0] = m_inputVertices[m_inputIndices[i * 3 + 0]];
            tri.v[1] = m_inputVertices[m_inputIndices[i * 3 + 1]];
            tri.v[2] = m_inputVertices[m_inputIndices[i * 3 + 2]];

            // 检查顶点是否被 VertexShader 标记为 culled（near-plane 剔除）
            tri.culled = tri.v[0].culled || tri.v[1].culled || tri.v[2].culled;
            m_outputTriangles.push_back(tri);
        }
    }

    // 透视除法和视锥剔除
    size_t culled = 0;
    for (auto& tri : m_outputTriangles) {
        // 如果已经被标记为 culled（near-plane 剔除），跳过 NDC 计算
        if (tri.culled) {
            culled++;
            continue;
        }

        // 透视除法得到 NDC
        computeNDC(tri.v[0]);
        computeNDC(tri.v[1]);
        computeNDC(tri.v[2]);

        // 视锥剔除
        if (shouldCull(tri)) {
            tri.culled = true;
            culled++;
            continue;
        }

        // 背面剔除（仅当启用时）
        if (m_config.primitiveAssembly.enable && shouldCullBackFace(tri)) {
            tri.culled = true;
            culled++;
        }
    }

    m_counters.invocation_count = m_outputTriangles.size();
    m_counters.extra_count0 = culled;  // culled count

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void PrimitiveAssembly::resetCounters() {
    m_counters = PerformanceCounters{};
}

void PrimitiveAssembly::computeNDC(Vertex& v) const {
    // Perspective divide: NDC = clip / w
    if (std::abs(v.w) > 1e-8f) {
        float invW = 1.0f / v.w;
        v.ndcX = v.x * invW;
        v.ndcY = v.y * invW;
        v.ndcZ = v.z * invW;
    } else {
        v.ndcX = v.x;
        v.ndcY = v.y;
        v.ndcZ = v.z;
    }
}

bool PrimitiveAssembly::shouldCull(const Triangle& tri) const {
    // AABB 视锥剔除
    float minX = std::min({tri.v[0].ndcX, tri.v[1].ndcX, tri.v[2].ndcX});
    float maxX = std::max({tri.v[0].ndcX, tri.v[1].ndcX, tri.v[2].ndcX});
    float minY = std::min({tri.v[0].ndcY, tri.v[1].ndcY, tri.v[2].ndcY});
    float maxY = std::max({tri.v[0].ndcY, tri.v[1].ndcY, tri.v[2].ndcY});
    float minZ = std::min({tri.v[0].ndcZ, tri.v[1].ndcZ, tri.v[2].ndcZ});
    float maxZ = std::max({tri.v[0].ndcZ, tri.v[1].ndcZ, tri.v[2].ndcZ});

    // NDC 范围 [-1, 1]
    // 完全在左侧或右侧
    if (maxX < -1.0f || minX > 1.0f) return true;
    // 完全在上方或下方
    if (maxY < -1.0f || minY > 1.0f) return true;
    // 完全在近平面之前或远平面之后
    if (maxZ < -1.0f || minZ > 1.0f) return true;

    return false;
}

bool PrimitiveAssembly::shouldCullBackFace(const Triangle& tri) const {
    // 转换为屏幕坐标
    float sx0 = (tri.v[0].ndcX + 1.0f) * 0.5f * m_viewportWidth;
    float sy0 = (1.0f - tri.v[0].ndcY) * 0.5f * m_viewportHeight;
    float sx1 = (tri.v[1].ndcX + 1.0f) * 0.5f * m_viewportWidth;
    float sy1 = (1.0f - tri.v[1].ndcY) * 0.5f * m_viewportHeight;
    float sx2 = (tri.v[2].ndcX + 1.0f) * 0.5f * m_viewportWidth;
    float sy2 = (1.0f - tri.v[2].ndcY) * 0.5f * m_viewportHeight;

    // 有符号面积法 (edge function)
    float area = (sx1 - sx0) * (sy2 - sy0) - (sy1 - sy0) * (sx2 - sx0);

    // 根据正面朝向和剔除模式决定是否剔除
    // m_config.primitiveAssembly.frontFaceCCW: true=CCW为正面
    // m_config.primitiveAssembly.cullBack: true=剔除背面

    if (m_config.primitiveAssembly.cullBack) {
        // 剔除背面：CCW (area > 0) 为正面，所以 area < 0 时是背面
        return area < 0.0f;
    } else {
        // 剔除正面：CW (area < 0) 为正面，所以 area > 0 时是正面
        return area > 0.0f;
    }
}

}  // namespace SoftGPU
