// ============================================================================
// SoftGPU - TestScene.hpp
// 测试场景定义和注册表
// PHASE4
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <core/PipelineTypes.hpp>
#include <core/RenderCommand.hpp>

namespace SoftGPU {

// ============================================================================
// TestScene - 测试场景基类
// ============================================================================
class TestScene {
public:
    virtual ~TestScene() = default;

    // 获取场景名称
    const std::string& getName() const { return m_name; }

    // 获取三角形数量
    virtual uint32_t getTriangleCount() const = 0;

    // 获取场景描述
    virtual const std::string& getDescription() const = 0;

    // 构建渲染命令（生成顶点数据并填充 RenderCommand）
    virtual void buildRenderCommand(RenderCommand& outCommand) = 0;

    // 获取预设的顶点数据（用于验证）
    virtual const std::vector<float>& getVertexData() const = 0;

protected:
    TestScene(const std::string& name, const std::string& description)
        : m_name(name), m_description(description) {}

    std::string m_name;
    std::string m_description;
};

// ============================================================================
// TestSceneRegistry - 场景注册表
// ============================================================================
class TestSceneRegistry {
public:
    // 获取单例
    static TestSceneRegistry& instance();

    // 注册场景
    void registerScene(std::shared_ptr<TestScene> scene);

    // 按名称获取场景
    std::shared_ptr<TestScene> getScene(const std::string& name) const;

    // 获取所有场景名称
    std::vector<std::string> getAllSceneNames() const;

    // 获取所有场景
    std::vector<std::shared_ptr<TestScene>> getAllScenes() const;

    // 注册所有内置场景
    void registerBuiltinScenes();

private:
    TestSceneRegistry() = default;

    std::vector<std::shared_ptr<TestScene>> m_scenes;
};

// ============================================================================
// 内置测试场景声明
// ============================================================================

// Triangle-1Tri: 单绿色三角形
std::shared_ptr<TestScene> createTriangle1TriScene();

// Triangle-Cube: 立方体 (12 triangles)
std::shared_ptr<TestScene> createTriangleCubeScene();

// Triangle-Cubes-100: 100个立方体 (1200 triangles)
std::shared_ptr<TestScene> createTriangleCubes100Scene();

// Triangle-SponzaStyle: Sponza风格走廊 (~80 triangles)
std::shared_ptr<TestScene> createTriangleSponzaStyleScene();

// PBR-Material: PBR材质参数场景
std::shared_ptr<TestScene> createPBRMaterialScene();

}  // namespace SoftGPU
