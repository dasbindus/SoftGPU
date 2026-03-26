// ============================================================================
// SoftGPU - Framebuffer.hpp
// 帧缓冲
// ============================================================================

#pragma once

#include "IStage.hpp"
#include "core/PipelineTypes.hpp"
#include <vector>
#include <cstring>

namespace SoftGPU {

// ============================================================================
// Framebuffer - 帧缓冲
// 职责：管理 color buffer 和 depth buffer，执行 Z-test
// ============================================================================
class Framebuffer : public IStage {
public:
    static constexpr uint32_t WIDTH       = FRAMEBUFFER_WIDTH;
    static constexpr uint32_t HEIGHT      = FRAMEBUFFER_HEIGHT;
    static constexpr uint32_t PIXEL_COUNT = WIDTH * HEIGHT;

    Framebuffer();
    ~Framebuffer();

    // 设置输入（来自 FragmentShader）
    void setInput(const std::vector<Fragment>& fragments);

    // 设置来自前一阶段的输入（由 connectStages 调用，不更新 version）
    void setInputFromConnect(const std::vector<Fragment>& fragments);

    // 清空 framebuffer
    void clear(const float* clearColor = nullptr);

    // 设置渲染状态
    void setDepthTestEnabled(bool enabled) { m_depthTestEnabled = enabled; }
    void setDepthWriteEnabled(bool enabled) { m_depthWriteEnabled = enabled; }

    // IStage 实现
    const char* getName() const override { return "Framebuffer"; }
    void execute() override;
    const PerformanceCounters& getCounters() const override { return m_counters; }
    void resetCounters() override;

    // 获取 buffer 指针
    const float* getColorBuffer() const { return m_colorBuffer.data(); }
    const float* getDepthBuffer() const { return m_depthBuffer.data(); }

private:
    // Pointer to previous stage's output (set by connectStages)
    const std::vector<Fragment>* m_inputFragmentsPtr = nullptr;
    uint32_t m_inputVersion = 0;  // 1 = connectStages, >=2 = setInput after connect
    std::vector<Fragment>  m_inputFragments;   // copy fallback
    std::vector<float>    m_colorBuffer;   // RGBA float, WIDTH*HEIGHT*4
    std::vector<float>    m_depthBuffer;   // Depth float, WIDTH*HEIGHT
    PerformanceCounters   m_counters;

    bool m_depthTestEnabled = true;
    bool m_depthWriteEnabled = true;

    // 内部：Z-test
    bool depthTest(uint32_t x, uint32_t y, float z);

    // 内部：写入 pixel
    void writePixel(uint32_t x, uint32_t y, float z, const float color[4]);
};

}  // namespace SoftGPU
