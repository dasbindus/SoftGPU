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

    // Triangle Strip 模式
    if (m_primitiveType == PrimitiveType::TRIANGLE_STRIP) {
        assembleTriangleStrip(m_inputVertices, m_outputTriangles);
    }
    // 非索引绘制
    else if (!m_indexed) {
        // 使用顶点顺序组装三角形（每3个顶点一个三角形）或 triangle list
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
    }
    // 索引绘制
    else {
        // 索引绘制：使用索引缓冲
        size_t triCount = m_inputIndices.size() / 3;
        size_t vertexStart = 0;

        for (size_t i = 0; i < triCount; ++i) {
            // 检查 primitive restart
            if (m_primitiveRestartEnabled && i > 0) {
                uint32_t i0 = m_inputIndices[i * 3 + 0];
                uint32_t i1 = m_inputIndices[i * 3 + 1];
                uint32_t i2 = m_inputIndices[i * 3 + 2];
                if (isRestartIndex(i0) || isRestartIndex(i1) || isRestartIndex(i2)) {
                    vertexStart = m_inputVertices.size();  // 重置，开始新序列
                    continue;
                }
            }

            // 检查索引是否有效
            uint32_t idx0 = m_inputIndices[i * 3 + 0];
            uint32_t idx1 = m_inputIndices[i * 3 + 1];
            uint32_t idx2 = m_inputIndices[i * 3 + 2];

            if (idx0 >= m_inputVertices.size() || idx1 >= m_inputVertices.size() || idx2 >= m_inputVertices.size()) {
                continue;  // 跳过无效索引
            }

            Triangle tri;
            tri.v[0] = m_inputVertices[idx0];
            tri.v[1] = m_inputVertices[idx1];
            tri.v[2] = m_inputVertices[idx2];

            // 检查顶点是否被 VertexShader 标记为 culled（near-plane 剔除）
            tri.culled = tri.v[0].culled || tri.v[1].culled || tri.v[2].culled;
            m_outputTriangles.push_back(tri);
        }
    }

    // 透视除法和视锥剔除
    size_t culled = 0;
    size_t clippedTriangles = 0;
    std::vector<Triangle> clippedResults;  // 存储裁剪产生的新三角形

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
            continue;
        }

        // 近平面裁剪
        ClipResult clip = clipAgainstNearPlane(tri);
        if (clip.count == 0) {
            // 三角形完全在近平面之外
            tri.culled = true;
            culled++;
            continue;
        } else if (clip.count == 3) {
            // 无需裁剪，三角形保留
            // tri stays as is (not culled)
        } else {
            // 需要裁剪，生成 1-2 个三角形
            // 标记原三角形为 culled
            tri.culled = true;
            culled++;
            // 收集裁剪产生的三角形（稍后添加）
            triangulateClipResult(clip, clippedResults);
            clippedTriangles++;
        }
    }

    // 将裁剪产生的三角形添加到输出
    for (auto& tri : clippedResults) {
        m_outputTriangles.push_back(tri);
    }

    m_counters.invocation_count = m_outputTriangles.size();
    m_counters.extra_count0 = culled;       // culled count
    m_counters.extra_count1 = clippedTriangles;  // clipped triangles count

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

    // 退化三角形 (面积为 0) 也要剔除
    constexpr float EPSILON = 1e-6f;
    if (std::abs(area) < EPSILON) {
        return true;
    }

    if (m_config.primitiveAssembly.cullBack) {
        // 剔除背面：CCW (area > 0) 为正面，所以 area < 0 时是背面
        return area < 0.0f;
    } else {
        // 剔除正面：CW (area < 0) 为正面，所以 area > 0 时是正面
        return area > 0.0f;
    }
}

bool PrimitiveAssembly::isRestartIndex(uint32_t idx) const {
    return m_primitiveRestartEnabled && (idx == m_primitiveRestartIndex || idx == 0xFFFFFFFF);
}

ClipResult PrimitiveAssembly::clipAgainstNearPlane(const Triangle& tri) const {
    ClipResult result;
    result.count = 0;

    // 近平面: ndcZ = -1
    constexpr float NEAR_Z = -1.0f;

    for (int i = 0; i < 3; i++) {
        const Vertex& curr = tri.v[i];
        const Vertex& next = tri.v[(i + 1) % 3];

        bool currInside = curr.ndcZ >= NEAR_Z;
        bool nextInside = next.ndcZ >= NEAR_Z;

        if (currInside) {
            result.verts[result.count++] = curr;
        }

        if (currInside != nextInside) {
            // 边与近平面相交，计算交点
            float t = (NEAR_Z - curr.ndcZ) / (next.ndcZ - curr.ndcZ);
            Vertex intersection = interpolateVertex(curr, next, t);
            result.verts[result.count++] = intersection;
        }
    }

    return result;
}

Vertex PrimitiveAssembly::interpolateVertex(const Vertex& v0, const Vertex& v1, float t) const {
    Vertex result;
    result.x = v0.x + t * (v1.x - v0.x);
    result.y = v0.y + t * (v1.y - v0.y);
    result.z = v0.z + t * (v1.z - v0.z);
    result.w = v0.w + t * (v1.w - v0.w);
    result.ndcX = v0.ndcX + t * (v1.ndcX - v0.ndcX);
    result.ndcY = v0.ndcY + t * (v1.ndcY - v0.ndcY);
    result.ndcZ = v0.ndcZ + t * (v1.ndcZ - v0.ndcZ);
    result.r = v0.r + t * (v1.r - v0.r);
    result.g = v0.g + t * (v1.g - v0.g);
    result.b = v0.b + t * (v1.b - v0.b);
    result.a = v0.a + t * (v1.a - v0.a);
    result.culled = false;
    return result;
}

void PrimitiveAssembly::triangulateClipResult(const ClipResult& clip, std::vector<Triangle>& output) const {
    if (clip.count < 3) return;

    if (clip.count == 3) {
        // 无需裁剪
        Triangle tri;
        tri.v[0] = clip.verts[0];
        tri.v[1] = clip.verts[1];
        tri.v[2] = clip.verts[2];
        tri.culled = false;
        output.push_back(tri);
    } else if (clip.count == 4) {
        // 裁剪生成 4 个顶点，组成 2 个三角形
        Triangle tri1;
        tri1.v[0] = clip.verts[0];
        tri1.v[1] = clip.verts[1];
        tri1.v[2] = clip.verts[2];
        tri1.culled = false;
        output.push_back(tri1);

        Triangle tri2;
        tri2.v[0] = clip.verts[0];
        tri2.v[1] = clip.verts[2];
        tri2.v[2] = clip.verts[3];
        tri2.culled = false;
        output.push_back(tri2);
    }
}

void PrimitiveAssembly::assembleTriangleStrip(const std::vector<Vertex>& vertices, std::vector<Triangle>& output) {
    if (vertices.size() < 3) return;

    size_t n = vertices.size();
    for (size_t i = 0; i + 2 < n; ++i) {
        Triangle tri;

        // Triangle strip: alternating winding order
        if (i % 2 == 0) {
            tri.v[0] = vertices[i];
            tri.v[1] = vertices[i + 1];
            tri.v[2] = vertices[i + 2];
        } else {
            tri.v[0] = vertices[i];
            tri.v[1] = vertices[i + 2];
            tri.v[2] = vertices[i + 1];
        }

        tri.culled = tri.v[0].culled || tri.v[1].culled || tri.v[2].culled;
        output.push_back(tri);
    }
}

}  // namespace SoftGPU
