// ============================================================================
// SoftGPU - TestScene Unit Tests
// PHASE4
// ============================================================================

#include <gtest/gtest.h>
#include "test/TestScene.hpp"
#include "test/TestSceneBuilder.hpp"

namespace {

using namespace SoftGPU;
using glm::vec3;

class TestSceneTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestSceneRegistry::instance().registerBuiltinScenes();
    }
};

// ---------------------------------------------------------------------------
// TestSceneRegistry Tests
// ---------------------------------------------------------------------------

TEST_F(TestSceneTest, RegistrySingleton) {
    auto& reg1 = TestSceneRegistry::instance();
    auto& reg2 = TestSceneRegistry::instance();
    EXPECT_EQ(&reg1, &reg2);
}

TEST_F(TestSceneTest, RegisterBuiltinScenes) {
    auto sceneNames = TestSceneRegistry::instance().getAllSceneNames();
    EXPECT_GE(sceneNames.size(), 5u);
}

TEST_F(TestSceneTest, GetSceneByName) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri");
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getName(), "Triangle-1Tri");
}

TEST_F(TestSceneTest, GetNonexistentScene) {
    auto scene = TestSceneRegistry::instance().getScene("NonExistentScene");
    EXPECT_EQ(scene, nullptr);
}

// ---------------------------------------------------------------------------
// Built-in Scene Tests
// ---------------------------------------------------------------------------

TEST_F(TestSceneTest, Triangle1TriScene) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-1Tri");
    ASSERT_NE(scene, nullptr);

    EXPECT_EQ(scene->getTriangleCount(), 1u);
    EXPECT_EQ(scene->getName(), "Triangle-1Tri");

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    EXPECT_EQ(cmd.drawParams.vertexCount, 3u);
    EXPECT_FALSE(cmd.drawParams.indexed);
}

TEST_F(TestSceneTest, TriangleCubeScene) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cube");
    ASSERT_NE(scene, nullptr);

    EXPECT_EQ(scene->getTriangleCount(), 12u);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    EXPECT_EQ(cmd.drawParams.vertexCount, 36u);  // 12 triangles * 3 vertices
}

TEST_F(TestSceneTest, TriangleCubes100Scene) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cubes-100");
    ASSERT_NE(scene, nullptr);

    EXPECT_EQ(scene->getTriangleCount(), 1200u);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);

    // 1200 triangles * 3 vertices = 3600 vertices
    EXPECT_EQ(cmd.drawParams.vertexCount, 3600u);
}

TEST_F(TestSceneTest, TriangleSponzaStyleScene) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-SponzaStyle");
    ASSERT_NE(scene, nullptr);

    // Should have around 80 triangles
    EXPECT_GE(scene->getTriangleCount(), 50u);
    EXPECT_LE(scene->getTriangleCount(), 200u);
}

TEST_F(TestSceneTest, PBRMaterialScene) {
    auto scene = TestSceneRegistry::instance().getScene("PBR-Material");
    ASSERT_NE(scene, nullptr);

    // PBR scene has 9 spheres with ~20 segments each
    EXPECT_GE(scene->getTriangleCount(), 100u);
}

// ---------------------------------------------------------------------------
// TestSceneBuilder Tests
// ---------------------------------------------------------------------------

TEST_F(TestSceneTest, BuilderDefaultScene) {
    TestSceneBuilder builder;
    auto scene = builder.build();
    ASSERT_NE(scene, nullptr);
}

TEST_F(TestSceneTest, BuilderSingleTriangle) {
    auto scene = TestSceneBuilder()
        .withType(TestSceneBuilder::SceneType::SingleTriangle)
        .withObjectScale(2.0f)
        .build();

    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getTriangleCount(), 1u);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    EXPECT_EQ(cmd.drawParams.vertexCount, 3u);
}

TEST_F(TestSceneTest, BuilderCube) {
    auto scene = TestSceneBuilder()
        .withType(TestSceneBuilder::SceneType::Cube)
        .withObjectScale(1.5f)
        .build();

    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getTriangleCount(), 12u);
}

TEST_F(TestSceneTest, BuilderMultipleCubes) {
    auto scene = TestSceneBuilder()
        .withType(TestSceneBuilder::SceneType::MultipleCubes)
        .withCubeCount(27)  // 3x3 grid
        .withSpacing(3.0f)
        .build();

    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getTriangleCount(), 27 * 12u);  // 27 cubes, 12 triangles each
}

TEST_F(TestSceneTest, BuilderCustomName) {
    auto scene = TestSceneBuilder()
        .withType(TestSceneBuilder::SceneType::Cube)
        .withCustomName("MyCustomCube")
        .withCustomDescription("My custom cube scene")
        .build();

    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getName(), "MyCustomCube");
    EXPECT_EQ(scene->getDescription(), "My custom cube scene");
}

TEST_F(TestSceneTest, BuilderPresets) {
    auto presets = TestSceneBuilder::getAvailablePresets();
    EXPECT_GE(presets.size(), 5u);

    for (const auto& name : presets) {
        auto scene = TestSceneBuilder::createPreset(name);
        ASSERT_NE(scene, nullptr) << "Failed to create preset: " << name;
        EXPECT_EQ(scene->getName(), name);
    }
}

// ---------------------------------------------------------------------------
// InstancedSceneBuilder Tests
// ---------------------------------------------------------------------------

TEST_F(TestSceneTest, InstancedSceneBuilder) {
    InstancedSceneBuilder builder;

    // Add some instances
    builder.addInstance({vec3(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, vec3(1.0f, 0.0f, 0.0f)});
    builder.addInstance({vec3(2.0f, 0.0f, 0.0f), 1.0f, 0.0f, vec3(0.0f, 1.0f, 0.0f)});
    builder.addInstance({vec3(4.0f, 0.0f, 0.0f), 1.0f, 0.0f, vec3(0.0f, 0.0f, 1.0f)});

    EXPECT_EQ(builder.getInstanceCount(), 3u);

    auto scene = builder.buildCubeInstances(100);
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->getTriangleCount(), 3 * 12u);
}

// ---------------------------------------------------------------------------
// Scene RenderCommand Tests
// ---------------------------------------------------------------------------

TEST_F(TestSceneTest, AllScenesProduceValidRenderCommands) {
    for (const auto& name : TestSceneRegistry::instance().getAllSceneNames()) {
        auto scene = TestSceneRegistry::instance().getScene(name);
        ASSERT_NE(scene, nullptr) << "Failed to get scene: " << name;

        RenderCommand cmd;
        scene->buildRenderCommand(cmd);

        // Basic validation
        EXPECT_GT(cmd.drawParams.vertexCount, 0u) << "Invalid vertex count for " << name;
        EXPECT_NE(cmd.vertexBufferData, nullptr) << "Null vertex buffer for " << name;
        EXPECT_GT(cmd.vertexBufferSize, 0u) << "Zero vertex buffer size for " << name;
    }
}

TEST_F(TestSceneTest, SceneVertexDataConsistency) {
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cube");
    ASSERT_NE(scene, nullptr);

    const auto& vertices = scene->getVertexData();

    // Should have 36 vertices * 8 floats = 288 floats (x,y,z,w,r,g,b,a)
    EXPECT_EQ(vertices.size(), 36u * 8u);

    // Each vertex should have valid color (alpha = 1.0 at index 7)
    for (size_t i = 7; i < vertices.size(); i += 8) {
        EXPECT_FLOAT_EQ(vertices[i], 1.0f);  // Alpha
    }
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
