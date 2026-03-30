// ============================================================================
// SoftGPU - main.cpp
// 支持两种模式：
//   1. GUI模式 (默认): GLFW窗口 + ImGui UI
//   2. Headless模式: 只输出PPM，无需图形界面
//
// 用法:
//   ./SoftGPU                                    # GUI模式
//   ./SoftGPU --headless                        # Headless模式，输出到当前目录
//   ./SoftGPU --headless --output /tmp/         # Headless模式，输出到指定目录
//   ./SoftGPU --headless --scene Triangle-1Tri  # Headless模式，指定场景
// ============================================================================

// Include system OpenGL first (on macOS use OpenGL3 for core profile)
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

// glad loader function declarations (from glad.c)
typedef unsigned char (*GLADloadproc)(const char* name);
extern "C" int gladLoadGLLoader(GLADloadproc load);

#include <string>
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
#include "test/TestScene.hpp"

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
    const char* output_filename = nullptr;  // 自定义输出文件名
    const char* scene_name = "Triangle-1Tri";  // 默认场景
};

void printHelp(const char* program) {
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  --headless              Run in headless mode (no GUI), output PPM file\n");
    printf("  --output <dir>          Output directory for PPM files (default: .)\n");
    printf("  --output-filename <name> Custom output filename (default: frame_0000.ppm)\n");
    printf("  --no-tbr                Disable TBR (Tile-Based Rendering) mode\n");
    printf("  --scene <name>          Scene to render in headless mode (default: Triangle-1Tri)\n");
    printf("  --help, -h              Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s                           # GUI mode\n", program);
    printf("  %s --headless                # Headless, output frame_0000.ppm to current directory\n", program);
    printf("  %s --headless --output /tmp/ # Headless, output to /tmp/frame_0000.ppm\n", program);
    printf("  %s --headless --output-filename my_render.ppm  # Output my_render.ppm\n", program);
    printf("  %s --headless --scene Triangle-Cube  # Render specific scene\n", program);
    printf("\nAvailable scenes:\n");
    TestSceneRegistry::instance().registerBuiltinScenes();
    for (const auto& name : TestSceneRegistry::instance().getAllSceneNames()) {
        printf("  - %s\n", name.c_str());
    }
}

CmdArgs parseArgs(int argc, char* argv[]) {
    CmdArgs args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            // Will be handled in main()
        } else if (strcmp(argv[i], "--headless") == 0) {
            args.headless = true;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (strcmp(argv[i], "--output-filename") == 0 && i + 1 < argc) {
            args.output_filename = argv[++i];
        } else if (strcmp(argv[i], "--no-tbr") == 0) {
            args.use_tbr = false;
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            args.scene_name = argv[++i];
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
    printf("[INFO] Scene: %s\n", args.scene_name);

    // 注册内置场景
    TestSceneRegistry::instance().registerBuiltinScenes();

    // 获取指定场景
    auto scene = TestSceneRegistry::instance().getScene(args.scene_name);
    if (!scene) {
        printf("[ERROR] Scene not found: %s\n", args.scene_name);
        printf("[INFO] Available scenes:\n");
        for (const auto& name : TestSceneRegistry::instance().getAllSceneNames()) {
            printf("  - %s\n", name.c_str());
        }
        return -1;
    }
    printf("[INFO] Found scene: %s (%s)\n", scene->getName().c_str(), scene->getDescription().c_str());

    // 创建渲染管线
    RenderPipeline pipeline;

    // 可选：禁用TBR模式（直接渲染到framebuffer）
    if (!args.use_tbr) {
        pipeline.setTBREnabled(false);
        printf("[INFO] TBR mode disabled\n");
    }

    // 创建渲染命令
    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    // 渲染
    printf("[INFO] Rendering...\n");
    pipeline.render(cmd);

    // 输出PPM
    const char* filename = args.output_filename ? args.output_filename : "frame_0000.ppm";
    pipeline.setDumpOutputPath(args.output_dir);
    pipeline.dump(filename);

    printf("[INFO] Dumped frame to: %s/%s\n", args.output_dir, filename);
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

    // Check if help was requested (headless is set as a marker)
    // We use use_tbr = true as default, if help was shown it stays true but we exit
    if (args.headless && args.output_dir == nullptr && args.scene_name == nullptr) {
        // This is a bit hacky - help case. Let's use a better check.
    }

    // Re-parse properly to detect help
    bool showHelp = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            showHelp = true;
            break;
        }
    }

    if (showHelp) {
        printHelp(argv[0]);
        return 0;
    }

    if (args.headless) {
        return runHeadless(args);
    } else {
        return runGUI();
    }
}
