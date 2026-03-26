// ============================================================================
// SoftGPU - Window.cpp
// GLFW 窗口封装实现
// ============================================================================

#include "Window.hpp"

#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>
#include <string>

namespace SoftGPU {

// ============================================================================
// GLFW 初始化状态
// ============================================================================
static bool s_glfwInitialized = false;

// ============================================================================
// 辅助函数
// ============================================================================
namespace {

void glfwErrorCallback(int error, const char* description) {
    fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

} // anonymous namespace

// ============================================================================
// 全局 GLFW 初始化
// ============================================================================
bool initGLFW() {
    if (s_glfwInitialized) {
        return true;
    }

    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        fprintf(stderr, "[ERROR] Failed to initialize GLFW\n");
        return false;
    }

    s_glfwInitialized = true;
    fprintf(stdout, "[INFO] GLFW initialized successfully\n");
    return true;
}

void terminateGLFW() {
    if (s_glfwInitialized) {
        glfwTerminate();
        s_glfwInitialized = false;
        fprintf(stdout, "[INFO] GLFW terminated\n");
    }
}

bool isGLFWInitialized() {
    return s_glfwInitialized;
}

// ============================================================================
// 窗口实现
// ============================================================================

Window::~Window() {
    destroy();
}

bool Window::create(const WindowConfig& config) {
    // 确保 GLFW 已初始化
    if (!initGLFW()) {
        return false;
    }

    // 保存配置
    m_config = config;

    // 设置窗口 hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, config.glVersionMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, config.glVersionMinor);

    if (config.coreProfile) {
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    } else {
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_OPENGL_ANY_PROFILE);
    }

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, config.forwardCompat ? GLFW_TRUE : GLFW_FALSE);

    if (config.debugContext) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    }

    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUSED, config.focused ? GLFW_TRUE : GLFW_FALSE);

    glfwWindowHint(GLFW_SAMPLES, config.samples);

    // 设置尺寸限制
    if (config.minWidth > 0 && config.minHeight > 0) {
        glfwWindowHint(GLFW_MIN_WIDTH, config.minWidth);
        glfwWindowHint(GLFW_MIN_HEIGHT, config.minHeight);
    }
    if (config.maxWidth > 0 && config.maxHeight > 0) {
        glfwWindowHint(GLFW_MAX_WIDTH, config.maxWidth);
        glfwWindowHint(GLFW_MAX_HEIGHT, config.maxHeight);
    }

    // 创建窗口
    m_window = glfwCreateWindow(
        config.width,
        config.height,
        config.title.data(),
        nullptr,  // monitor
        nullptr    // share
    );

    if (!m_window) {
        fprintf(stderr, "[ERROR] Failed to create GLFW window\n");
        return false;
    }

    // 设置上下文
    glfwMakeContextCurrent(m_window);

    fprintf(stdout, "[INFO] Window created: %dx%d, OpenGL %d.%d\n",
            config.width, config.height,
            config.glVersionMajor, config.glVersionMinor);

    return true;
}

void Window::destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        fprintf(stdout, "[INFO] Window destroyed\n");
    }
}

bool Window::shouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::setShouldClose(bool value) {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, value ? GLFW_TRUE : GLFW_FALSE);
    }
}

int Window::getWidth() const {
    int width = 0;
    if (m_window) {
        glfwGetWindowSize(m_window, &width, nullptr);
    }
    return width;
}

int Window::getHeight() const {
    int height = 0;
    if (m_window) {
        glfwGetWindowSize(m_window, nullptr, &height);
    }
    return height;
}

void Window::getSize(int& width, int& height) const {
    if (m_window) {
        glfwGetWindowSize(m_window, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}

void Window::getFramebufferSize(int& width, int& height) const {
    if (m_window) {
        glfwGetFramebufferSize(m_window, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}

void Window::setTitle(std::string_view title) {
    if (m_window) {
        glfwSetWindowTitle(m_window, title.data());
    }
}

void Window::swapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents() {
    glfwWaitEvents();
}

void Window::waitEventsTimeout(double timeout) {
    glfwWaitEventsTimeout(timeout);
}

void Window::show() {
    if (m_window) {
        glfwShowWindow(m_window);
    }
}

void Window::hide() {
    if (m_window) {
        glfwHideWindow(m_window);
    }
}

void Window::focus() {
    if (m_window) {
        glfwFocusWindow(m_window);
    }
}

void Window::maximize() {
    if (m_window) {
        glfwMaximizeWindow(m_window);
    }
}

void Window::minimize() {
    if (m_window) {
        glfwIconifyWindow(m_window);
    }
}

void Window::restore() {
    if (m_window) {
        glfwRestoreWindow(m_window);
    }
}

void Window::setSwapInterval(int interval) {
    if (m_window) {
        glfwSwapInterval(interval);
    }
}

GLFWglproc Window::getGLFWProcAddress(const char* name) {
    return glfwGetProcAddress(name);
}

// ============================================================================
// 全局函数实现
// ============================================================================

void getPrimaryMonitorWorkarea(int& width, int& height, int& x, int& y) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode) {
            width = mode->width;
            height = mode->height;
        }
        glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
    } else {
        width = 1920;
        height = 1080;
        x = 0;
        y = 0;
    }
}

Window* getCurrentContextWindow() {
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    if (!glfwWindow) {
        return nullptr;
    }
    // 注意：这个函数不能直接获取用户指针，只能返回当前上下文对应的窗口
    // 如果需要关联 Window*，需要在创建时设置 glfwSetWindowUserPointer
    return nullptr; // 需要在外部管理映射
}

} // namespace SoftGPU
