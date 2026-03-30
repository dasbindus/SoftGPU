// ============================================================================
// SoftGPU - Application.hpp
// 应用主类
// ============================================================================

#pragma once

#include <memory>

#include "core/Common.hpp"
#include "platform/Window.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/ImGuiRenderer.hpp"

namespace SoftGPU {

// ============================================================================
// ApplicationConfig - 应用配置
// ============================================================================
struct ApplicationConfig {
    WindowConfig window;
    bool enableImGui = true;
    bool enableDebug = false;
    std::string_view applicationName = "SoftGPU";
};

// ============================================================================
// IApplication - 应用接口
// ============================================================================
class IApplication {
public:
    virtual ~IApplication() = default;

    // ========================================================================
    // 应用生命周期
    // ========================================================================

    // 应用初始化（创建窗口、初始化渲染器等）
    virtual bool initialize(const ApplicationConfig& config) = 0;

    // 应用主循环
    virtual void run() = 0;

    // 应用退出
    virtual void exit() = 0;

    // 应用清理
    virtual void shutdown() = 0;

    // ========================================================================
    // 状态查询
    // ========================================================================

    virtual bool isRunning() const = 0;
    virtual bool isPaused() const = 0;

    // ========================================================================
    // 窗口访问
    // ========================================================================

    virtual Window& getWindow() = 0;
    virtual const Window& getWindow() const = 0;

    // ========================================================================
    // 渲染器访问
    // ========================================================================

    virtual IRenderer* getRenderer() = 0;
    virtual ImGuiRenderer* getImGuiRenderer() = 0;
};

// ============================================================================
// Application - 默认应用实现
// ============================================================================
class Application : public IApplication {
public:
    SOFTGPU_NON_COPYABLE_AND_MOVABLE(Application)

    Application();
    ~Application() override;

    // ========================================================================
    // IApplication 接口
    // ========================================================================

    bool initialize(const ApplicationConfig& config) override;
    void run() override;
    void exit() override;
    void shutdown() override;

    bool isRunning() const override { return m_running && !m_exiting; }
    bool isPaused() const override { return m_paused; }

    Window& getWindow() override { return m_window; }
    const Window& getWindow() const override { return m_window; }

    IRenderer* getRenderer() override { return nullptr; }  // PHASE0 返回 nullptr
    ImGuiRenderer* getImGuiRenderer() override { return m_imGuiRenderer.get(); }

    // ========================================================================
    // 帧回调（可由子类重写）
    // ========================================================================

    // 每帧更新（逻辑）
    virtual void onUpdate([[maybe_unused]] float deltaTime) {}

    // 每帧渲染
    virtual void onRender() {}

    // 每帧结束后调用
    virtual void onFrameEnd() {}

    // ========================================================================
    // 应用控制
    // ========================================================================

    // 暂停/恢复
    void pause();
    void resume();

    // 设置时间缩放
    void setTimeScale(float scale) { m_timeScale = scale; }
    float getTimeScale() const { return m_timeScale; }

    // 获取帧时间
    float getDeltaTime() const { return m_deltaTime; }
    float getTotalTime() const { return m_totalTime; }
    float getFPS() const { return m_fps; }

    // 获取应用配置
    const ApplicationConfig& getConfig() const { return m_config; }

protected:
    // 窗口配置获取
    WindowConfig getDefaultWindowConfig() const;

private:
    void mainLoop();
    void updateTime();
    void processInput();

    ApplicationConfig m_config;
    Window m_window;
    std::unique_ptr<ImGuiRenderer> m_imGuiRenderer;

    bool m_initialized = false;
    bool m_running = false;
    bool m_exiting = false;
    bool m_paused = false;

    float m_timeScale = 1.0f;
    float m_deltaTime = 0.0f;
    float m_totalTime = 0.0f;
    float m_fps = 0.0f;
    float m_frameCount = 0.0f;
    float m_fpsUpdateTimer = 0.0f;

    static constexpr float FPS_UPDATE_INTERVAL = 0.5f;  // 每 0.5 秒更新一次 FPS
};

// ============================================================================
// 全局应用实例获取
// ============================================================================
IApplication* getApplication();
void setApplication(IApplication* app);

} // namespace SoftGPU
