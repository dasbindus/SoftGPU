// ============================================================================
// SoftGPU - main.cpp
// 支持两种模式：
//   1. GUI模式 (默认): GLFW窗口 + ImGui UI
//   2. Headless模式: 只输出PPM，无需图形界面
//
// 用法:
//   ./SoftGPU                        # GUI模式
//   ./SoftGPU --headless             # Headless模式，输出到当前目录
//   ./SoftGPU --headless --output /tmp/  # Headless模式，输出到指定目录
// ============================================================================

#include <glad/glad.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <string>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "app/Application.hpp"
#include "platform/Window.hpp"
#include "renderer/ImGuiRenderer.hpp"
#include "pipeline/RenderPipeline.hpp"
#include "core/PipelineTypes.hpp"

// Helper: build identity matrix array
std::array<float, 16> identityMatrix() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

using namespace SoftGPU;

// 窗口配置
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr const char* WINDOW_TITLE = "SoftGPU - Tile-Based GPU Simulator";
constexpr float CLEAR_COLOR[] = { 0.1f, 0.1f, 0.15f, 1.0f };

// ============================================================================
// 命令行参数解析
// ============================================================================
struct CmdArgs {
    bool headless = false;
    bool use_tbr = true;  // 默认使用TBR
    const char* output_dir = ".";
};

CmdArgs parseArgs(int argc, char* argv[]) {
    CmdArgs args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            args.headless = true;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (strcmp(argv[i], "--no-tbr") == 0) {
            args.use_tbr = false;
        }
    }
    return args;
}

// ============================================================================
// Headless模式：渲染并输出PPM
// ============================================================================
int runHeadless(const CmdArgs& args) {
    printf("[INFO] Running in HEADLESS mode\n");
    printf("[INFO] Output directory: %s\n", args.output_dir);

    // 绿色三角形顶点数据
    float vertices[] = {
        // v0: top center
        0.0f, 0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
        // v1: bottom left
        -0.5f, -0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
        // v2: bottom right
        0.5f, -0.5f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,  // green
    };

    // 创建渲染管线
    RenderPipeline pipeline;

    // 可选：禁用TBR模式（直接渲染到framebuffer）
    if (!args.use_tbr) {
        pipeline.setTBREnabled(false);
        printf("[INFO] TBR mode disabled\n");
    }

    // 创建渲染命令
    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = 24;  // 3 vertices * 8 floats (pos + color)
    cmd.drawParams.vertexCount = 3;
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    // 渲染
    printf("[INFO] Rendering green triangle...\n");
    pipeline.render(cmd);

    // 输出PPM
    char outputPath[512];
    snprintf(outputPath, sizeof(outputPath), "%s/green_triangle.ppm", args.output_dir);
    pipeline.setDumpOutputPath(args.output_dir);
    pipeline.dump("frame_0000.ppm");

    printf("[INFO] Dumped frame to: %s/frame_0000.ppm\n", args.output_dir);
    printf("[INFO] Headless render complete\n");

    return 0;
}

// ============================================================================
// GUI模式：标准GLFW + ImGui
// ============================================================================
int runGUI() {
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

    // 窗口hint
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    // ========================================================================
    // Step 2: 创建窗口
    // ========================================================================
    GLFWwindow* glfwWindow = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT,
        WINDOW_TITLE,
        nullptr,
        nullptr
    );

    if (!glfwWindow) {
        fprintf(stderr, "[ERROR] Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(glfwWindow);
    glfwSwapInterval(1);

    // ========================================================================
    // Step 3: 初始化 glad
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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    // ========================================================================
    // Step 5: 主循环
    // ========================================================================
    while (!glfwWindowShouldClose(glfwWindow)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UI面板
        {
            ImGui::Begin("SoftGPU Status");
            ImGui::Text("Phase: PHASE0 - Environment Setup");
            ImGui::Text("Status: OK");
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);
            ImGui::Separator();
            ImGui::Text("OpenGL Info:");
            ImGui::BulletText("Version: %s", glGetString(GL_VERSION));
            ImGui::BulletText("Renderer: %s", glGetString(GL_RENDERER));
            ImGui::Separator();
            if (ImGui::Button("Exit", ImVec2(100, 30))) {
                glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
            }
            ImGui::End();
        }

        ImGui::Render();
        glClearColor(CLEAR_COLOR[0], CLEAR_COLOR[1], CLEAR_COLOR[2], CLEAR_COLOR[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

// ============================================================================
// 主入口
// ============================================================================
int main(int argc, char* argv[])
{
    CmdArgs args = parseArgs(argc, argv);

    if (args.headless) {
        return runHeadless(args);
    } else {
        return runGUI();
    }
}
