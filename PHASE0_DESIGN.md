# SoftGPU PHASE0 设计文档

**版本：** v0.1  
**作者：** SoftGPU Architect Team  
**日期：** 2026-03-25  
**阶段目标：** 完成开发环境搭建，确保代码可编译、可运行、可测试

---

## 1. 项目目录结构

```
SoftGPU/
├── CMakeLists.txt                    # 根目录 CMake 配置
├── build/                            # 构建输出目录（不要提交到 git）
├── extern/
│   ├── GLFW/                         # GLFW 源码（git submodule）
│   ├── glad/                         # OpenGL loader (glad)
│   ├── imgui/                        # ImGui 源码（git submodule）
│   ├── glm/                          # glm 头文件（git submodule）
│   └── stb/                          # stb 头文件（git submodule）
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                      # 程序入口
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── Common.hpp                # 通用类型定义、宏
│   │   └── Math.hpp                  # 数学类型别名（使用 glm）
│   ├── platform/
│   │   ├── CMakeLists.txt
│   │   ├── Window.cpp                # GLFW 窗口封装
│   │   └── Window.hpp
│   ├── renderer/
│   │   ├── CMakeLists.txt
│   │   ├── Renderer.hpp              # 渲染器抽象接口
│   │   └── ImGuiRenderer.hpp         # ImGui 渲染器
│   └── app/
│       ├── CMakeLists.txt
│       └── Application.hpp           # 应用主类
├── tests/
│   ├── CMakeLists.txt
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   └── math_test.cpp             # 数学库基础测试
│   └── platform/
│       ├── CMakeLists.txt
│       └── window_test.cpp           # 窗口创建测试
└── docs/
    └── PHASE0_DESIGN.md              # 本文档
```

---

## 2. CMakeLists.txt 完整写法

### 2.1 根目录 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(SoftGPU VERSION 0.1.0 LANGUAGES CXX)

# ============================================
# C++ 标准
# ============================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================
# 查找依赖（系统包）
# ============================================
# OpenGL (必须)
find_package(OpenGL REQUIRED)

# ============================================
# 第三方库（extern/ 子模块）
# ============================================
# GLFW - 从源码构建
add_subdirectory(extern/GLFW)
# glad - OpenGL loader
add_subdirectory(extern/glad)
# imgui - 从源码构建
add_subdirectory(extern/imgui)
# glm - header-only，直接包含路径即可
# stb - header-only，直接包含路径即可

# ============================================
# 头文件路径
# ============================================
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/glm
    ${CMAKE_SOURCE_DIR}/extern/glad/include
    ${CMAKE_SOURCE_DIR}/extern/GLFW/include
    ${CMAKE_SOURCE_DIR}/extern/imgui
    ${CMAKE_SOURCE_DIR}/extern/stb
)

# ============================================
# 子目录
# ============================================
add_subdirectory(src)
add_subdirectory(tests)

# ============================================
# 编译选项
# ============================================
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
```

### 2.2 src/CMakeLists.txt

```cmake
add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(renderer)
add_subdirectory(app)

# ============================================
# 主程序
# ============================================
add_executable(SoftGPU
    main.cpp
)

target_link_libraries(SoftGPU PRIVATE
    app
    platform
    renderer
    OpenGL::GL
    glfw
    imgui
)

# Windows: 链接 Windows 库
if(WIN32)
    target_link_libraries(SoftGPU PRIVATE
        dwmapi.lib
        imm32.lib
    )
endif()
```

### 2.3 src/core/CMakeLists.txt

```cmake
add_library(core STATIC
    Common.hpp
    Math.hpp
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### 2.4 src/platform/CMakeLists.txt

```cmake
add_library(platform STATIC
    Window.cpp
    Window.hpp
)

target_link_libraries(platform PUBLIC
    core
    glfw
    OpenGL::GL
)

target_include_directories(platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### 2.5 src/renderer/CMakeLists.txt

```cmake
add_library(renderer STATIC
    Renderer.hpp
    ImGuiRenderer.hpp
)

target_link_libraries(renderer PUBLIC
    core
    imgui
)

target_include_directories(renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### 2.6 src/app/CMakeLists.txt

```cmake
add_library(app STATIC
    Application.hpp
)

target_link_libraries(app PUBLIC
    core
    platform
    renderer
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### 2.7 tests/CMakeLists.txt

```cmake
include(GoogleTest)

add_subdirectory(core)
add_subdirectory(platform)
```

### 2.8 tests/core/CMakeLists.txt

```cmake
add_executable(math_test math_test.cpp)

target_link_libraries(math_test PRIVATE
    core
    gtest
)

include(GoogleTest)
gtest_discover_tests(math_test)
```

### 2.9 tests/platform/CMakeLists.txt

```cmake
add_executable(window_test window_test.cpp)

target_link_libraries(window_test PRIVATE
    platform
    glfw
    OpenGL::GL
    gtest
)

include(GoogleTest)
gtest_discover_tests(window_test)
```

---

## 3. 环境依赖说明

### 3.1 必须安装的系统依赖

#### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libx11-dev \
    libglfw3-dev \
    libglfw3 \
    libgtest-dev \
    libgmock-dev
```

#### macOS (Homebrew)

```bash
brew install cmake glfw googletest
```

#### Windows (vcpkg)

```bash
vcpkg install glfw3 gtest
```

### 3.2 extern 子模块初始化

首次克隆后执行：

```bash
cd SoftGPU
git submodule update --init --recursive
```

或者手动克隆子模块：

```bash
# GLFW
git clone --depth 1 https://github.com/glfw/glfw.git extern/GLFW

# glad (使用 glad-generator 生成，或直接下载)
git clone --depth 1 https://github.com/Dav1dde/glad.git extern/glad

# imgui
git clone --depth 1 https://github.com/ocornut/imgui.git extern/imgui

# glm (header-only)
git clone --depth 1 https://github.com/g-truc/glm.git extern/glm

# stb (单头文件)
mkdir -p extern/stb
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o extern/stb/stb_image.h
```

### 3.3 glad 特殊说明

glad 需要生成，不能直接 clone 使用：

```bash
# 方法1: 在线生成
# 访问 https://glad.dav1d.de/
#   - Language: C/C++
#   - Specification: OpenGL
#   - API: gl=4.6, gles1=off, gles2=off, glsc2=off
#   - Profile: Core
#   - Extensions: 留空
# 下载并解压到 extern/glad/

# 方法2: 使用 glad-cli
pip install glfw  # 不需要
curl -L https://github.com/Dav1dde/glad/archive/refs/heads/master.zip -o glad.zip
unzip glad.zip
cd glad
python -m glad --out-path=../glad_generated --generator=c --spec=gl --version=4.6 --profile=core
```

**注意：** glad 需要放在 `extern/glad/` 目录，且 include 路径指向 `extern/glad/include/`。

---

## 4. main.cpp 框架

```cpp
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
constexpr char* WINDOW_TITLE = "SoftGPU - Tile-Based GPU Simulator";
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
```

---

## 5. ImGui + GLFW 集成步骤（代码级说明）

### 5.1 依赖关系图

```
┌─────────────┐
│   main.cpp  │
└──────┬──────┘
       │
       ├── 依赖 GLFW (window)
       ├── 依赖 glad (OpenGL loader)
       ├── 依赖 ImGui (UI library)
       │       │
       │       ├── imgui_impl_glfw.cpp (Platform backend)
       │       └── imgui_impl_opengl3.cpp (Render backend)
       │
       └── 依赖 Application (业务层)
```

### 5.2 集成检查清单

**GLFW 初始化（必须按顺序）：**

1. `glfwInit()` — 初始化 GLFW
2. 设置窗口 hint（版本号、profile 等）
3. `glfwCreateWindow()` — 创建窗口
4. `glfwMakeContextCurrent()` — 绑定 OpenGL 上下文
5. `gladLoadGLLoader()` — 加载 OpenGL 函数指针

**ImGui 初始化（必须按顺序）：**

1. `ImGui::CreateContext()` — 创建 ImGui 上下文
2. 配置 `ImGuiIO`（可选）
3. `ImGui::StyleColorsDark()` 或其他主题
4. `ImGui_ImplGlfw_InitForOpenGL()` — 初始化 GLFW backend
5. `ImGui_ImplOpenGL3_Init()` — 初始化 OpenGL3 backend

**每帧渲染（必须按顺序）：**

1. `glfwPollEvents()` — 处理事件
2. `ImGui_ImplOpenGL3_NewFrame()` — 准备 OpenGL 渲染
3. `ImGui_ImplGlfw_NewFrame()` — 准备 GLFW 事件
4. `ImGui::NewFrame()` — 开始 ImGui 帧
5. 调用 ImGui UI 代码（`ImGui::Begin`, `ImGui::Text`, etc.）
6. `ImGui::Render()` — 结束 ImGui 帧
7. `glClear()` — 清屏
8. `ImGui_ImplOpenGL3_RenderDrawData()` — 绘制 ImGui 数据
9. `glfwSwapBuffers()` — 交换缓冲区

**退出清理（必须按顺序）：**

1. `ImGui_ImplOpenGL3_Shutdown()`
2. `ImGui_ImplGlfw_Shutdown()`
3. `ImGui::DestroyContext()`
4. `glfwDestroyWindow()`
5. `glfwTerminate()`

### 5.3 常见错误及解决

| 错误 | 原因 | 解决 |
|------|------|------|
| `Segmentation fault` | glad 未加载 | 确保 `gladLoadGLLoader` 在任何 OpenGL 调用之前成功 |
| 黑色窗口 | `glfwMakeContextCurrent` 未调用 | 创建窗口后立即调用 |
| ImGui 无响应 | 忘记调用 `ImGui_ImplGlfw_InitForOpenGL` | 传入正确的 window 和 true |
| 渲染错位 | 窗口 resize 未处理 | 添加 `glfwSetFramebufferSizeCallback` 并更新 viewport |

---

## 6. 第一个测试用例

### 6.1 测试目标

验证以下内容：
- glm 数学库正常工作
- stb_image 可正常加载
- 渲染器基础结构存在

### 6.2 tests/core/math_test.cpp

```cpp
#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ============================================================================
// Math Basic Test
// 测试 glm 库的基本功能
// ============================================================================

namespace {

using vec3 = glm::vec3;
using mat4 = glm::mat4;
using quat = glm::quat;

class MathTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// 向量测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, Vec3BasicOperations)
{
    vec3 a(1.0f, 2.0f, 3.0f);
    vec3 b(4.0f, 5.0f, 6.0f);

    // 加法
    vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);

    // 点积
    float dot = glm::dot(a, b);
    EXPECT_FLOAT_EQ(dot, 32.0f); // 1*4 + 2*5 + 3*6 = 32

    // 叉积
    vec3 cross = glm::cross(a, b);
    EXPECT_FLOAT_EQ(cross.x, -3.0f);  // 2*6 - 3*5 = -3
    EXPECT_FLOAT_EQ(cross.y, 6.0f);   // 3*4 - 1*6 = 6
    EXPECT_FLOAT_EQ(cross.z, -3.0f);  // 1*5 - 2*4 = -3
}

TEST_F(MathTest, Vec3Normalization)
{
    vec3 v(3.0f, 4.0f, 0.0f);
    vec3 n = glm::normalize(v);

    EXPECT_FLOAT_EQ(glm::length(n), 1.0f);
    EXPECT_FLOAT_EQ(n.x, 0.6f);
    EXPECT_FLOAT_EQ(n.y, 0.8f);
}

// ---------------------------------------------------------------------------
// 矩阵测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, Mat4Identity)
{
    mat4 identity = mat4(1.0f);

    // 对角线为 1，其他为 0
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_FLOAT_EQ(identity[i][j], expected);
        }
    }
}

TEST_F(MathTest, Mat4PerspectiveProjection)
{
    float aspect = 1280.0f / 720.0f;
    float fov = glm::radians(45.0f);
    float near = 0.1f;
    float far = 100.0f;

    mat4 proj = glm::perspective(fov, aspect, near, far);

    // 验证 proj[1][1] = 1/tan(fov/2) = 1/tan(22.5°) ≈ 2.414
    float expected_cot = 1.0f / glm::tan(fov * 0.5f);
    EXPECT_FLOAT_EQ(proj[1][1], expected_cot);
}

TEST_F(MathTest, Mat4Translation)
{
    vec3 translate(1.0f, 2.0f, 3.0f);
    mat4 T = glm::translate(mat4(1.0f), translate);

    // 验证平移矩阵
    EXPECT_FLOAT_EQ(T[3][0], 1.0f);
    EXPECT_FLOAT_EQ(T[3][1], 2.0f);
    EXPECT_FLOAT_EQ(T[3][2], 3.0f);
}

TEST_F(MathTest, Mat4TransformChain)
{
    // 平移 * 旋转 * 缩放
    vec3 translation(1.0f, 0.0f, 0.0f);
    vec3 scale(2.0f, 2.0f, 2.0f);

    mat4 T = glm::translate(mat4(1.0f), translation);
    mat4 S = glm::scale(mat4(1.0f), scale);

    mat4 M = S * T; // 先平移后缩放

    // 验证：点 (1, 0, 0) 经变换后为 (4, 0, 0)
    // T * (1,0,0,1) = (2,0,0,1)
    // S * (2,0,0,1) = (4,0,0,1)
    vec4 p(1.0f, 0.0f, 0.0f, 1.0f);
    vec4 result = M * p;

    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

// ---------------------------------------------------------------------------
// 四元数测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, QuatBasicRotation)
{
    // 绕 Z 轴旋转 90 度
    quat q = glm::angleAxis(glm::radians(90.0f), vec3(0.0f, 0.0f, 1.0f));

    vec3 v(1.0f, 0.0f, 0.0f);
    vec3 rotated = q * v;

    // 绕 Z 轴旋转 90° 后，(1,0,0) 应变为 (0,1,0)
    EXPECT_NEAR(rotated.x, 0.0f, 0.001f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.001f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.001f);
}

TEST_F(MathTest, QuatNormalization)
{
    quat q(1.0f, 2.0f, 3.0f, 4.0f);
    quat n = glm::normalize(q);

    EXPECT_NEAR(glm::length(n), 1.0f, 0.0001f);
}

TEST_F(MathTest, QuatToMat4)
{
    // 单位四元数应转换为单位矩阵
    quat q(1.0f, 0.0f, 0.0f, 0.0f); // w=1, 其他为 0
    mat4 m = glm::mat4_cast(q);

    mat4 identity = mat4(1.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(m[i][j], identity[i][j]);
        }
    }
}

// ---------------------------------------------------------------------------
// 数学常量测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, MathConstants)
{
    // PI 值验证
    EXPECT_NEAR(glm::pi<float>(), 3.14159265358979f, 0.0001f);

    // epsilon 值合理
    EXPECT_GT(glm::epsilon<float>(), 0.0f);
    EXPECT_LT(glm::epsilon<float>(), 1e-5f);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 主函数（由 GoogleTest 提供）
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

### 6.3 tests/platform/window_test.cpp

```cpp
#include <gtest/gtest.h>
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
```

---

## 7. P0 验收标准

### 7.1 验收检查清单

#### ✅ 环境依赖（必须满足）

| 编号 | 检查项 | 验证方法 | 预期结果 |
|------|--------|----------|----------|
| P0-ENV-1 | CMake >= 3.16 | `cmake --version` | 输出 3.16+ |
| P0-ENV-2 | C++ 编译器支持 C++17 | `g++ --version` | GCC 9+ / Clang 10+ / MSVC 19+ |
| P0-ENV-3 | OpenGL 库存在 | `pkg-config --exists gl` | 返回成功 |
| P0-ENV-4 | GLFW 库存在 | `pkg-config --exists glfw3` | 返回成功 |
| P0-ENV-5 | GoogleTest 库存在 | `pkg-config --exists gtest` | 返回成功 |
| P0-ENV-6 | 所有 extern 子模块已 clone | `ls extern/*/` | 5 个子目录存在 |

#### ✅ 构建成功（必须满足）

| 编号 | 检查项 | 验证方法 | 预期结果 |
|------|--------|----------|----------|
| P0-BUILD-1 | 顶层 CMake 配置成功 | `cmake -B build` | 输出 "Configuring done" |
| P0-BUILD-2 | 编译无错误 | `cmake --build build` | 无 error 输出 |
| P0-BUILD-3 | 主程序生成 | `ls build/bin/SoftGPU` | 文件存在 |
| P0-BUILD-4 | 测试可执行文件生成 | `ls build/tests/core/math_test` | 文件存在 |
| P0-BUILD-5 | 库文件生成 | `ls build/lib/*.a` 或 `*.so` | 至少 3 个库 |

#### ✅ 运行验证（必须满足）

| 编号 | 检查项 | 验证方法 | 预期结果 |
|------|--------|----------|----------|
| P0-RUN-1 | 主程序启动 | `./build/bin/SoftGPU` | 窗口显示，5秒内无崩溃 |
| P0-RUN-2 | 窗口标题正确 | 检查窗口标题栏 | 显示 "SoftGPU - Tile-Based GPU Simulator" |
| P0-RUN-3 | OpenGL 版本输出 | 终端查看 stdout | 输出 "OpenGL Version: 4.x" |
| P0-RUN-4 | ImGui UI 可见 | 窗口中有深色面板 | 显示 Status、FPS 等信息 |
| P0-RUN-5 | 程序可正常退出 | 点击 Exit 或关闭窗口 | 无段错误，输出 "shutdown gracefully" |

#### ✅ 测试验证（必须满足）

| 编号 | 检查项 | 验证方法 | 预期结果 |
|------|--------|----------|----------|
| P0-TEST-1 | math_test 通过 | `./build/tests/core/math_test` | 输出 "[ PASSED ]" |
| P0-TEST-2 | 所有 math_test 用例通过 | 统计 PASSED 数量 | >= 7 个测试通过 |
| P0-TEST-3 | window_test 可执行 | `./build/tests/platform/window_test` | 无 SIGSEGV |
| P0-TEST-4 | GoogleTest 发现测试 | `ctest -N` | 列出 >= 7 个测试 |

#### ✅ 代码质量（必须满足）

| 编号 | 检查项 | 验证方法 | 预期结果 |
|------|--------|----------|----------|
| P0-QUAL-1 | 无编译警告 | `cmake --build build 2>&1 | grep warning` | 无 warning 输出 |
| P0-QUAL-2 | 目录结构符合设计 | `find SoftGPU/src -type f -name "*.hpp" -o -name "*.cpp"` | 至少 8 个源文件 |
| P0-QUAL-3 | main.cpp 包含所有必需部分 | `grep -c "ImGui\|glfw\|GLAD" main.cpp` | >= 3 处匹配 |

### 7.2 快速验收脚本

```bash
#!/bin/bash
# validate_phase0.sh - PHASE0 验收脚本

set -e

cd /path/to/SoftGPU

echo "========================================"
echo "PHASE0 验收测试"
echo "========================================"

# 1. 环境检查
echo "[1/5] 检查环境依赖..."
command -v cmake >/dev/null 2>&1 || { echo "CMake 未安装"; exit 1; }
command -v g++ >/dev/null 2>&1 || { echo "G++ 未安装"; exit 1; }

# 2. CMake 配置
echo "[2/5] CMake 配置..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
echo "[3/5] 编译..."
cmake --build build -j$(nproc)

# 4. 检查产物
echo "[4/5] 检查产物..."
test -f build/bin/SoftGPU || { echo "主程序未生成"; exit 1; }
test -f build/tests/core/math_test || { echo "测试程序未生成"; exit 1; }

# 5. 运行测试
echo "[5/5] 运行测试..."
./build/tests/core/math_test

echo "========================================"
echo "PHASE0 验收通过 ✓"
echo "========================================"
```

### 7.3 预期里程碑时间

| 阶段 | 预计时长 | 交付物 |
|------|----------|--------|
| PHASE0 环境搭建 | 1-2 天 | 可编译运行的空壳项目 |
| PHASE1 核心架构 | 2-3 周 | Tile-Based 渲染管线基础 |
| PHASE2 图形 API | 2-3 周 | OpenGL/Vulkan 命令实现 |
| PHASE3 性能分析 | 2-3 周 | Profiler + 可视化 |
| PHASE4 可视化调优 | 2-3 周 | 交互式调试工具 |
| PHASE5 文档+发布 | 1 周 | 完整文档 + Release |

---

## 附录 A: 常见问题 FAQ

### Q1: 编译时报 `GLFW not found`
**A:** 安装 `libglfw3-dev` 或使用 git submodule 中的 GLFW 源码

### Q2: 编译时报 `cannot find -lglad`
**A:** glad 需要手动生成，参见第 3.3 节

### Q3: 运行时窗口闪退
**A:** 检查终端输出，可能是 `gladLoadGLLoader` 失败

### Q4: ImGui 无响应
**A:** 确保调用了 `ImGui_ImplGlfw_InitForOpenGL(window, true)`

### Q5: 测试在 CI 中跳过
**A:** window_test 需要显示设备，在无头环境中跳过是预期行为

---

## 附录 B: 参考资源

- GLFW 文档: https://www.glfw.org/documentation.html
- ImGui 文档: https://github.com/ocornut/imgui/wiki
- glm 文档: https://glm.g-truc.net/0.9.9/index.html
- GoogleTest: https://google.github.io/googletest/

---

**文档结束**
