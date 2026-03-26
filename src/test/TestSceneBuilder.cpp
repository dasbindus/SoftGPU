// ============================================================================
// SoftGPU - TestSceneBuilder.cpp
// 动态测试场景生成器实现
// PHASE4
// ============================================================================

#include "TestSceneBuilder.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
using vec4 = glm::vec4;
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

// ============================================================================
// TestSceneBuilder Implementation
// ============================================================================
TestSceneBuilder& TestSceneBuilder::withType(SceneType type) {
    m_config.type = type;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withCubeCount(uint32_t count) {
    m_config.cubeCount = count;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withSphereCount(uint32_t count) {
    m_config.sphereCount = count;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withObjectScale(float scale) {
    m_config.objectScale = scale;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withSpacing(float spacing) {
    m_config.spacing = spacing;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withInstancing(bool enable) {
    m_config.enableInstancing = enable;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withCustomName(const std::string& name) {
    m_config.customName = name;
    return *this;
}

TestSceneBuilder& TestSceneBuilder::withCustomDescription(const std::string& desc) {
    m_config.customDescription = desc;
    return *this;
}

// Helper: create cube vertices
// Format per vertex: x, y, z, w, r, g, b, a (8 floats)
static void createCubeTemplate(std::vector<float>& vertices, float size) {
    float h = size * 0.5f;

    // Front face
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f});

    // Back face
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f});

    // Left face
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f});

    // Right face
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f});

    // Top face
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, +h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, +h, -h, 1.0f,  1.0f, 0.0f, 1.0f, 1.0f});

    // Bottom face
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, -h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {+h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
    vertices.insert(vertices.end(), {-h, -h, +h, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f});
}

std::shared_ptr<TestScene> TestSceneBuilder::build() {
    return buildFromConfig();
}

std::shared_ptr<TestScene> TestSceneBuilder::buildFromConfig() {
    // Create a custom scene based on configuration
    struct DynamicScene : public TestScene {
        DynamicScene(const std::string& name, const std::string& desc,
                     const std::vector<float>& verts, uint32_t triCount)
            : TestScene(name, desc), m_vertices(verts), m_triCount(triCount) {}

        uint32_t getTriangleCount() const override { return m_triCount; }
        const std::string& getDescription() const override { return m_description; }

        void buildRenderCommand(RenderCommand& outCommand) override {
            outCommand.vertexBufferData = m_vertices.data();
            outCommand.vertexBufferSize = m_vertices.size();
            outCommand.indexBufferData = nullptr;
            outCommand.indexBufferSize = 0;
            outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 7);
            outCommand.drawParams.firstVertex = 0;
            outCommand.drawParams.indexed = false;

            float aspect = 640.0f / 480.0f;
            mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
            mat4 view = lookAt(vec3(8.0f, 6.0f, 8.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
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
        uint32_t m_triCount;
    };

    std::vector<float> vertices;
    uint32_t triCount = 0;
    std::string name = m_config.customName.empty() ? "DynamicScene" : m_config.customName;
    std::string desc = m_config.customDescription;

    float scale = m_config.objectScale;
    float spacing = m_config.spacing;

    switch (m_config.type) {
        case SceneType::SingleTriangle: {
            // Single triangle
            vertices = {
                -0.5f * scale, -0.5f * scale, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
                 0.5f * scale, -0.5f * scale, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
                 0.0f * scale,  0.5f * scale, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f
            };
            triCount = 1;
            if (desc.empty()) desc = "Custom single triangle";
            break;
        }

        case SceneType::Cube: {
            createCubeTemplate(vertices, scale);
            triCount = 12;
            if (desc.empty()) desc = "Custom single cube";
            break;
        }

        case SceneType::MultipleCubes: {
            std::vector<float> cubeTemplate;
            createCubeTemplate(cubeTemplate, scale * 0.8f);

            uint32_t count = std::min(m_config.cubeCount, 1000u);
            float gridSize = std::ceil(std::sqrt(static_cast<float>(count)));
            float offset = -static_cast<float>(gridSize - 1) * spacing * 0.5f;

            for (uint32_t i = 0; i < count; i++) {
                uint32_t x = i % static_cast<uint32_t>(gridSize);
                uint32_t z = i / static_cast<uint32_t>(gridSize);
                float px = offset + x * spacing;
                float pz = offset + z * spacing;

                for (size_t j = 0; j < cubeTemplate.size(); j += 7) {
                    vertices.push_back(cubeTemplate[j] + px);
                    vertices.push_back(cubeTemplate[j + 1]);
                    vertices.push_back(cubeTemplate[j + 2] + pz);
                    vertices.push_back(cubeTemplate[j + 3]);
                    vertices.push_back(cubeTemplate[j + 4]);
                    vertices.push_back(cubeTemplate[j + 5]);
                    vertices.push_back(cubeTemplate[j + 6]);
                }
            }
            triCount = count * 12;
            if (desc.empty()) desc = "Custom " + std::to_string(count) + " cubes";
            break;
        }

        case SceneType::Spheres: {
            // Simplified sphere generation
            const int segments = 12;
            uint32_t count = std::min(m_config.sphereCount, 100u);

            float radius = scale * 0.5f;
            float gridSize = std::ceil(std::sqrt(static_cast<float>(count)));
            float offset = -static_cast<float>(gridSize - 1) * spacing * 0.5f;

            for (uint32_t s = 0; s < count; s++) {
                uint32_t x = s % static_cast<uint32_t>(gridSize);
                uint32_t z = s / static_cast<uint32_t>(gridSize);
                float cx = offset + x * spacing;
                float cz = offset + z * spacing;

                // Color based on position
                float cr = 0.5f + 0.5f * (x / gridSize);
                float cg = 0.5f + 0.5f * (z / gridSize);
                float cb = 0.5f;

                for (int lat = 0; lat < segments; lat++) {
                    float theta1 = static_cast<float>(lat) * PI / segments;
                    float theta2 = static_cast<float>(lat + 1) * PI / segments;

                    for (int lon = 0; lon < segments; lon++) {
                        float phi1 = static_cast<float>(lon) * TWO_PI / segments;
                        float phi2 = static_cast<float>(lon + 1) * TWO_PI / segments;

                        float x1 = cx + radius * sin(theta1) * cos(phi1);
                        float y1 = cz + radius * cos(theta1);
                        float z1 = cz + radius * sin(theta1) * sin(phi1);

                        float x2 = cx + radius * sin(theta1) * cos(phi2);
                        float y2 = cz + radius * cos(theta1);
                        float z2 = cz + radius * sin(theta1) * sin(phi2);

                        float x3 = cx + radius * sin(theta2) * cos(phi2);
                        float y3 = cz + radius * cos(theta2);
                        float z3 = cz + radius * sin(theta2) * sin(phi2);

                        float x4 = cx + radius * sin(theta2) * cos(phi1);
                        float y4 = cz + radius * cos(theta2);
                        float z4 = cz + radius * sin(theta2) * sin(phi1);

                        vertices.insert(vertices.end(), {x1, y1, z1, 1.0f, cr, cg, cb, 1.0f});
                        vertices.insert(vertices.end(), {x2, y2, z2, 1.0f, cr, cg, cb, 1.0f});
                        vertices.insert(vertices.end(), {x3, y3, z3, 1.0f, cr, cg, cb, 1.0f});
                        vertices.insert(vertices.end(), {x1, y1, z1, 1.0f, cr, cg, cb, 1.0f});
                        vertices.insert(vertices.end(), {x3, y3, z3, 1.0f, cr, cg, cb, 1.0f});
                        vertices.insert(vertices.end(), {x4, y4, z4, 1.0f, cr, cg, cb, 1.0f});
                    }
                }
            }
            triCount = count * segments * segments * 2;
            if (desc.empty()) desc = "Custom " + std::to_string(count) + " spheres";
            break;
        }

        case SceneType::Corridor:
        case SceneType::Custom:
        default: {
            // Fallback to single triangle
            vertices = {
                -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
                 0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f,
                 0.0f,  0.5f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f
            };
            triCount = 1;
            if (desc.empty()) desc = "Custom scene (fallback to triangle)";
            break;
        }
    }

    return std::make_shared<DynamicScene>(name, desc, vertices, triCount);
}

// Static presets
std::shared_ptr<TestScene> TestSceneBuilder::createPreset(const std::string& presetName) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    return TestSceneRegistry::instance().getScene(presetName);
}

std::vector<std::string> TestSceneBuilder::getAvailablePresets() {
    TestSceneRegistry::instance().registerBuiltinScenes();
    return TestSceneRegistry::instance().getAllSceneNames();
}

// ============================================================================
// InstancedSceneBuilder Implementation
// ============================================================================
InstancedSceneBuilder& InstancedSceneBuilder::addInstance(const InstanceData& instance) {
    m_instances.push_back(instance);
    return *this;
}

InstancedSceneBuilder& InstancedSceneBuilder::clearInstances() {
    m_instances.clear();
    return *this;
}

std::shared_ptr<TestScene> InstancedSceneBuilder::buildCubeInstances(uint32_t maxInstances) {
    struct CubeInstancesScene : public TestScene {
        CubeInstancesScene(const std::string& name, const std::string& desc,
                          const std::vector<float>& verts, uint32_t triCount)
            : TestScene(name, desc), m_vertices(verts), m_triCount(triCount) {}

        uint32_t getTriangleCount() const override { return m_triCount; }
        const std::string& getDescription() const override { return m_description; }

        void buildRenderCommand(RenderCommand& outCommand) override {
            outCommand.vertexBufferData = m_vertices.data();
            outCommand.vertexBufferSize = m_vertices.size();
            outCommand.indexBufferData = nullptr;
            outCommand.indexBufferSize = 0;
            outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 7);
            outCommand.drawParams.firstVertex = 0;
            outCommand.drawParams.indexed = false;

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
        uint32_t m_triCount;
    };

    std::vector<float> cubeTemplate;
    createCubeTemplate(cubeTemplate, 0.8f);

    std::vector<float> vertices;
    uint32_t instanceCount = std::min(static_cast<uint32_t>(m_instances.size()), maxInstances);

    for (uint32_t i = 0; i < instanceCount; i++) {
        const auto& inst = m_instances[i];
        mat4 transform = translate(inst.position) *
                        rotate(inst.rotation, vec3(0.0f, 1.0f, 0.0f)) *
                        scale(vec3(inst.scale));

        for (size_t j = 0; j < cubeTemplate.size(); j += 7) {
            vec4 v(cubeTemplate[j], cubeTemplate[j+1], cubeTemplate[j+2], 1.0f);
            vec4 transformed = transform * v;

            vertices.push_back(transformed.x);
            vertices.push_back(transformed.y);
            vertices.push_back(transformed.z);
            vertices.push_back(inst.color.r);
            vertices.push_back(inst.color.g);
            vertices.push_back(inst.color.b);
            vertices.push_back(cubeTemplate[j + 6]);
        }
    }

    uint32_t triCount = instanceCount * 12;
    std::string name = "InstancedCubes-" + std::to_string(instanceCount);
    std::string desc = "Instanced " + std::to_string(instanceCount) + " cubes";

    return std::make_shared<CubeInstancesScene>(name, desc, vertices, triCount);
}

std::shared_ptr<TestScene> InstancedSceneBuilder::buildSphereInstances(uint32_t maxInstances) {
    struct SphereInstancesScene : public TestScene {
        SphereInstancesScene(const std::string& name, const std::string& desc,
                            const std::vector<float>& verts, uint32_t triCount)
            : TestScene(name, desc), m_vertices(verts), m_triCount(triCount) {}

        uint32_t getTriangleCount() const override { return m_triCount; }
        const std::string& getDescription() const override { return m_description; }

        void buildRenderCommand(RenderCommand& outCommand) override {
            outCommand.vertexBufferData = m_vertices.data();
            outCommand.vertexBufferSize = m_vertices.size();
            outCommand.indexBufferData = nullptr;
            outCommand.indexBufferSize = 0;
            outCommand.drawParams.vertexCount = static_cast<uint32_t>(m_vertices.size() / 7);
            outCommand.drawParams.firstVertex = 0;
            outCommand.drawParams.indexed = false;

            float aspect = 640.0f / 480.0f;
            mat4 proj = perspective(radians(60.0f), aspect, 0.1f, 100.0f);
            mat4 view = lookAt(vec3(6.0f, 4.0f, 6.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
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
        uint32_t m_triCount;
    };

    const int segments = 12;
    std::vector<float> vertices;
    uint32_t instanceCount = std::min(static_cast<uint32_t>(m_instances.size()), maxInstances);

    for (uint32_t s = 0; s < instanceCount; s++) {
        const auto& inst = m_instances[s];

        for (int lat = 0; lat < segments; lat++) {
            float theta1 = static_cast<float>(lat) * PI / segments;
            float theta2 = static_cast<float>(lat + 1) * PI / segments;

            for (int lon = 0; lon < segments; lon++) {
                float phi1 = static_cast<float>(lon) * TWO_PI / segments;
                float phi2 = static_cast<float>(lon + 1) * TWO_PI / segments;

                float x1 = inst.position.x + inst.scale * sin(theta1) * cos(phi1);
                float y1 = inst.position.y + inst.scale * cos(theta1);
                float z1 = inst.position.z + inst.scale * sin(theta1) * sin(phi1);

                float x2 = inst.position.x + inst.scale * sin(theta1) * cos(phi2);
                float y2 = inst.position.y + inst.scale * cos(theta1);
                float z2 = inst.position.z + inst.scale * sin(theta1) * sin(phi2);

                float x3 = inst.position.x + inst.scale * sin(theta2) * cos(phi2);
                float y3 = inst.position.y + inst.scale * cos(theta2);
                float z3 = inst.position.z + inst.scale * sin(theta2) * sin(phi2);

                float x4 = inst.position.x + inst.scale * sin(theta2) * cos(phi1);
                float y4 = inst.position.y + inst.scale * cos(theta2);
                float z4 = inst.position.z + inst.scale * sin(theta2) * sin(phi1);

                vertices.insert(vertices.end(), {x1, y1, z1, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
                vertices.insert(vertices.end(), {x2, y2, z2, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
                vertices.insert(vertices.end(), {x3, y3, z3, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
                vertices.insert(vertices.end(), {x1, y1, z1, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
                vertices.insert(vertices.end(), {x3, y3, z3, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
                vertices.insert(vertices.end(), {x4, y4, z4, 1.0f, inst.color.r, inst.color.g, inst.color.b, 1.0f});
            }
        }
    }

    uint32_t triCount = instanceCount * segments * segments * 2;
    std::string name = "InstancedSpheres-" + std::to_string(instanceCount);
    std::string desc = "Instanced " + std::to_string(instanceCount) + " spheres";

    return std::make_shared<SphereInstancesScene>(name, desc, vertices, triCount);
}

}  // namespace SoftGPU
