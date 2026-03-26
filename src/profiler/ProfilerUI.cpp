// ============================================================================
// SoftGPU - ProfilerUI.cpp
// ImGui-based profiler visualization panel
// PHASE5: Performance analysis and bottleneck detection
// ============================================================================

#include "ProfilerUI.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace SoftGPU {

// ============================================================================
// ProfilerUI
// ============================================================================
ProfilerUI::ProfilerUI() {
}

ProfilerUI::~ProfilerUI() {
}

void ProfilerUI::setFrameProfiler(FrameProfiler* profiler) {
    m_profiler = profiler;
}

void ProfilerUI::setBottleneckDetector(BottleneckDetector* detector) {
    m_detector = detector;
}

void ProfilerUI::render() {
    if (!m_visible) return;
    if (!m_profiler) return;

    ImGui::SetNextWindowSizeConstraints({400, 300}, {1200, 900});
    if (!ImGui::Begin("SoftGPU Profiler##PHASE5", &m_visible, WINDOW_FLAGS)) {
        ImGui::End();
        return;
    }

    // --- Summary ---
    double fps = m_profiler->getFps();
    double frameMs = m_profiler->getFrameTimeMs();
    ImGui::Text("Frame Time: %.2f ms   FPS: %.1f", frameMs, fps);
    ImGui::Separator();

    // --- Bottleneck Indicator ---
    renderBottleneckIndicator();
    ImGui::Separator();

    // --- Pipeline Architecture Diagram ---
    ImGui::Text("Pipeline Stages:");
    renderPipelineDiagram();
    ImGui::Separator();

    // --- Time Breakdown Chart ---
    ImGui::Text("Time Breakdown:");
    renderTimeChart();
    ImGui::Separator();

    // --- Detailed Stats Table ---
    if (ImGui::CollapsingHeader("Detailed Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(5, "stage_stats", true);
        ImGui::SetColumnWidth(0, 140.0f);
        ImGui::SetColumnWidth(1, 70.0f);
        ImGui::SetColumnWidth(2, 70.0f);
        ImGui::SetColumnWidth(3, 80.0f);
        ImGui::SetColumnWidth(4, 60.0f);

        ImGui::Text("Stage");       ImGui::NextColumn();
        ImGui::Text("Invocations"); ImGui::NextColumn();
        ImGui::Text("Cycles");      ImGui::NextColumn();
        ImGui::Text("Time (ms)");   ImGui::NextColumn();
        ImGui::Text("%%");          ImGui::NextColumn();
        ImGui::Separator();

        auto allStats = m_profiler->getAllStats();
        for (size_t i = 0; i < STAGE_COUNT; ++i) {
            StageHandle stage = static_cast<StageHandle>(i);
            const ProfilerStats& s = allStats[i];
            StageColor col = StageColor::fromPercent(s.percent);
            ImGui::TextColored(ImVec4(col.r, col.g, col.b, 1.0f), "%s",
                             IProfiler::stageToString(stage));
            ImGui::NextColumn();
            ImGui::Text("%llu", (unsigned long long)s.invocations);
            ImGui::NextColumn();
            ImGui::Text("%llu", (unsigned long long)s.cycles);
            ImGui::NextColumn();
            ImGui::Text("%.3f", s.ms);
            ImGui::NextColumn();
            ImGui::Text("%.1f%%", s.percent);
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    // --- Bottleneck Scores ---
    if (m_detector && ImGui::CollapsingHeader("Bottleneck Scores")) {
        ImGui::Text("Shader Bound:   %.1f%%", m_detector->getShaderBoundScore() * 100.0f);
        ImGui::Text("Memory Bound:   %.1f%%", m_detector->getMemoryBoundScore() * 100.0f);
        ImGui::Text("FillRate Bound: %.1f%%", m_detector->getFillRateBoundScore() * 100.0f);
        ImGui::Text("Compute Bound:  %.1f%%", m_detector->getComputeBoundScore() * 100.0f);
    }

    ImGui::End();
}

void ProfilerUI::renderBottleneckIndicator() const {
    if (!m_profiler) return;

    BottleneckResult result = m_profiler->detectBottleneck();

    ImVec4 color;
    const char* label;
    switch (result.type) {
        case BottleneckType::ShaderBound:    color = ImVec4(0.9f, 0.3f, 0.1f, 1.0f); label = "Shader Bound";   break;
        case BottleneckType::MemoryBound:    color = ImVec4(0.2f, 0.6f, 0.9f, 1.0f); label = "Memory Bound";   break;
        case BottleneckType::FillRateBound:  color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f); label = "Fill Rate Bound"; break;
        case BottleneckType::ComputeBound:   color = ImVec4(0.5f, 0.8f, 0.3f, 1.0f); label = "Compute Bound";  break;
        default:                             color = ImVec4(0.5f, 0.9f, 0.5f, 1.0f); label = "Balanced";       break;
    }

    ImGui::Text("Bottleneck: ");
    ImGui::SameLine();
    ImGui::TextColored(color, "%s", label);
    ImGui::SameLine();
    ImGui::Text(" (confidence: %.0f%%, severity: %.0f%%)",
                result.confidence * 100.0f, result.severity * 100.0f);
    ImGui::Text("Info: %s", result.description.c_str());

    if (m_detector) {
        ImGui::Text("Recommendation: %s",
                    m_detector->getRecommendation(result).c_str());
    }
}

void ProfilerUI::renderPipelineDiagram() const {
    if (!m_profiler) return;

    const char* stageNames[] = {
        "CmdProc", "VShader", "PAssembly",
        "Raster", "FShader", "Framebuffer",
        "TileWB", "Tiling"
    };

    auto stats = m_profiler->getAllStats();

    // Find max ms for scaling
    double maxMs = 0.0;
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        maxMs = std::max(maxMs, stats[i].ms);
    }
    if (maxMs <= 0.0) maxMs = 1.0;

    // Draw horizontal bar per stage
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        StageHandle stage = static_cast<StageHandle>(i);
        StageColor col = StageColor::fromPercent(stats[i].percent);

        ImGui::Text("%-10s", stageNames[i]);
        ImGui::SameLine();
        float barWidth = static_cast<float>(stats[i].ms / maxMs) * 200.0f;
        ImGui::SameLine(100.0f, 0.0f);
        ImGui::TextColored(ImVec4(col.r, col.g, col.b, 1.0f),
                           "[%6.2f%%] %s",
                           stats[i].percent,
                           IProfiler::stageToString(stage));
        ImGui::SameLine();
        ImGui::Text(" %.3f ms", stats[i].ms);
    }
}

void ProfilerUI::renderTimeChart() const {
    if (!m_profiler) return;

    auto stats = m_profiler->getAllStats();

    // Total frame time
    double totalMs = m_profiler->getFrameTimeMs();
    if (totalMs <= 0.0) totalMs = 1.0;

    const char* stageNames[] = {
        "CommandProcessor", "VertexShader", "PrimitiveAssembly",
        "Rasterizer", "FragmentShader", "Framebuffer",
        "TileWriteBack", "Tiling"
    };

    // Draw stacked bar chart (simulate with colored segments)
    static const ImU32 colors[] = {
        IM_COL32(100, 180, 255, 255),  // blue
        IM_COL32(100, 255, 150, 255),  // green
        IM_COL32(255, 200, 100, 255),  // orange
        IM_COL32(255, 100, 100, 255),  // red
        IM_COL32(200, 100, 255, 255),  // purple
        IM_COL32(255, 255, 100, 255),  // yellow
        IM_COL32(100, 255, 255, 255),  // cyan
        IM_COL32(200, 200, 200, 255),  // gray
    };

    constexpr float BAR_HEIGHT = 24.0f;
    constexpr float CHART_WIDTH = 350.0f;
    const ImU32 BG = IM_COL32(40, 40, 40, 255);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Draw background
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + CHART_WIDTH, canvasPos.y + BAR_HEIGHT), BG);

    // Draw segments
    float cursorX = canvasPos.x;
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        float width = static_cast<float>((stats[i].ms / totalMs) * CHART_WIDTH);
        if (width < 1.0f) continue;
        drawList->AddRectFilled(
            ImVec2(cursorX, canvasPos.y),
            ImVec2(cursorX + width, canvasPos.y + BAR_HEIGHT),
            colors[i % 8]);
        cursorX += width;
    }

    // Border
    drawList->AddRect(canvasPos,
        ImVec2(canvasPos.x + CHART_WIDTH, canvasPos.y + BAR_HEIGHT),
        IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(ImVec2(CHART_WIDTH, BAR_HEIGHT));

    // Legend
    ImGui::Columns(4, "legend", true);
    for (size_t i = 0; i < STAGE_COUNT; ++i) {
        ImVec4 col = ImColor(colors[i % 8]);
        ImGui::ColorButton("##", col, ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
        ImGui::Text("%s %.1f%%", stageNames[i], stats[i].percent);
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

}  // namespace SoftGPU
