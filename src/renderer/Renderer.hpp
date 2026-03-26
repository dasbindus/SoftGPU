// ============================================================================
// SoftGPU - Renderer.hpp
// 渲染器抽象接口
// ============================================================================

#pragma once

#include <cstdint>
#include <string_view>

namespace SoftGPU {

// ============================================================================
// 渲染器类型
// ============================================================================
enum class RendererType {
    Unknown = 0,
    OpenGL,
    Vulkan,
    DirectX12,
    Metal,
    Software  // SoftGPU 软件渲染
};

// ============================================================================
// 渲染器能力
// ============================================================================
struct RendererCapabilities {
    RendererType type = RendererType::Unknown;

    // 着色器支持
    bool supportsVertexShader = false;
    bool supportsFragmentShader = false;
    bool supportsComputeShader = false;
    bool supportsGeometryShader = false;
    bool supportsTessellation = false;

    // 纹理支持
    uint32_t maxTextureSize = 0;
    uint32_t max3DTextureSize = 0;
    uint32_t maxCubeMapSize = 0;
    uint32_t maxTextureUnits = 0;

    // 渲染目标
    bool supportsRenderToTexture = true;
    bool supportsMultipleRenderTargets = true;
    uint32_t maxRenderTargets = 1;

    // 帧缓冲
    bool supportsFramebuffer = true;
    bool supportsFramebufferBlit = true;
    bool supportsFramebufferMultisample = true;
    uint32_t maxSamples = 1;

    // 几何
    uint32_t maxVertexAttributes = 0;
    uint32_t maxVertexOutputComponents = 0;
    uint32_t maxFragmentInputComponents = 0;
    uint32_t maxDrawBuffers = 1;

    // 统一缓冲区
    uint64_t maxUniformBlockSize = 0;
    uint32_t maxUniformBlocks = 0;

    // 计算
    uint32_t maxComputeWorkGroupSizeX = 0;
    uint32_t maxComputeWorkGroupSizeY = 0;
    uint32_t maxComputeWorkGroupSizeZ = 0;
    uint32_t maxComputeWorkGroupsX = 0;
    uint32_t maxComputeWorkGroupsY = 0;
    uint32_t maxComputeWorkGroupsZ = 0;
};

// ============================================================================
// 清除标记
// ============================================================================
enum class ClearMask : uint32_t {
    None       = 0,
    Color      = 1 << 0,
    Depth      = 1 << 1,
    Stencil    = 1 << 2,
    ColorDepth = Color | Depth,
    All        = Color | Depth | Stencil
};

constexpr ClearMask operator|(ClearMask a, ClearMask b) {
    return static_cast<ClearMask>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr ClearMask operator&(ClearMask a, ClearMask b) {
    return static_cast<ClearMask>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// ============================================================================
// 渲染器统计
// ============================================================================
struct RendererStats {
    uint64_t drawCalls = 0;
    uint64_t trianglesDrawn = 0;
    uint64_t verticesProcessed = 0;
    uint64_t pixelsWritten = 0;
    uint64_t framesRendered = 0;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};

// ============================================================================
// IRenderer - 渲染器抽象接口
// ============================================================================
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ========================================================================
    // 初始化和销毁
    // ========================================================================

    // 初始化渲染器
    virtual bool initialize() = 0;

    // 销毁渲染器
    virtual void shutdown() = 0;

    // 是否已初始化
    virtual bool isInitialized() const = 0;

    // ========================================================================
    // 渲染状态
    // ========================================================================

    // 开始新帧
    virtual void newFrame() = 0;

    // 结束帧
    virtual void endFrame() = 0;

    // 清除缓冲区
    virtual void clear(ClearMask mask) = 0;

    // 设置清除颜色
    virtual void setClearColor(float r, float g, float b, float a) = 0;

    // 设置视口
    virtual void setViewport(int x, int y, int width, int height) = 0;

    // ========================================================================
    // 能力查询
    // ========================================================================

    virtual RendererType getType() const = 0;
    virtual std::string_view getName() const = 0;
    virtual const RendererCapabilities& getCapabilities() const = 0;
    virtual const RendererStats& getStats() const = 0;

    // ========================================================================
    // 同步
    // ========================================================================

    // 等待渲染完成
    virtual void finish() = 0;

    // 刷新命令
    virtual void flush() = 0;
};

// ============================================================================
// 渲染器工厂（未来扩展）
// ============================================================================
class RendererFactory {
public:
    static IRenderer* create(RendererType type);
    static void destroy(IRenderer* renderer);
};

} // namespace SoftGPU
