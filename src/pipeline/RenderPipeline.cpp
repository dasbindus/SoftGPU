// ============================================================================
// SoftGPU - RenderPipeline.cpp
// TBR (Tile-Based Rendering) Pipeline Implementation
// PHASE2: TilingStage + Per-tile loop
// ============================================================================

#include "RenderPipeline.hpp"
#include "ShaderCore.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace SoftGPU {

RenderPipeline::RenderPipeline()
    : m_commandProcessor(),
      m_vertexShader(),
      m_primitiveAssembly(),
      m_tilingStage(),
      m_rasterizer(),
      m_fragmentShader(),
      m_earlyZ(std::make_unique<EarlyZ>()),
      m_framebuffer(),
      m_tileBuffer(),
      m_memory(),
      m_tileWriteBack() {
    connectStages();

    // Load default vertex shader ISA program so VS executes via ISA path, not CPP ref
    ShaderFunction defaultVS = ShaderCore::getDefaultVertexShader();
    if (!defaultVS.code.empty()) {
        m_vertexShader.SetProgram(defaultVS.code.data(), defaultVS.code.size());
    }
}

RenderPipeline::~RenderPipeline() {
}

void RenderPipeline::connectStages() {
    // PHASE1 风格连接（保持兼容）
    // Stage 3: PrimitiveAssembly -> Stage 4: Rasterizer
    m_rasterizer.setInputFromConnect(m_primitiveAssembly.getOutput());
    m_rasterizer.setViewport(FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);
    //
    // Stage 4: Rasterizer -> Stage 5: FragmentShader
    m_fragmentShader.setInputFromConnect(m_rasterizer.getOutput());
    //
    // Stage 5: FragmentShader -> Stage 6: Framebuffer
    m_framebuffer.setInputFromConnect(m_fragmentShader.getOutput());
    //
    // Stage 6: Framebuffer -> Stage 7: TileWriteBack
    m_tileWriteBack.setFramebuffer(&m_framebuffer);
    //
    // GMEM base pointer injection (needed for writeGMEM to work)
    m_memory.setGMEMBase(const_cast<float*>(m_tileWriteBack.getGMEMColor()),
                         const_cast<float*>(m_tileWriteBack.getGMEMDepth()));
}

void RenderPipeline::initGMEM(const RenderCommand& command) {
    // Initialize GMEM with clear color / depth
    // This is called once per frame to set up the "previous frame" state
    // In PHASE2 TBR, we start with cleared tiles
    (void)command;
    // GMEM is already cleared in TileWriteBack constructor
}

void RenderPipeline::render(const RenderCommand& command) {
    // PHASE5: Begin frame profiling
    if (m_profilerEnabled) {
        FrameProfiler::get().beginFrame();
    }

    // Execute common stages: CommandProcessor -> VertexShader -> PrimitiveAssembly
    executeCommonStages(command);

    if (m_tbrEnabled) {
        renderTBRMode();
    } else {
        renderPhase1Mode();
    }

    // PHASE5: End frame profiling and update bottleneck metrics
    if (m_profilerEnabled) {
        FrameProfiler::get().endFrame();
        updateBottleneckMetrics(command);
    }
}

void RenderPipeline::executeCommonStages(const RenderCommand& command) {
    // Stage 1: CommandProcessor
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::CommandProcessor);
    m_commandProcessor.setCommand(command);
    m_commandProcessor.execute();
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::CommandProcessor);

    // Clear framebuffer with clear color (PHASE1 compat)
    m_framebuffer.clear(command.clearColor.data());

    // Get command data for per-render stage setup
    const auto& vb = m_commandProcessor.getVertexBuffer();
    const auto& ib = m_commandProcessor.getIndexBuffer();
    const auto& uniforms = m_commandProcessor.getUniforms();
    const auto& drawParams = m_commandProcessor.getDrawParams();

    // Stage 2: VertexShader (input is command-dependent, set per-render)
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::VertexShader);
    m_vertexShader.setInput(vb, ib, uniforms);
    m_vertexShader.setVertexCount(drawParams.vertexCount);
    m_vertexShader.execute();
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::VertexShader);

    // Stage 3: PrimitiveAssembly (input is command-dependent, set per-render)
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::PrimitiveAssembly);
    m_primitiveAssembly.setInput(
        m_vertexShader.getOutput(),
        drawParams.indexed ? ib : std::vector<uint32_t>(),
        drawParams.indexed
    );
    m_primitiveAssembly.execute();
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::PrimitiveAssembly);
}

void RenderPipeline::renderTBRMode() {
    // ====================================================================
    // PHASE2 TBR 模式
    // ====================================================================

    // Reset memory stats for this frame
    m_memory.resetCounters();

    // Stage 8: TilingStage（一次性，对所有 triangles 做 binning）
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::Tiling);
    m_tilingStage.setInput(m_primitiveAssembly.getOutput());
    m_tilingStage.execute();
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::Tiling);

    // Initialize all tile buffers (clear to far plane / clear color)
    m_tileBuffer.initAllTiles();

    // Per-tile loop (Rasterizer + FragmentShader per tile)
    executePerTileLoop();

    // Stage 7: TileWriteBack - sync only EMPTY tiles from TileBuffer to GMEM.
    // Tiles WITH triangles were already stored per-tile in executeTile above.
    syncEmptyTilesToGMEM();

    // Also sync GMEM to Framebuffer for PHASE1 test compatibility
    syncGMEMToFramebuffer();
}

void RenderPipeline::renderPhase1Mode() {
    // ====================================================================
    // PHASE1 兼容模式（直接光栅化，无 TBR）
    // ====================================================================
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::Rasterizer);
    m_rasterizer.execute();   // Stage 4: Rasterizer
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::Rasterizer);

    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::FragmentShader);
    m_fragmentShader.execute(); // Stage 5: FragmentShader
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::FragmentShader);

    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::Framebuffer);
    m_framebuffer.execute();  // Stage 6: Framebuffer
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::Framebuffer);

    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::TileWriteBack);
    m_tileWriteBack.execute(); // Stage 7: TileWriteBack
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::TileWriteBack);
}

void RenderPipeline::executePerTileLoop() {
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::Rasterizer);
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::FragmentShader);
    m_frameTotalFragments = 0;

    for (uint32_t tileY = 0; tileY < NUM_TILES_Y; ++tileY) {
        for (uint32_t tileX = 0; tileX < NUM_TILES_X; ++tileX) {
            uint32_t tileIndex = tileY * NUM_TILES_X + tileX;

            // Check if this tile has any triangles
            const auto& bin = m_tilingStage.getTileBin(tileIndex);
            if (bin.triangleIndices.empty()) {
                // No triangles for this tile - still need to load from GMEM
                executeTile(tileIndex, tileX, tileY);
                continue;
            }

            // Execute tile (load, rasterize, shade, store)
            executeTile(tileIndex, tileX, tileY);
            m_frameTotalFragments += m_rasterizer.getCounters().extra_count1;
        }
    }
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::FragmentShader);
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::Rasterizer);
}

void RenderPipeline::syncEmptyTilesToGMEM() {
    if (m_profilerEnabled) FrameProfiler::get().beginStage(StageHandle::TileWriteBack);
    for (uint32_t tileY = 0; tileY < NUM_TILES_Y; ++tileY) {
        for (uint32_t tileX = 0; tileX < NUM_TILES_X; ++tileX) {
            uint32_t tileIndex = tileY * NUM_TILES_X + tileX;
            const auto& bin = m_tilingStage.getTileBin(tileIndex);
            if (bin.triangleIndices.empty()) {
                TileBuffer& tileMem = m_tileBuffer.getTileBuffer(tileIndex);
                m_tileWriteBack.storeTileToGMEM(tileIndex, &m_memory, tileMem);
            }
        }
    }
    if (m_profilerEnabled) FrameProfiler::get().endStage(StageHandle::TileWriteBack);
}

void RenderPipeline::updateBottleneckMetrics(const RenderCommand& command) {
    FrameProfiler& prof = FrameProfiler::get();

    // Bandwidth utilization from memory subsystem
    prof.setMemoryBandwidthUtilization(m_memory.getBandwidthUtilization());

    // Fragment shader ratio (% of frame time)
    auto fsStats = prof.getStats(StageHandle::FragmentShader);
    auto totalMs = prof.getFrameTimeMs();
    if (totalMs > 0.0) {
        prof.setFragmentShaderRatio(fsStats.ms / totalMs);
    }

    // Rasterizer efficiency: fragments_output / (viewport_pixels)
    uint64_t viewportPixels = static_cast<uint64_t>(FRAMEBUFFER_WIDTH) *
                             static_cast<uint64_t>(FRAMEBUFFER_HEIGHT);
    double rasterEff = (viewportPixels > 0) ?
        static_cast<double>(m_frameTotalFragments) / static_cast<double>(viewportPixels) : 0.0;
    rasterEff = std::min(rasterEff, 1.0);  // clamp
    prof.setRasterizerEfficiency(rasterEff);

    // Core utilization estimate: time not blocked by memory
    // Approximate as 1.0 - memory_bound_fraction
    double coreUtil = 1.0 - m_memory.getBandwidthUtilization() * 0.5;
    coreUtil = std::clamp(coreUtil, 0.0, 1.0);
    prof.setCoreUtilization(coreUtil);

    // Update bottleneck detector
    m_bottleneckDetector.setFrameProfiler(&prof);
    m_bottleneckDetector.setMetricsCollector(&m_metricsCollector);
    m_metricsCollector.updateBandwidthUtilization(m_memory.getBandwidthUtilization());
    m_metricsCollector.updateRasterizerEfficiency(rasterEff);
    m_metricsCollector.updateCoreUtilization(coreUtil);
    m_metricsCollector.updateFragmentShaderRatio(fsStats.ms / std::max(totalMs, 0.001));
    m_metricsCollector.updateTriangleCount(
        static_cast<uint64_t>(m_primitiveAssembly.getOutput().size()));
    m_metricsCollector.updateFragmentCount(m_frameTotalFragments);
    m_metricsCollector.updatePixelCount(viewportPixels);
}

void RenderPipeline::syncGMEMToFramebuffer() {
    // Sync GMEM content back to Framebuffer for PHASE1 test compatibility
    const float* gmemColor = m_tileWriteBack.getGMEMColor();
    const float* gmemDepth = m_tileWriteBack.getGMEMDepth();

    float* fbColor = const_cast<float*>(m_framebuffer.getColorBuffer());
    float* fbDepth = const_cast<float*>(m_framebuffer.getDepthBuffer());

    // Iterate through all tiles and copy GMEM to framebuffer
    for (uint32_t tileY = 0; tileY < NUM_TILES_Y; ++tileY) {
        for (uint32_t tileX = 0; tileX < NUM_TILES_X; ++tileX) {
            uint32_t tileIndex = tileY * NUM_TILES_X + tileX;

            for (uint32_t py = 0; py < TILE_HEIGHT; ++py) {
                uint32_t screenY = tileY * TILE_HEIGHT + py;
                if (screenY >= FRAMEBUFFER_HEIGHT) continue;

                for (uint32_t px = 0; px < TILE_WIDTH; ++px) {
                    uint32_t screenX = tileX * TILE_WIDTH + px;
                    if (screenX >= FRAMEBUFFER_WIDTH) continue;

                    // GMEM offset for this pixel within the tile (in pixels)
                    size_t gmemPixelOffset = tileIndex * TILE_SIZE + py * TILE_WIDTH + px;
                    size_t screenIdx = screenY * FRAMEBUFFER_WIDTH + screenX;

                    // Copy color (RGBA) - GMEM stores as [pixelIdx * 4 + channel]
                    size_t gmemColorIdx = gmemPixelOffset * 4;
                    fbColor[screenIdx * 4 + 0] = gmemColor[gmemColorIdx + 0];
                    fbColor[screenIdx * 4 + 1] = gmemColor[gmemColorIdx + 1];
                    fbColor[screenIdx * 4 + 2] = gmemColor[gmemColorIdx + 2];
                    fbColor[screenIdx * 4 + 3] = gmemColor[gmemColorIdx + 3];

                    // Copy depth
                    fbDepth[screenIdx] = gmemDepth[gmemPixelOffset];
                }
            }
        }
    }
}

void RenderPipeline::executeTile(uint32_t tileIndex, uint32_t tileX, uint32_t tileY) {
    // Get the tile's triangle list from TilingStage
    const auto& bin = m_tilingStage.getTileBin(tileIndex);
    const std::vector<Triangle>& allTriangles = m_tilingStage.getInputTriangles();

    // Build per-tile triangle list (indices → actual triangles)
    std::vector<Triangle> tileTriangles;
    tileTriangles.reserve(bin.triangleIndices.size());
    for (uint32_t idx : bin.triangleIndices) {
        tileTriangles.push_back(allTriangles[idx]);
    }

    // Get reference to this tile's LMEM
    TileBuffer& tileMem = m_tileBuffer.getTileBuffer(tileIndex);

    // Step 1: Load tile from GMEM (initialize LMEM with GMEM state)
    // Only load previous frame data for tiles WITHOUT current triangles.
    // For tiles WITH current triangles, we want FRESH rendering (not mixed with previous frame).
    if (bin.triangleIndices.empty()) {
        // No current triangles - preserve previous frame's data
        m_tileWriteBack.loadTileFromGMEM(tileIndex, &m_memory, tileMem);
    }
    // For tiles with triangles, skip load - we want fresh rendering

    // Step 2: Rasterizer (per-tile) - outputs fragments with screen coords
    m_rasterizer.setTrianglesForTile(tileTriangles, tileX, tileY);
    m_rasterizer.executePerTile();
    const auto& fragments = m_rasterizer.getOutput();

    // Step 2.5: EarlyZ - filter occluded fragments before FragmentShader
    const float* tileDepth = tileMem.depth.data();
    auto filteredFragments = m_earlyZ->filterOccluded(fragments, tileDepth, TILE_WIDTH, tileX, tileY);

    // Step 3: FragmentShader - shade and write to TileBuffer (per-tile)
    m_fragmentShader.setTileBufferManager(&m_tileBuffer);
    m_fragmentShader.setTileIndex(tileIndex);
    m_fragmentShader.setInputAndExecuteTile(filteredFragments, tileX, tileY);

    // Step 4: Store tile to GMEM (per-tile write-back for tiles with geometry)
    // Tiles without triangles skip store; they'll be handled by storeAllTilesFromBuffer
    // with cleared data (from initAllTiles). This avoids redundant stores.
    if (!bin.triangleIndices.empty()) {
        m_tileWriteBack.storeTileToGMEM(tileIndex, &m_memory, tileMem);
    }
}

void RenderPipeline::dump(const std::string& filename) const {
    // Clear output path to ensure clean filename when m_output_dir is empty
    // This prevents stale m_outputPath from previous tests affecting current dump
    const_cast<RenderPipeline*>(this)->m_dumper.setOutputPath("");

    // Sync GMEM to Framebuffer first for consistent output
    if (m_tbrEnabled) {
        const_cast<RenderPipeline*>(this)->syncGMEMToFramebuffer();
        const float* colorData = m_framebuffer.getColorBuffer();
        m_dumper.dumpPPM(colorData, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, filename);
    } else {
        const float* colorData = m_framebuffer.getColorBuffer();
        m_dumper.dumpPPM(colorData, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, filename);
    }
}

void RenderPipeline::setDumpOutputPath(const std::string& path) {
    m_dumper.setOutputPath(path);
}

void RenderPipeline::dumpFrame(uint32_t frameIndex) {
    // Sync GMEM to Framebuffer first for consistent output in TBR mode
    if (m_tbrEnabled) {
        const_cast<RenderPipeline*>(this)->syncGMEMToFramebuffer();
        const float* colorData = m_framebuffer.getColorBuffer();
        m_dumper.dumpFrame(colorData, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, frameIndex);
    } else {
        const float* colorData = m_framebuffer.getColorBuffer();
        m_dumper.dumpFrame(colorData, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, frameIndex);
    }
    m_frameIndex = frameIndex + 1;
}

void RenderPipeline::printPerformanceReport() const {
    std::cout << "\n========== PHASE2 TBR Performance Report ==========\n";

    std::cout << "CommandProcessor:  inv=" << m_commandProcessor.getCounters().invocation_count
              << "  elapsed=" << std::fixed << std::setprecision(3)
              << m_commandProcessor.getCounters().elapsed_ms << " ms\n";

    std::cout << "VertexShader:     inv=" << m_vertexShader.getCounters().invocation_count
              << "  cycles=" << m_vertexShader.getCounters().cycle_count
              << "  elapsed=" << m_vertexShader.getCounters().elapsed_ms << " ms\n";

    std::cout << "PrimitiveAssembly: inv=" << m_primitiveAssembly.getCounters().invocation_count
              << "  culled=" << m_primitiveAssembly.getCounters().extra_count0
              << "  elapsed=" << m_primitiveAssembly.getCounters().elapsed_ms << " ms\n";

    std::cout << "TilingStage:      inv=" << m_tilingStage.getCounters().invocation_count
              << "  triangles_binned=" << m_tilingStage.getCounters().extra_count0
              << "  tiles_affected=" << m_tilingStage.getCounters().extra_count1
              << "  elapsed=" << m_tilingStage.getCounters().elapsed_ms << " ms\n";

    std::cout << "Rasterizer:       inv=" << m_rasterizer.getCounters().invocation_count
              << "  tile_fragments=" << m_rasterizer.getCounters().extra_count1
              << "  elapsed=" << m_rasterizer.getCounters().elapsed_ms << " ms\n";

    std::cout << "FragmentShader:   inv=" << m_fragmentShader.getCounters().invocation_count
              << "  elapsed=" << m_fragmentShader.getCounters().elapsed_ms << " ms\n";

    std::cout << "Framebuffer:      inv=" << m_framebuffer.getCounters().invocation_count
              << "  writes=" << m_framebuffer.getCounters().extra_count0
              << "  depth_tests=" << m_framebuffer.getCounters().extra_count1
              << "  elapsed=" << m_framebuffer.getCounters().elapsed_ms << " ms\n";

    std::cout << "TileWriteBack:    inv=" << m_tileWriteBack.getCounters().invocation_count
              << "  elapsed=" << m_tileWriteBack.getCounters().elapsed_ms << " ms\n";

    std::cout << "MemorySubsystem:\n";
    std::cout << "  read_bytes=" << m_memory.getReadBytes()
              << "  write_bytes=" << m_memory.getWriteBytes()
              << "  total_accesses=" << m_memory.getAccessCount() << "\n";
    std::cout << "  bandwidth_util=" << std::setprecision(4)
              << (m_memory.getBandwidthUtilization() * 100.0) << "%\n";
    std::cout << "  l2_hit_rate=" << std::setprecision(4)
              << (m_memory.getL2Cache().getHitRate() * 100.0) << "%\n";

    std::cout << "==================================================\n";
}

}  // namespace SoftGPU
