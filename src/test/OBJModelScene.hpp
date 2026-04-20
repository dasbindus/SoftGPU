// ============================================================================
// SoftGPU - OBJModelScene.hpp
// OBJ Model Loading Test Scene
// ============================================================================

#pragma once

#include "TestScene.hpp"
#include <string>
#include <memory>

namespace SoftGPU {

// ============================================================================
// OBJModelScene - Loads and renders an OBJ model file
// ============================================================================
class OBJModelScene : public TestScene {
public:
    // Construct with path to OBJ file
    explicit OBJModelScene(const std::string& objFilepath);

    uint32_t getTriangleCount() const override { return m_triangleCount; }

    const std::string& getDescription() const override { return m_description; }

    void buildRenderCommand(RenderCommand& outCommand) override;

    const std::vector<float>& getVertexData() const override { return m_vertices; }

    // Check if model was loaded successfully
    bool isLoaded() const { return m_loaded; }

private:
    std::string m_objFilepath;
    std::string m_description;
    std::string m_error;
    std::vector<float> m_vertices;
    uint32_t m_triangleCount = 0;
    bool m_loaded = false;
};

// Factory function
std::shared_ptr<TestScene> createOBJModelScene(const std::string& objFilepath);

} // namespace SoftGPU
