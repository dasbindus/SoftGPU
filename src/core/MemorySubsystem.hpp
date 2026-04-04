// ============================================================================
// SoftGPU - MemorySubsystem.hpp
// GMEM Bandwidth Model + L2 Cache Simulation
// PHASE2 NEW
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <chrono>

namespace SoftGPU {

// ============================================================================
// 带宽配置（可调）
// ============================================================================
constexpr double DEFAULT_BANDWIDTH_GBPS = 100.0;  // 100 GB/s
constexpr double NS_PER_SEC = 1e9;

// Cache 配置
constexpr uint32_t CACHE_LINE_SIZE = 64;   // bytes（模拟 L2 cache line）
constexpr uint32_t L2_CACHE_SETS   = 512;  // P0-4: 256→512, 128KB→256KB
constexpr uint32_t L2_CACHE_WAYS   = 8;    // 8-way set associative
constexpr size_t   L2_CACHE_SIZE   = CACHE_LINE_SIZE * L2_CACHE_SETS * L2_CACHE_WAYS; // 256 KB
constexpr uint32_t CACHE_TILE_SIZE_BYTES = 65536; // P0-4: tile 大小 64KB，用于 tile-aware 替换

// ============================================================================
// MemoryAccessType - 访问类型
// ============================================================================
enum class MemoryAccessType {
    LoadTile,     // GMEM → LMEM（load tile 到 TileBuffer）
    StoreTile,    // LMEM → GMEM（tile 完成，写回）
    ReadVertex,   // VB/IB 读取（Phase2 不重点建模，简化为计数）
};

// ============================================================================
// TokenBucket - 令牌桶带宽模型
// ============================================================================
struct TokenBucket {
    double tokens = 0.0;           // 当前令牌数
    double maxTokens = 0.0;       // 桶容量（bytes）
    double refillRate = 0.0;       // 每秒补充令牌数（bytes/s）
    double lastRefillTime = 0.0;   // 上次补充时间（墙上时间，ms）
    double lastRefillTokens = 0.0; // 上次 refill 时的 tokens 基准值

    void init(double bandwidthGBps, double capacityMultiplier = 1.0);
    bool tryConsume(size_t bytes);
    void refill();
    
private:
    std::chrono::steady_clock::time_point start_time;  // 首个 refill 时刻
};

// ============================================================================
// CacheLine - L2 Cache 行状态
// ============================================================================
struct CacheLine {
    uint64_t tag = 0;
    bool valid = false;
    bool dirty = false;
    uint32_t lastUsed = 0;   // 简化 LRU tick
    uint32_t lastTile = 0;   // P0-4: 最近访问该 line 的 tile index（用于 tile-aware 替换）
};

// ============================================================================
// L2CacheSim - L2 Cache 模拟器
// ============================================================================
class L2CacheSim {
public:
    L2CacheSim();

    // 访问地址，返回是否命中（hit = true）
    bool access(uint64_t address, bool isWrite);

    // 统计
    double getHitRate() const;
    uint64_t getHits() const { return m_hits; }
    uint64_t getMisses() const { return m_misses; }
    void resetStats();

private:
    uint64_t m_hits = 0;
    uint64_t m_misses = 0;
    uint32_t m_currentTick = 0;
    std::array<CacheLine, L2_CACHE_SETS * L2_CACHE_WAYS> m_lines;

    // P0-4: tile-aware 替换策略
    uint32_t m_tileAwareWindow = 32;  // 最近的 32 个 tile 命中最优先
    uint32_t m_currentTileIdx = 0;    // 当前正在服务的 tile index（由 access() 调用方维护）

    uint32_t getSetIndex(uint64_t address) const;
    uint64_t getTag(uint64_t address) const;
    uint32_t getTileIndex(uint64_t address) const;  // P0-4: 从 address 推导 tile index
};

// ============================================================================
// MemorySubsystem - GMEM 模拟 + 带宽模型
// ============================================================================
class MemorySubsystem {
public:
    MemorySubsystem(double bandwidthGBps = DEFAULT_BANDWIDTH_GBPS);

    // 带宽记录接口
    // 每次 GMEM 访问调用 addAccess，消耗令牌
    // 返回值：true = 允许访问，false = 带宽耗尽（阻塞模拟）
    bool addAccess(size_t bytes, MemoryAccessType type);

    // 统计读取 / 写入字节数
    void recordRead(size_t bytes)  { m_totalReadBytes += bytes; }
    void recordWrite(size_t bytes) { m_totalWriteBytes += bytes; }

    // GMEM 模拟读写（带 L2 cache 模拟）
    // gmemColor/gmemDepth 指向 TileWriteBack 的 GMEM 存储（由 RenderPipeline 注入）
    // 返回值：true = 成功执行 memcpy，false = 带宽超限，跳过 memcpy
    void setGMEMBase(float* gmemColor, float* gmemDepth);
    bool readGMEM(void* dst, uint64_t offset, size_t bytes);
    bool writeGMEM(uint64_t offset, const void* src, size_t bytes);

    // L2 Cache 查询
    L2CacheSim& getL2Cache() { return m_l2Cache; }
    const L2CacheSim& getL2Cache() const { return m_l2Cache; }

    // P0-3: 暴露 TokenBucket 供外部检查带宽
    TokenBucket& getBucket() { return m_bucket; }
    const TokenBucket& getBucket() const { return m_bucket; }

    // 带宽利用率（0.0 ~ 1.0）
    double getBandwidthUtilization() const;
    double getConsumedBandwidthGBps() const;

    // 重置统计
    void resetCounters();

    // 性能计数器
    uint64_t getReadBytes()   const { return m_totalReadBytes; }
    uint64_t getWriteBytes()  const { return m_totalWriteBytes; }
    uint64_t getAccessCount() const { return m_accessCount; }
    uint64_t getBandwidthOverLimitCount() const { return m_bandwidthOverLimitCount; }
    double   getElapsedMs()  const { return m_elapsedMs; }

private:
    TokenBucket m_bucket;
    L2CacheSim  m_l2Cache;
    uint64_t    m_totalReadBytes = 0;
    uint64_t    m_totalWriteBytes = 0;
    uint64_t    m_accessCount = 0;
    double      m_elapsedMs = 0.0;
    double      m_startTimeMs = 0.0;
    uint64_t    m_bandwidthOverLimitCount = 0;  // 带宽超限次数

    // GMEM 基址（指向 TileWriteBack 的 m_gmemColor/m_gmemDepth，由 RenderPipeline 注入）
    float*      m_gmemColorBase = nullptr;
    float*      m_gmemDepthBase = nullptr;

    // 内部：获取墙上时间（ms）
    double getCurrentTimeMs() const;
};

}  // namespace SoftGPU
