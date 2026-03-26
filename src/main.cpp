// ============================================================================
// SoftGPU - main.cpp
// 最小可运行版本：窗口 + ImGui 初始化 + 帧缓冲显示
// ============================================================================

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "app/Application.hpp"
#include "platform/Window.hpp"
#include "renderer/ImGuiRenderer.hpp"

// 窗口配置
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr const char* WINDOW_TITLE = "SoftGPU - Tile-Based GPU Simulator";
constexpr float CLEAR_COLOR[] = { 0.1f, 0.1f, 0.15f, 1.0f }; // 深蓝灰

int main(int argc, char* argv[])
{
    // ========================================================================
    // Step 1: GLFW 初始化
    // ========================================================================
    if (!glfwInit()) {
        fprintf(stderr, "[ERROR] Failed to initialize GLFW\n");
        return -1;
    }

    // 配置 OpenGL 上下文（Core Profile 4.6）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // 窗口hint（可选）
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    // ========================================================================
    // Step 2: 创建窗口
    // ========================================================================
    GLFWwindow* glfwWindow = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT,
        WINDOW_TITLE,
        nullptr,  // monitor（窗口模式）
        nullptr   // share（不共享上下文）
    );

    if (!glfwWindow) {
        fprintf(stderr, "[ERROR] Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    // 将新创建的 OpenGL context 设为当前线程的 context
    glfwMakeContextCurrent(glfwWindow);

    // 启用垂直同步（防止撕裂）
    glfwSwapInterval(1);

    // ========================================================================
    // Step 3: 初始化 glad（OpenGL loader）
    // ========================================================================
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "[ERROR] Failed to initialize glad\n");
        glfwTerminate();
        return -1;
    }

    printf("[INFO] OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("[INFO] Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[INFO] SoftGPU Phase0 - Environment Ready\n");

    // ========================================================================
    // Step 4: 初始化 ImGui
    // ========================================================================
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // 键盘导航
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // 启用 Docking

    // 设置默认样式
    ImGui::StyleColorsDark();

    // 初始化 Platform 和 Renderer backend
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);  // true = 安装回调
    ImGui_ImplOpenGL3_Init("#version 460 core");    // GLSL 版本字符串

    // ========================================================================
    // Step 5: 主循环
    // ========================================================================
    while (!glfwWindowShouldClose(glfwWindow)) {
        // --- 事件处理 ---
        glfwPollEvents();

        // --- 开始 ImGui 帧 ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ====================================================================
        // 渲染内容（这里放置你的 UI 代码）
        // ====================================================================

        // 示例：创建一个简单的面板
        {
            ImGui::Begin("SoftGPU Status");

            ImGui::Text("Phase: PHASE0 - Environment Setup");
            ImGui::Text("Status: OK");
            ImGui::Separator();

            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);

            ImGui::Separator();

            // 显示 OpenGL 信息
            ImGui::Text("OpenGL Info:");
            ImGui::BulletText("Version: %s", glGetString(GL_VERSION));
            ImGui::BulletText("Renderer: %s", glGetString(GL_RENDERER));
            ImGui::BulletText("Vendor: %s", glGetString(GL_VENDOR));

            ImGui::Separator();

            // 退出按钮
            if (ImGui::Button("Exit", ImVec2(100, 30))) {
                glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
            }

            ImGui::End();
        }

        // --- 渲染 ImGui ---
        ImGui::Render();

        // --- 清屏 ---
        glClearColor(CLEAR_COLOR[0], CLEAR_COLOR[1], CLEAR_COLOR[2], CLEAR_COLOR[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- 绘制 ImGui ---
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // --- 交换前后缓冲区 ---
        glfwSwapBuffers(glfwWindow);
    }

    // ========================================================================
    // Step 6: 清理资源
    // ========================================================================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfwWindow);
    glfwTerminate();

    printf("[INFO] SoftGPU shutdown gracefully\n");
    return 0;
}
