// ============================================================================
// SoftGPU - RenderPipeline.hpp
// 渲染管线
// PHASE2: TBR (Tile-Based Rendering) 主循环
// ============================================================================

#pragma once

#include "stages/CommandProcessor.hpp"
#include "stages/VertexShader.hpp"
#include "stages/PrimitiveAssembly.hpp"
#include "stages/TilingStage.hpp"
#include "stages/Rasterizer.hpp"
#include "stages/FragmentShader.hpp"
#include "stages/Framebuffer.hpp"
#include "stages/TileBuffer.hpp"
#include "stages/TileWriteBack.hpp"
#include "core/MemorySubsystem.hpp"
#include "core/RenderCommand.hpp"
#include "utils/FrameDumper.hpp"
#include "profiler/FrameProfiler.hpp"
#include "profiler/BottleneckDetector.hpp"

namespace SoftGPU {

// ============================================================================
// RenderPipeline - TBR 渲染管线
// PHASE2: 
//   - TilingStage bins triangles to per-tile lists
//   - Per-tile loop: load → rasterize → fragment shader → store
//   - GMEM ↔ LMEM sync via MemorySubsystem
// ============================================================================
class RenderPipeline {
public:
    RenderPipeline();
    ~RenderPipeline();

    // 执行一次完整的 TBR 渲染
    void render(const RenderCommand& command);

    // 获取当前 framebuffer（PHASE1 兼容，仍可用）
    const Framebuffer* getFramebuffer() const { return &m_framebuffer; }

    // ========================================================================
    // GMEM 数据访问（用于 Present / 导出）
    // ========================================================================
    const float* getGMEMColor() const { return m_tileWriteBack.getGMEMColor(); }
    const float* getGMEMDepth() const { return m_tileWriteBack.getGMEMDepth(); }

    // ========================================================================
    // dump 方法
    // ========================================================================
    void dump(const std::string& filename) const;
    void setDumpOutputPath(const std::string& path);
    void dumpFrame(uint32_t frameIndex);
    const std::string& getDumpOutputPath() const { return m_dumper.getOutputPath(); }

    // ========================================================================
    // 性能报告
    // ========================================================================
    void printPerformanceReport() const;

    // 获取各阶段引用（用于调试）
    const CommandProcessor&  getCommandProcessor()  const { return m_commandProcessor; }
    const VertexShader&      getVertexShader()      const { return m_vertexShader; }
    const PrimitiveAssembly& getPrimitiveAssembly() const { return m_primitiveAssembly; }
    const TilingStage&       getTilingStage()      const { return m_tilingStage; }
    const Rasterizer&        getRasterizer()        const { return m_rasterizer; }
    const FragmentShader&    getFragmentShader()    const { return m_fragmentShader; }
    const TileWriteBack&     getTileWriteBack()     const { return m_tileWriteBack; }
    const MemorySubsystem&   getMemorySubsystem()   const { return m_memory; }
    MemorySubsystem&         getMemorySubsystem()         { return m_memory; }

    // 带宽 / Cache 统计
    double getBandwidthUtilization() const { return m_memory.getBandwidthUtilization(); }
    double getL2HitRate() const { return m_memory.getL2Cache().getHitRate(); }

    // 开启/关闭 TBR 模式（用于 PHASE1 兼容测试）
    void setTBREnabled(bool enabled) { m_tbrEnabled = enabled; }
    bool isTBREnabled() const { return m_tbrEnabled; }

    // ========================================================================
    // PHASE5: 性能分析器访问
    // ========================================================================
    FrameProfiler&      getProfiler()       { return FrameProfiler::get(); }
    const FrameProfiler& getProfiler()      const { return FrameProfiler::get(); }
    BottleneckDetector&  getBottleneckDetector() { return m_bottleneckDetector; }

    // 开启/关闭性能分析（默认开启）
    void setProfilerEnabled(bool enabled) { m_profilerEnabled = enabled; }
    bool isProfilerEnabled() const { return m_profilerEnabled; }

private:
    CommandProcessor   m_commandProcessor;
    VertexShader       m_vertexShader;
    PrimitiveAssembly  m_primitiveAssembly;
    TilingStage        m_tilingStage;
    Rasterizer         m_rasterizer;
    FragmentShader     m_fragmentShader;
    Framebuffer        m_framebuffer;      // PHASE1 兼容，仍用于直接输出
    TileBufferManager  m_tileBuffer;       // PHASE2 LMEM
    MemorySubsystem    m_memory;           // PHASE2 GMEM 带宽模型
    TileWriteBack      m_tileWriteBack;    // PHASE2 GMEM ↔ LMEM 同步

    FrameDumper        m_dumper;
    uint32_t           m_frameIndex = 0;
    bool               m_tbrEnabled = true;  // PHASE2 默认开启 TBR
    bool               m_profilerEnabled = true;  // PHASE5 默认开启性能分析
    uint64_t           m_frameTotalFragments = 0; // PHASE5: 总 fragment 数（跨所有 tile）

    // PHASE5: 瓶颈检测
    MetricsCollector   m_metricsCollector;
    BottleneckDetector m_bottleneckDetector{};

    // 内部：更新瓶颈指标
    void updateBottleneckMetrics(const RenderCommand& command);

    // 内部：执行单个 tile
    void executeTile(uint32_t tileIndex, uint32_t tileX, uint32_t tileY);

    // 内部：同步 GMEM 到 Framebuffer（PHASE1 测试兼容）
    void syncGMEMToFramebuffer();

    // 内部：连接各阶段的数据流（PHASE1 风格连接）
    void connectStages();

    // 内部：初始化 GMEM
    void initGMEM(const RenderCommand& command);

    // R-P0-3: render() 拆解 - 单一职责原则
    // 执行公共阶段：CommandProcessor -> VertexShader -> PrimitiveAssembly
    void executeCommonStages(const RenderCommand& command);
    // TBR 渲染模式
    void renderTBRMode();
    // PHASE1 兼容模式
    void renderPhase1Mode();
    // Per-tile 循环
    void executePerTileLoop();
    // 同步空 tiles 到 GMEM
    void syncEmptyTilesToGMEM();
};

}  // namespace SoftGPU
