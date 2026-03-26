// ============================================================================
// SoftGPU - ImGuiRenderer.hpp
// ImGui 渲染器封装
// ============================================================================

#pragma once

#include <string>
#include "Renderer.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace SoftGPU {

// ============================================================================
// ImGuiRenderer - ImGui 渲染器封装
// ============================================================================
class ImGuiRenderer {
public:
    SOFTGPU_NON_COPYABLE_AND_MOVABLE(ImGuiRenderer)

    ImGuiRenderer();
    ~ImGuiRenderer();

    // ========================================================================
    // 初始化
    // ========================================================================

    // 初始化 ImGui（需要 OpenGL 上下文已创建）
    bool initialize(GLFWwindow* window);

    // 销毁 ImGui
    void shutdown();

    // 是否已初始化
    bool isInitialized() const { return m_initialized; }

    // ========================================================================
    // 帧管理
    // ========================================================================

    // 开始新 ImGui 帧
    void newFrame();

    // 渲染 ImGui（调用此函数后会绘制 ImGui）
    void render();

    // 结束 ImGui 帧并绘制
    void endFrame();

    // ========================================================================
    // 配置
    // ========================================================================

    // 设置 GLSL 版本字符串
    void setGLSLVersion(std::string_view version);

    // 获取 GLSL 版本
    const char* getGLSLVersion() const { return m_glslVersion.c_str(); }

    // 是否启用 Docking
    void enableDocking(bool enable);
    bool isDockingEnabled() const { return m_dockingEnabled; }

    // 是否启用视图端口
    void enableViewports(bool enable);
    bool isViewportsEnabled() const { return m_viewportsEnabled; }

    // ========================================================================
    // 样式
    // ========================================================================

    // 设置深色主题
    void setDarkTheme();

    // 设置浅色主题
    void setLightTheme();

    // ========================================================================
    // IO
    // ========================================================================

    ImGuiIO& getIO() { return ImGui::GetIO(); }
    const ImGuiIO& getIO() const { return ImGui::GetIO(); }

private:
    bool m_initialized = false;
    bool m_dockingEnabled = true;
    bool m_viewportsEnabled = false;
    std::string m_glslVersion = "#version 460 core";

    GLFWwindow* m_window = nullptr;

    // 内部初始化（分离为单个函数以便错误处理）
    bool initBackend();
    void shutdownBackend();
};

// ============================================================================
// 便捷函数
// ============================================================================

// 便捷创建 ImGuiRenderer
std::unique_ptr<ImGuiRenderer> createImGuiRenderer();

} // namespace SoftGPU
