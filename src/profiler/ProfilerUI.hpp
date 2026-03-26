// ============================================================================
// SoftGPU - ProfilerUI.hpp
// ImGui-based profiler visualization panel
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#pragma once

#include "IProfiler.hpp"
#include "FrameProfiler.hpp"
#include "BottleneckDetector.hpp"
#include <imgui.h>

namespace SoftGPU {

// ============================================================================
// StageColor - Color coding for pipeline visualization
// ============================================================================
struct StageColor {
    float r, g, b;  // 0.0 ~ 1.0

    static StageColor green()  { return {0.2f, 0.8f, 0.2f}; }  // < 50%
    static StageColor yellow() { return {1.0f, 0.8f, 0.0f}; }  // 50% ~ 80%
    static StageColor red()    { return {0.9f, 0.2f, 0.1f}; }  // >= 80%
    static StageColor gray()   { return {0.5f, 0.5f, 0.5f}; }  // unknown/0%

    static StageColor fromPercent(double percent) {
        if (percent < 50.0) return green();
        if (percent < 80.0) return yellow();
        return red();
    }
};

// ============================================================================
// ProfilerUI - ImGui panel for profiler visualization
// ============================================================================
class ProfilerUI {
public:
    ProfilerUI();
    ~ProfilerUI();

    // Set profiler source
    void setFrameProfiler(FrameProfiler* profiler);
    void setBottleneckDetector(BottleneckDetector* detector);

    // Render the ImGui panel (call each frame)
    void render();

    // Show/hide the profiler window
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

    // Toggle visibility
    void toggle() { m_visible = !m_visible; }

    // Show architecture diagram with color-coded stages
    void renderPipelineDiagram() const;

    // Render time breakdown bar chart
    void renderTimeChart() const;

    // Render bottleneck indicator
    void renderBottleneckIndicator() const;

private:
    FrameProfiler*    m_profiler = nullptr;
    BottleneckDetector* m_detector = nullptr;
    bool              m_visible = true;

    // ImGui window flags
    static constexpr ImGuiWindowFlags WINDOW_FLAGS =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings;
};

}  // namespace SoftGPU
