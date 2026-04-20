// ============================================================================
// SoftGPU - OBJModelScene.cpp
// OBJ Model Loading Test Scene Implementation
// ============================================================================

#include "OBJModelScene.hpp"
#include <utils/OBJLoader.hpp>
#include <core/PipelineTypes.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace SoftGPU {
using vec3 = glm::vec3;

OBJModelScene::OBJModelScene(const std::string& objFilepath)
    : TestScene("OBJ-Model", "OBJ model: " + objFilepath)
    , m_objFilepath(objFilepath)
{
    // Load OBJ file
    OBJLoader loader;
    if (loader.load(objFilepath)) {
        const auto& mesh = loader.getMesh();
        m_vertices = mesh.toSimpleVertices();
        m_triangleCount = mesh.getTriangleCount();
        m_loaded = true;
        m_description = "OBJ model: " + objFilepath + " (" + std::to_string(m_triangleCount) + " triangles)";
    } else {
        m_error = "Failed to load OBJ: " + loader.getError();
        m_description = m_error;
    }
}

void OBJModelScene::buildRenderCommand(RenderCommand& outCommand) {
    if (!m_loaded || m_vertices.empty()) {
        // Return empty command
        outCommand.vertexBufferData = nullptr;
        outCommand.vertexBufferSize = 0;
        outCommand.drawParams.vertexCount = 0;
        return;
    }

    outCommand.vertexBufferData = m_vertices.data();
    outCommand.vertexBufferSize = m_vertices.size();
    outCommand.indexBufferData = nullptr;
    outCommand.indexBufferSize = 0;
    outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 8);  // 8 floats per vertex
    outCommand.drawParams.firstVertex = 0;
    outCommand.drawParams.indexed = false;

    // Use perspective projection similar to TriangleCubeScene
    float aspect = 640.0f / 480.0f;
    float fovY = 60.0f * 3.14159f / 180.0f;
    float f = 1.0f / tanf(fovY * 0.5f);
    float near = 0.1f, far = 100.0f;

    outCommand.projectionMatrix = {
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, (far + near) / (near - far), -1.0f,
        0.0f, 0.0f, (2.0f * far * near) / (near - far), 0.0f
    };

    // Check if this is a teapot model
    bool isTeapot = (m_objFilepath.find("teapot") != std::string::npos);

    // View matrix: camera at (4.0, 2.5, 4.0) looking at object center
    // For cube: center at y=0 (object centered at origin)
    // For teapot: center at y=1.575 (object center from teapot geometry)
    vec3 eye = {4.0f, 2.5f, 4.0f};
    vec3 center = isTeapot ? vec3{0.0f, 1.575f, 0.0f} : vec3{0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};

    vec3 f_vec = normalize(center - eye);
    vec3 s = normalize(cross(f_vec, up));
    vec3 u = cross(s, f_vec);

    outCommand.viewMatrix = {
        s.x, u.x, -f_vec.x, 0.0f,
        s.y, u.y, -f_vec.y, 0.0f,
        s.z, u.z, -f_vec.z, 0.0f,
        -dot(s, eye), -dot(u, eye), dot(f_vec, eye), 1.0f
    };

    // Model matrix
    // For teapot: rotate 30 degrees CW around Y-axis
    // For cube: identity (no rotation)
    if (isTeapot) {
        float cosA = 0.866f;
        float sinA = 0.5f;
        outCommand.modelMatrix = {
            cosA, 0.0f, -sinA, 0.0f,  // x' = cosA*x - sinA*z
            0.0f, 1.0f, 0.0f, 0.0f,   // y' = y
            sinA, 0.0f, cosA, 0.0f,    // z' = sinA*x + cosA*z
            0.0f, 0.0f, 0.0f, 1.0f
        };
    } else {
        // Identity matrix for cube
        outCommand.modelMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    }

    outCommand.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
}

// Helper function declarations for math operations
namespace {
    inline vec3 normalize(const vec3& v) {
        float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 0.0001f) return {0.0f, 0.0f, 0.0f};
        return {v.x / len, v.y / len, v.z / len};
    }

    inline vec3 cross(const vec3& a, const vec3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float dot(const vec3& a, const vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline vec3 operator-(const vec3& a, const vec3& b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
}

std::shared_ptr<TestScene> createOBJModelScene(const std::string& objFilepath) {
    return std::make_shared<OBJModelScene>(objFilepath);
}

} // namespace SoftGPU
