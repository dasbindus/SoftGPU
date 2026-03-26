// ============================================================================
// SoftGPU - TestSceneBuilder.hpp
// 动态测试场景生成器
// PHASE4
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "TestScene.hpp"

namespace SoftGPU {

// ============================================================================
// TestSceneBuilder - 动态场景生成器
// 支持运行时生成自定义测试场景
// ============================================================================
class TestSceneBuilder {
public:
    TestSceneBuilder() = default;

    // ------------------------------------------------------------------------
    // 场景类型枚举
    // ------------------------------------------------------------------------
    enum class SceneType {
        SingleTriangle,
        Cube,
        MultipleCubes,
        Corridor,
        Spheres,
        Custom
    };

    // ------------------------------------------------------------------------
    // 场景配置
    // ------------------------------------------------------------------------
    struct SceneConfig {
        SceneType type = SceneType::SingleTriangle;
        uint32_t cubeCount = 1;           // For MultipleCubes
        uint32_t sphereCount = 1;          // For Spheres
        float objectScale = 1.0f;
        float spacing = 2.0f;              // Spacing between objects
        bool enableInstancing = false;     // Use instancing if supported
        std::string customName;
        std::string customDescription;
    };

    // ------------------------------------------------------------------------
    // 链式配置 API
    // ------------------------------------------------------------------------
    TestSceneBuilder& withType(SceneType type);
    TestSceneBuilder& withCubeCount(uint32_t count);
    TestSceneBuilder& withSphereCount(uint32_t count);
    TestSceneBuilder& withObjectScale(float scale);
    TestSceneBuilder& withSpacing(float spacing);
    TestSceneBuilder& withInstancing(bool enable);
    TestSceneBuilder& withCustomName(const std::string& name);
    TestSceneBuilder& withCustomDescription(const std::string& desc);

    // ------------------------------------------------------------------------
    // 构建场景
    // ------------------------------------------------------------------------
    std::shared_ptr<TestScene> build();

    // ------------------------------------------------------------------------
    // 预设场景（简化 API）
    // ------------------------------------------------------------------------
    static std::shared_ptr<TestScene> createPreset(const std::string& presetName);

    // ------------------------------------------------------------------------
    // 获取所有可用预设
    // ------------------------------------------------------------------------
    static std::vector<std::string> getAvailablePresets();

private:
    SceneConfig m_config;

    // 内部：从配置生成场景
    std::shared_ptr<TestScene> buildFromConfig();
};

// ============================================================================
// InstancedSceneBuilder - 实例化场景生成器
// 用于生成大量相似物体的高效场景
// ============================================================================
class InstancedSceneBuilder {
public:
    struct InstanceData {
        glm::vec3 position;
        float scale;
        float rotation;  // radians around Y axis
        glm::vec3 color;
    };

    InstancedSceneBuilder& addInstance(const InstanceData& instance);
    InstancedSceneBuilder& clearInstances();

    std::shared_ptr<TestScene> buildCubeInstances(uint32_t maxInstances = 1000);
    std::shared_ptr<TestScene> buildSphereInstances(uint32_t maxInstances = 1000);

    size_t getInstanceCount() const { return m_instances.size(); }

private:
    std::vector<InstanceData> m_instances;
};

}  // namespace SoftGPU
