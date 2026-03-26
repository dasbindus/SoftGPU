// ============================================================================
// SoftGPU - TestScene.cpp
// 测试场景实现
// PHASE4
// ============================================================================

#include "TestScene.hpp"
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// Use glm directly to avoid constexpr issues with older glm versions
#ifndef PI
#define PI 3.14159265358979f
#endif
#ifndef TWO_PI
#define TWO_PI (2.0f * PI)
#endif
#ifndef HALF_PI
#define HALF_PI (PI * 0.5f)
#endif

namespace SoftGPU {

using mat4 = glm::mat4;
using vec3 = glm::vec3;
using quat = glm::quat;

inline float radians(float degrees) { return degrees * PI / 180.0f; }
inline mat4 perspective(float fovY, float aspect, float near, float far) {
    return glm::perspective(fovY, aspect, near, far);
}
inline mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
    return glm::lookAt(eye, center, up);
}
inline mat4 identity() { return mat4(1.0f); }
inline mat4 translate(const vec3& t) { return glm::translate(mat4(1.0f), t); }
inline mat4 rotate(float radians, const vec3& axis) { return glm::rotate(mat4(1.0f), radians, axis); }
inline mat4 scale(const vec3& s) { return glm::scale(mat4(1.0f), s); }
inline vec3 normalize(const vec3& v) { return glm::normalize(v); }

// ============================================================================
// Helper: 创建简单立方体的8个顶点
// ============================================================================
static void createCubeVertices(std::vector<float>& vertices, float size = 1.0f) {
    float h = size * 0.5f;
    // 8 vertices of cube, each with position (x,y,z,w) + color (r,g,b,a)
    // Using separate triangles for each face (12 triangles total)
    // Format per vertex: x, y, z, w, r, g, b, a (8 floats)

    // Reset
    vertices.clear();

    // Front face (z = +h) - Green
    // v0: (-h, -h, +h), v1: (+h, -h, +h), v2: (+h, +h, +h), v3: (-h, +h, +h)
    // Triangle 1
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    // Triangle 2
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});

    // Back face (z = -h) - Blue
    // v4: (-h, -h, -h), v5: (+h, -h, -h), v6: (+h, +h, -h), v7: (-h, +h, -h)
    // Triangle 3
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    // Triangle 4
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});

    // Left face (x = -h) - Red
    // Triangle 5
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    // Triangle 6
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});

    // Right face (x = +h) - Yellow
    // Triangle 7
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    // Triangle 8
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});

    // Top face (y = +h) - Magenta
    // Triangle 9
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    // Triangle 10
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});

    // Bottom face (y = -h) - Cyan
    // Triangle 11
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    // Triangle 12
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
}

// ============================================================================
// Triangle-1Tri Scene: 单绿色三角形
// ============================================================================
class Triangle1TriScene : public TestScene {
public:
    Triangle1TriScene() : TestScene("Triangle-1Tri", "Single green triangle") {
        // 1 triangle = 3 vertices
        // Format: x, y, z, w, r, g, b, a (8 floats per vertex)
        m_vertices = {
            -0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,  // bottom-left, green
             0.5f, -0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f,  // bottom-right, green
             0.0f,  0.5f, 0.0f, 1.0f,   0.0f, 1.0f, 0.0f, 1.0f   // top, green
        };
    }

    uint32_t getTriangleCount() const override { return 1; }

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override {
        outCommand.vertexBufferData = m_vertices.data();
        outCommand.vertexBufferSize = m_vertices.size();
        outCommand.indexBufferData = nullptr;
        outCommand.indexBufferSize = 0;
        outCommand.drawParams.vertexCount = 3;
        outCommand.drawParams.firstVertex = 0;
        outCommand.drawParams.indexed = false;

        // 单位矩阵 (与 test_Integration 相同)
        outCommand.viewMatrix = std::array<float, 16>{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // 单位投影矩阵
        outCommand.projectionMatrix = std::array<float, 16>{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        // 单位模型矩阵
        outCommand.modelMatrix = std::array<float, 16>{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        outCommand.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    }

    const std::vector<float>& getVertexData() const override { return m_vertices; }

private:
    std::vector<float> m_vertices;
};

// ============================================================================
// Triangle-Cube Scene: 立方体 (12 triangles)
// ============================================================================
class TriangleCubeScene : public TestScene {
public:
    TriangleCubeScene() : TestScene("Triangle-Cube", "Single cube with 12 triangles") {
        createCubeVertices(m_vertices, 1.0f);
    }

    uint32_t getTriangleCount() const override { return 12; }

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override {
        outCommand.vertexBufferData = m_vertices.data();
        outCommand.vertexBufferSize = m_vertices.size();
        outCommand.indexBufferData = nullptr;
        outCommand.indexBufferSize = 0;
        outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 8);
        outCommand.drawParams.firstVertex = 0;
        outCommand.drawParams.indexed = false;

        // 透视投影
        float aspect = 640.0f / 480.0f;
        mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
        mat4 view = lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 model = identity();

        // Copy to array (column-major)
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                outCommand.projectionMatrix[i * 4 + j] = proj[i][j];
                outCommand.viewMatrix[i * 4 + j] = view[i][j];
                outCommand.modelMatrix[i * 4 + j] = model[i][j];
            }
        }

        outCommand.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
    }

    const std::vector<float>& getVertexData() const override { return m_vertices; }

private:
    std::vector<float> m_vertices;
};

// ============================================================================
// Triangle-Cubes-100 Scene: 100个立方体 (1200 triangles)
// ============================================================================
class TriangleCubes100Scene : public TestScene {
public:
    TriangleCubes100Scene() : TestScene("Triangle-Cubes-100", "100 cubes with 1200 triangles") {
        // 创建一个立方体的模板
        std::vector<float> cubeTemplate;
        createCubeVertices(cubeTemplate, 0.8f);

        // 10x10 grid of cubes
        float spacing = 2.0f;
        float offset = -9.0f; // center the grid

        for (int x = 0; x < 10; x++) {
            for (int z = 0; z < 10; z++) {
                float px = offset + x * spacing;
                float pz = offset + z * spacing;

                // Add 36 vertices per cube (12 triangles * 3 vertices)
                // Format per vertex: x, y, z, w, r, g, b, a (8 floats)
                for (size_t i = 0; i < cubeTemplate.size(); i += 8) {
                    float vx = cubeTemplate[i] + px;
                    float vy = cubeTemplate[i + 1];
                    float vz = cubeTemplate[i + 2] + pz;
                    float vw = cubeTemplate[i + 3];
                    float vr = cubeTemplate[i + 4];
                    float vg = cubeTemplate[i + 5];
                    float vb = cubeTemplate[i + 6];
                    float va = cubeTemplate[i + 7];

                    m_vertices.insert(m_vertices.end(), {vx, vy, vz, vw, vr, vg, vb, va});
                }
            }
        }
    }

    uint32_t getTriangleCount() const override { return 1200; }

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override {
        outCommand.vertexBufferData = m_vertices.data();
        outCommand.vertexBufferSize = m_vertices.size();
        outCommand.indexBufferData = nullptr;
        outCommand.indexBufferSize = 0;
        outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 8);
        outCommand.drawParams.firstVertex = 0;
        outCommand.drawParams.indexed = false;

        // 透视投影
        float aspect = 640.0f / 480.0f;
        mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
        mat4 view = lookAt(vec3(12.0f, 8.0f, 12.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 model = identity();

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                outCommand.projectionMatrix[i * 4 + j] = proj[i][j];
                outCommand.viewMatrix[i * 4 + j] = view[i][j];
                outCommand.modelMatrix[i * 4 + j] = model[i][j];
            }
        }

        outCommand.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
    }

    const std::vector<float>& getVertexData() const override { return m_vertices; }

private:
    std::vector<float> m_vertices;
};

// ============================================================================
// Triangle-SponzaStyle Scene: Sponza风格走廊 (~80 triangles)
// Simplified archway and pillar structure
// ============================================================================
class TriangleSponzaStyleScene : public TestScene {
public:
    TriangleSponzaStyleScene() : TestScene("Triangle-SponzaStyle", "Sponza-style corridor with ~80 triangles") {
        buildSponzaStyleGeometry();
    }

    uint32_t getTriangleCount() const override { return static_cast<uint32_t>(m_vertices.size() / 24); } // 8 floats per vertex

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override {
        outCommand.vertexBufferData = m_vertices.data();
        outCommand.vertexBufferSize = m_vertices.size();
        outCommand.indexBufferData = nullptr;
        outCommand.indexBufferSize = 0;
        outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 8);
        outCommand.drawParams.firstVertex = 0;
        outCommand.drawParams.indexed = false;

        // 透视投影
        float aspect = 640.0f / 480.0f;
        mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
        mat4 view = lookAt(vec3(0.0f, 2.0f, 8.0f), vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 model = identity();

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                outCommand.projectionMatrix[i * 4 + j] = proj[i][j];
                outCommand.viewMatrix[i * 4 + j] = view[i][j];
                outCommand.modelMatrix[i * 4 + j] = model[i][j];
            }
        }

        outCommand.clearColor = {0.1f, 0.1f, 0.15f, 1.0f};
    }

    const std::vector<float>& getVertexData() const override { return m_vertices; }

private:
    std::vector<float> m_vertices;

    // Helper to add a quad (2 triangles)
    // Format per vertex: x, y, z, w, r, g, b, a (8 floats)
    void addQuad(float x0, float y0, float z0,
                 float x1, float y1, float z1,
                 float x2, float y2, float z2,
                 float x3, float y3, float z3,
                 float r, float g, float b) {
        // Triangle 1: v0, v1, v2
        m_vertices.insert(m_vertices.end(), {x0, y0, z0, 1.0f, r, g, b, 1.0f});
        m_vertices.insert(m_vertices.end(), {x1, y1, z1, 1.0f, r, g, b, 1.0f});
        m_vertices.insert(m_vertices.end(), {x2, y2, z2, 1.0f, r, g, b, 1.0f});
        // Triangle 2: v0, v2, v3
        m_vertices.insert(m_vertices.end(), {x0, y0, z0, 1.0f, r, g, b, 1.0f});
        m_vertices.insert(m_vertices.end(), {x2, y2, z2, 1.0f, r, g, b, 1.0f});
        m_vertices.insert(m_vertices.end(), {x3, y3, z3, 1.0f, r, g, b, 1.0f});
    }

    // Helper to add a pillar (4 thin quads)
    void addPillar(float cx, float cz, float height, float width, float r, float g, float b) {
        float hw = width * 0.5f;

        // Front face
        addQuad(cx - hw, 0.0f, cz - hw,
                cx + hw, 0.0f, cz - hw,
                cx + hw, height, cz - hw,
                cx - hw, height, cz - hw,
                r, g, b);

        // Back face
        addQuad(cx + hw, 0.0f, cz + hw,
                cx - hw, 0.0f, cz + hw,
                cx - hw, height, cz + hw,
                cx + hw, height, cz + hw,
                r, g, b);

        // Left face
        addQuad(cx - hw, 0.0f, cz + hw,
                cx - hw, 0.0f, cz - hw,
                cx - hw, height, cz - hw,
                cx - hw, height, cz + hw,
                r, g, b);

        // Right face
        addQuad(cx + hw, 0.0f, cz - hw,
                cx + hw, 0.0f, cz + hw,
                cx + hw, height, cz + hw,
                cx + hw, height, cz - hw,
                r, g, b);
    }

    void buildSponzaStyleGeometry() {
        // Simplified Sponza-style layout:
        // - Main floor
        // - Two side walls
        // - 4 pillars
        // - Archway entrance

        float floorY = 0.0f;
        float wallHeight = 3.0f;

        // Floor
        addQuad(-6.0f, floorY, -8.0f,
                 6.0f, floorY, -8.0f,
                 6.0f, floorY,  8.0f,
                -6.0f, floorY,  8.0f,
                0.5f, 0.4f, 0.3f);

        // Ceiling
        addQuad(-6.0f, wallHeight,  8.0f,
                 6.0f, wallHeight,  8.0f,
                 6.0f, wallHeight, -8.0f,
                -6.0f, wallHeight, -8.0f,
                0.4f, 0.35f, 0.3f);

        // Left wall
        addQuad(-6.0f, floorY, -8.0f,
                -6.0f, floorY,  8.0f,
                -6.0f, wallHeight, 8.0f,
                -6.0f, wallHeight, -8.0f,
                0.6f, 0.5f, 0.4f);

        // Right wall
        addQuad(6.0f, floorY,  8.0f,
                6.0f, floorY, -8.0f,
                6.0f, wallHeight, -8.0f,
                6.0f, wallHeight,  8.0f,
                0.6f, 0.5f, 0.4f);

        // Back wall
        addQuad(-6.0f, floorY, 8.0f,
                 6.0f, floorY, 8.0f,
                 6.0f, wallHeight, 8.0f,
                -6.0f, wallHeight, 8.0f,
                0.55f, 0.45f, 0.35f);

        // Pillars (4)
        addPillar(-4.0f, -4.0f, wallHeight, 0.4f, 0.7f, 0.65f, 0.6f);
        addPillar(-4.0f,  4.0f, wallHeight, 0.4f, 0.7f, 0.65f, 0.6f);
        addPillar( 4.0f, -4.0f, wallHeight, 0.4f, 0.7f, 0.65f, 0.6f);
        addPillar( 4.0f,  4.0f, wallHeight, 0.4f, 0.7f, 0.65f, 0.6f);

        // Archway top (above entrance)
        addQuad(-2.0f, 2.0f, -8.0f,
                 2.0f, 2.0f, -8.0f,
                 2.0f, wallHeight, -8.0f,
                -2.0f, wallHeight, -8.0f,
                0.65f, 0.55f, 0.45f);

        // Small decorative elements (vases/shelves) - simplified as small cubes
        std::vector<float> smallCube;
        createCubeVertices(smallCube, 0.3f);

        // Add 4 small decorative cubes on pillars
        float positions[][3] = {
            {-4.0f, wallHeight, -4.0f},
            {-4.0f, wallHeight,  4.0f},
            { 4.0f, wallHeight, -4.0f},
            { 4.0f, wallHeight,  4.0f}
        };

        for (auto& pos : positions) {
            for (size_t i = 0; i < smallCube.size(); i += 7) {
                m_vertices.push_back(smallCube[i]     + pos[0]);
                m_vertices.push_back(smallCube[i + 1] + pos[1]);
                m_vertices.push_back(smallCube[i + 2] + pos[2]);
                m_vertices.push_back(smallCube[i + 3]);
                m_vertices.push_back(smallCube[i + 4]);
                m_vertices.push_back(smallCube[i + 5]);
                m_vertices.push_back(smallCube[i + 6]);
            }
        }
    }
};

// ============================================================================
// PBR-Material Scene: PBR材质参数场景
// This scene focuses on material properties rather than geometry count
// ============================================================================
class PBRMaterialScene : public TestScene {
public:
    PBRMaterialScene() : TestScene("PBR-Material", "PBR material parameters visualization") {
        // Create a scene with various PBR material spheres arranged in a grid
        // Each sphere demonstrates different PBR parameter combinations

        // PBR parameters: metallic, roughness, albedo
        // We'll visualize 9 material variations as colored spheres

        struct PBRParams {
            float metallic, roughness;
            float r, g, b;
        };

        PBRParams materials[] = {
            // Gold variations
            {1.0f, 0.1f, 1.0f, 0.8f, 0.0f},    // Polished gold
            {1.0f, 0.3f, 1.0f, 0.7f, 0.0f},    // Brushed gold
            {1.0f, 0.6f, 1.0f, 0.6f, 0.0f},    // Rough gold
            // Silver variations
            {1.0f, 0.1f, 0.9f, 0.9f, 0.9f},    // Polished silver
            {1.0f, 0.4f, 0.8f, 0.8f, 0.8f},    // Brushed silver
            {1.0f, 0.8f, 0.7f, 0.7f, 0.7f},    // Rough silver
            // Non-metallic variations
            {0.0f, 0.1f, 0.8f, 0.2f, 0.2f},    // Glossy red (plastic)
            {0.0f, 0.4f, 0.2f, 0.6f, 0.3f},    // Semi-gloss green
            {0.0f, 0.8f, 0.3f, 0.3f, 0.8f},    // Matte purple
        };

        float spacing = 2.5f;
        float offsetX = -spacing;
        float offsetZ = -spacing;

        for (int i = 0; i < 9; i++) {
            float px = offsetX + (i % 3) * spacing;
            float pz = offsetZ + (i / 3) * spacing;

            addSphere(px, 0.0f, pz, 0.6f,
                      materials[i].r,
                      materials[i].g,
                      materials[i].b);
        }
    }

    uint32_t getTriangleCount() const override {
        // Approximate: 9 spheres with ~20 segments each = ~180 triangles
        return 180;
    }

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override {
        outCommand.vertexBufferData = m_vertices.data();
        outCommand.vertexBufferSize = m_vertices.size();
        outCommand.indexBufferData = nullptr;
        outCommand.indexBufferSize = 0;
        outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 8);
        outCommand.drawParams.firstVertex = 0;
        outCommand.drawParams.indexed = false;

        float aspect = 640.0f / 480.0f;
        mat4 proj = perspective(radians(50.0f), aspect, 0.1f, 100.0f);
        mat4 view = lookAt(vec3(4.0f, 3.0f, 6.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
        mat4 model = identity();

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                outCommand.projectionMatrix[i * 4 + j] = proj[i][j];
                outCommand.viewMatrix[i * 4 + j] = view[i][j];
                outCommand.modelMatrix[i * 4 + j] = model[i][j];
            }
        }

        outCommand.clearColor = {0.05f, 0.05f, 0.1f, 1.0f};
    }

    const std::vector<float>& getVertexData() const override { return m_vertices; }

private:
    std::vector<float> m_vertices;

    void addSphere(float cx, float cy, float cz, float radius,
                   float r, float g, float b) {
        const int segments = 12;  // Latitudinal and longitudinal segments

        for (int lat = 0; lat < segments; lat++) {
            float theta1 = static_cast<float>(lat) * PI / segments;
            float theta2 = static_cast<float>(lat + 1) * PI / segments;

            for (int lon = 0; lon < segments; lon++) {
                float phi1 = static_cast<float>(lon) * TWO_PI / segments;
                float phi2 = static_cast<float>(lon + 1) * TWO_PI / segments;

                // 4 vertices of the quad
                float x1 = cx + radius * sin(theta1) * cos(phi1);
                float y1 = cy + radius * cos(theta1);
                float z1 = cz + radius * sin(theta1) * sin(phi1);

                float x2 = cx + radius * sin(theta1) * cos(phi2);
                float y2 = cy + radius * cos(theta1);
                float z2 = cz + radius * sin(theta1) * sin(phi2);

                float x3 = cx + radius * sin(theta2) * cos(phi2);
                float y3 = cy + radius * cos(theta2);
                float z3 = cz + radius * sin(theta2) * sin(phi2);

                float x4 = cx + radius * sin(theta2) * cos(phi1);
                float y4 = cy + radius * cos(theta2);
                float z4 = cz + radius * sin(theta2) * sin(phi1);

                // Triangle 1: v1, v2, v3
                // Format: x, y, z, w, r, g, b, a (8 floats)
                m_vertices.insert(m_vertices.end(), {x1, y1, z1, 1.0f, r, g, b, 1.0f});
                m_vertices.insert(m_vertices.end(), {x2, y2, z2, 1.0f, r, g, b, 1.0f});
                m_vertices.insert(m_vertices.end(), {x3, y3, z3, 1.0f, r, g, b, 1.0f});
                // Triangle 2: v1, v3, v4
                m_vertices.insert(m_vertices.end(), {x1, y1, z1, 1.0f, r, g, b, 1.0f});
                m_vertices.insert(m_vertices.end(), {x3, y3, z3, 1.0f, r, g, b, 1.0f});
                m_vertices.insert(m_vertices.end(), {x4, y4, z4, 1.0f, r, g, b, 1.0f});
            }
        }
    }
};

// ============================================================================
// Scene Factory Functions
// ============================================================================
std::shared_ptr<TestScene> createTriangle1TriScene() {
    return std::make_shared<Triangle1TriScene>();
}

std::shared_ptr<TestScene> createTriangleCubeScene() {
    return std::make_shared<TriangleCubeScene>();
}

std::shared_ptr<TestScene> createTriangleCubes100Scene() {
    return std::make_shared<TriangleCubes100Scene>();
}

std::shared_ptr<TestScene> createTriangleSponzaStyleScene() {
    return std::make_shared<TriangleSponzaStyleScene>();
}

std::shared_ptr<TestScene> createPBRMaterialScene() {
    return std::make_shared<PBRMaterialScene>();
}

// ============================================================================
// TestSceneRegistry Implementation
// ============================================================================
TestSceneRegistry& TestSceneRegistry::instance() {
    static TestSceneRegistry registry;
    return registry;
}

void TestSceneRegistry::registerScene(std::shared_ptr<TestScene> scene) {
    m_scenes.push_back(scene);
}

std::shared_ptr<TestScene> TestSceneRegistry::getScene(const std::string& name) const {
    for (const auto& scene : m_scenes) {
        if (scene->getName() == name) {
            return scene;
        }
    }
    return nullptr;
}

std::vector<std::string> TestSceneRegistry::getAllSceneNames() const {
    std::vector<std::string> names;
    for (const auto& scene : m_scenes) {
        names.push_back(scene->getName());
    }
    return names;
}

std::vector<std::shared_ptr<TestScene>> TestSceneRegistry::getAllScenes() const {
    return m_scenes;
}

void TestSceneRegistry::registerBuiltinScenes() {
    registerScene(createTriangle1TriScene());
    registerScene(createTriangleCubeScene());
    registerScene(createTriangleCubes100Scene());
    registerScene(createTriangleSponzaStyleScene());
    registerScene(createPBRMaterialScene());
}

}  // namespace SoftGPU
