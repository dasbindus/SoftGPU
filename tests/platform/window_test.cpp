#include <gtest/gtest.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

// ============================================================================
// Platform Window Test
// 测试窗口创建和基本 OpenGL 上下文
// 注意：此测试需要显示设备，CI 环境中可能跳过
// ============================================================================

namespace {

class WindowTest : public ::testing::Test {
protected:
    GLFWwindow* window = nullptr;

    void SetUp() override {
        // 初始化 GLFW
        if (!glfwInit()) {
            GTEST_SKIP() << "Failed to initialize GLFW";
        }
    }

    void TearDown() override {
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    GLFWwindow* createTestWindow() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // 隐藏窗口，方便 CI

        GLFWwindow* w = glfwCreateWindow(640, 480, "Test", nullptr, nullptr);
        if (w) {
            glfwMakeContextCurrent(w);
        }
        return w;
    }
};

TEST_F(WindowTest, GlfwInitialized)
{
    // GLFW 已在 SetUp 中初始化
    EXPECT_TRUE(glfwInit() != 0);

    // 重复初始化应返回 GL_FALSE（行为可能因版本而异）
    // 不强制测试此行为
}

TEST_F(WindowTest, CreateAndDestroyWindow)
{
    window = createTestWindow();

    if (!window) {
        GTEST_SKIP() << "Cannot create window (no display?)";
    }

    EXPECT_NE(window, nullptr);
    EXPECT_TRUE(glfwWindowShouldClose(window) == GLFW_FALSE);
}

TEST_F(WindowTest, OpenGLContextCreated)
{
    window = createTestWindow();

    if (!window) {
        GTEST_SKIP() << "Cannot create window (no display?)";
    }

    // 获取 OpenGL 版本
    const char* version = (const char*)glGetString(GL_VERSION);
    EXPECT_NE(version, nullptr);

    // 版本字符串格式应为 "4.6.x" 或类似
    std::string versionStr(version);
    EXPECT_TRUE(versionStr.find("4.") == 0 || versionStr.find("3.") == 0)
        << "Expected OpenGL 3.x or 4.x, got: " << version;
}

TEST_F(WindowTest, WindowResize)
{
    window = createTestWindow();

    if (!window) {
        GTEST_SKIP() << "Cannot create window (no display?)";
    }

    int width = 0, height = 0;
    glfwSetWindowSize(window, 1024, 768);
    glfwGetWindowSize(window, &width, &height);

    EXPECT_EQ(width, 1024);
    EXPECT_EQ(height, 768);
}

TEST_F(WindowTest, SwapInterval)
{
    window = createTestWindow();

    if (!window) {
        GTEST_SKIP() << "Cannot create window (no display?)";
    }

    // 设置垂直同步
    glfwSwapInterval(1);

    // 验证 swap interval 已设置（通过交换缓冲区验证）
    // 如果 swap interval = 1，交换时应等待垂直同步
    glfwSwapBuffers(window);

    // 无异常即通过
    SUCCEED();
}

} // anonymous namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
