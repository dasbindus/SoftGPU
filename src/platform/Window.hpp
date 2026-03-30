// ============================================================================
// SoftGPU - Window.hpp
// GLFW 窗口封装
// ============================================================================

#pragma once

#include "core/Common.hpp"
#include <GLFW/glfw3.h>
#include <string_view>

namespace SoftGPU {

// ============================================================================
// WindowConfig - 窗口配置
// ============================================================================
struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string_view title = "SoftGPU";
    bool visible = true;
    bool resizable = true;
    bool decorated = true;
    bool focused = true;
    int minWidth = -1;   // -1 means no minimum
    int minHeight = -1;
    int maxWidth = -1;
    int maxHeight = -1;

    // OpenGL 上下文配置
    int glVersionMajor = 4;
    int glVersionMinor = 6;
    bool coreProfile = true;
    bool forwardCompat = true;
    bool debugContext = false;

    // 多采样
    int samples = 4;
};

// ============================================================================
// Window - GLFW 窗口封装类
// ============================================================================
class Window {
public:
    SOFTGPU_NON_COPYABLE_AND_MOVABLE(Window)

    Window() = default;
    ~Window();

    // 创建窗口
    bool create(const WindowConfig& config);

    // 销毁窗口
    void destroy();

    // 是否应该关闭
    bool shouldClose() const;

    // 设置关闭标志
    void setShouldClose(bool value);

    // 获取原生窗口指针
    GLFWwindow* getNativeHandle() const { return m_window; }

    // 获取窗口尺寸
    int getWidth() const;
    int getHeight() const;
    void getSize(int& width, int& height) const;

    // 获取帧缓冲区尺寸（考虑 DPI）
    void getFramebufferSize(int& width, int& height) const;

    // 获取窗口配置
    const WindowConfig& getConfig() const { return m_config; }

    // 获取 OpenGL 版本
    int getGLVersionMajor() const { return m_config.glVersionMajor; }
    int getGLVersionMinor() const { return m_config.glVersionMinor; }

    // 设置窗口标题
    void setTitle(std::string_view title);

    // 交换缓冲区
    void swapBuffers();

    // 轮询事件
    void pollEvents();

    // 等待事件（阻塞）
    void waitEvents();

    // 等待事件超时
    void waitEventsTimeout(double timeout);

    // 显示/隐藏窗口
    void show();
    void hide();

    // 聚焦窗口
    void focus();

    // 最大化/最小化/还原
    void maximize();
    void minimize();
    void restore();

    // 设置垂直同步
    void setSwapInterval(int interval);

    // 获取 GLFW 工厂函数（用于 glad）
    static GLFWglproc getGLFWProcAddress(const char* name);

    // 窗口是否有效
    explicit operator bool() const { return m_window != nullptr; }
    bool isValid() const { return m_window != nullptr; }

private:
    GLFWwindow* m_window = nullptr;
    WindowConfig m_config;
};

// ============================================================================
// 便捷函数
// ============================================================================

// 初始化 GLFW（全局一次）
bool initGLFW();
void terminateGLFW();
bool isGLFWInitialized();

// 获取主显示器工作区尺寸
void getPrimaryMonitorWorkarea(int& width, int& height, int& x, int& y);

// 获取当前 OpenGL 上下文对应的窗口
Window* getCurrentContextWindow();

} // namespace SoftGPU
