// ============================================================================
// SoftGPU - MemorySubsystem.cpp
// GMEM Bandwidth Model + L2 Cache Simulation
// PHASE2 NEW
// ============================================================================

#include "MemorySubsystem.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>

namespace SoftGPU {

// ============================================================================
// TokenBucket Implementation
// ============================================================================
void TokenBucket::init(double bandwidthGBps, double capacityMultiplier) {
    // 桶容量 = 1 秒的带宽（capacityMultiplier 可以放大）
    maxTokens = bandwidthGBps * 1e9 * capacityMultiplier;  // bytes
    tokens = maxTokens;
    refillRate = bandwidthGBps * 1e9;  // bytes/s
    lastRefillTime = 0.0;
}

bool TokenBucket::tryConsume(size_t bytes) {
    refill();

    if (tokens >= static_cast<double>(bytes)) {
        tokens -= static_cast<double>(bytes);
        return true;
    } else {
        // 带宽不足，返回 false（不执行 memcpy，由调用者决定如何处理）
        return false;
    }
}

void TokenBucket::refill() {
    // P0-3: 使用墙上时间线性补充令牌（累积式）
    using namespace std::chrono;
    
    auto now = steady_clock::now();
    
    // 首次 refill，初始化基准时间
    if (start_time.time_since_epoch().count() == 0) {
        start_time = now;
        lastRefillTime = 0.0;
        lastRefillTokens = tokens;  // 记录初始 tokens 基准
        return;
    }
    
    double elapsed_s = duration<double>(now - start_time).count();
    double elapsed_tokens = elapsed_s * refillRate;
    
    // 线性补充：tokens = min(maxTokens, lastRefillTokens + elapsed_tokens)
    // lastRefillTokens 是上次 refill 时的 tokens 值（扣除已消耗后的基准）
    tokens = std::min(maxTokens, lastRefillTokens + elapsed_tokens);
    lastRefillTime = elapsed_s * 1000.0;
    lastRefillTokens = tokens;
}

// ============================================================================
// L2CacheSim Implementation
// ============================================================================
L2CacheSim::L2CacheSim() {
    resetStats();
}

bool L2CacheSim::access(uint64_t address, bool isWrite) {
    uint32_t setIdx = getSetIndex(address);
    uint64_t tag = getTag(address);
    uint32_t currentTile = getTileIndex(address);

    // 在 set 内查找匹配的 line
    for (uint32_t way = 0; way < L2_CACHE_WAYS; ++way) {
        uint32_t lineIdx = setIdx * L2_CACHE_WAYS + way;
        CacheLine& line = m_lines[lineIdx];

        if (line.valid && line.tag == tag) {
            // HIT
            m_hits++;
            line.lastUsed = m_currentTick++;
            line.lastTile = currentTile;  // P0-4: 记录命中的 tile
            if (isWrite) {
                line.dirty = true;
            }
            return true;
        }
    }

    // MISS：选择 victim 行替换（tile-aware LRU）
    // 策略：
    //   1. 优先选择 invalid line（无 eviction 开销）
    //   2. 否则优先淘汰 lastTile != currentTile 的 line（保留当前 tile 的局部性）
    //   3. 同 tile 内按 LRU 年龄最老淘汰

    uint32_t victimWay = 0;
    bool foundInvalid = false;
    int32_t bestScore = -1;  // 越大越好：score = (lastTile != currentTile ? 1 : 0) * INF + age
    uint32_t bestAge = 0;

    for (uint32_t way = 0; way < L2_CACHE_WAYS; ++way) {
        uint32_t lineIdx = setIdx * L2_CACHE_WAYS + way;
        CacheLine& line = m_lines[lineIdx];

        if (!line.valid) {
            // 找到 invalid line，立即选中
            victimWay = way;
            foundInvalid = true;
            break;
        }

        uint32_t age = m_currentTick - line.lastUsed;
        bool differentTile = (line.lastTile != currentTile);

        // Score: 不同 tile 权重最高，其次是 LRU 年龄
        int32_t score = differentTile ? (100000 + age) : age;

        if (score > bestScore) {
            bestScore = score;
            bestAge = age;
            victimWay = way;
        }
    }

    uint32_t victimIdx = setIdx * L2_CACHE_WAYS + victimWay;
    CacheLine& victim = m_lines[victimIdx];

    // Write-back 简化：如果 victim dirty，不模拟写回带宽
    victim.tag = tag;
    victim.valid = true;
    victim.dirty = isWrite;
    victim.lastUsed = m_currentTick++;
    victim.lastTile = currentTile;  // P0-4: 记录新 line 的 tile
    m_misses++;

    return false;
}

double L2CacheSim::getHitRate() const {
    uint64_t total = m_hits + m_misses;
    if (total == 0) return 0.0;
    return static_cast<double>(m_hits) / static_cast<double>(total);
}

void L2CacheSim::resetStats() {
    m_hits = 0;
    m_misses = 0;
    m_currentTick = 0;
    for (auto& line : m_lines) {
        line.valid = false;
        line.dirty = false;
        line.tag = 0;
        line.lastUsed = 0;
        line.lastTile = 0;  // P0-4
    }
}

uint32_t L2CacheSim::getSetIndex(uint64_t address) const {
    return static_cast<uint32_t>((address / CACHE_LINE_SIZE) % L2_CACHE_SETS);
}

uint64_t L2CacheSim::getTag(uint64_t address) const {
    return address / (CACHE_LINE_SIZE * L2_CACHE_SETS);
}

uint32_t L2CacheSim::getTileIndex(uint64_t address) const {
    return static_cast<uint32_t>(address / CACHE_TILE_SIZE_BYTES);
}

// ============================================================================
// MemorySubsystem Implementation
// ============================================================================
MemorySubsystem::MemorySubsystem(double bandwidthGBps) {
    m_bucket.init(bandwidthGBps);
    resetCounters();
    m_startTimeMs = getCurrentTimeMs();
}

bool MemorySubsystem::addAccess(size_t bytes, MemoryAccessType type) {
    // 消耗令牌
    bool allowed = m_bucket.tryConsume(bytes);
    m_accessCount++;

    if (allowed) {
        switch (type) {
            case MemoryAccessType::LoadTile:
                m_totalReadBytes += bytes;
                break;
            case MemoryAccessType::StoreTile:
                m_totalWriteBytes += bytes;
                break;
            case MemoryAccessType::ReadVertex:
                m_totalReadBytes += bytes;
                break;
        }
    } else {
        // 带宽超限，记录事件
        m_bandwidthOverLimitCount++;
    }

    return allowed;
}

void MemorySubsystem::setGMEMBase(float* gmemColor, float* gmemDepth) {
    m_gmemColorBase = gmemColor;
    m_gmemDepthBase = gmemDepth;
}

bool MemorySubsystem::readGMEM(void* dst, uint64_t offset, size_t bytes) {
    // 检查带宽是否允许
    if (!m_bucket.tryConsume(bytes)) {
        // 带宽超限，记录事件
        m_bandwidthOverLimitCount++;
        return false;
    }
    m_accessCount++;
    m_totalReadBytes += bytes;

    // 模拟 GMEM 读取
    uint64_t addr = offset;

    // 对每个 cache line 进行 L2 cache 模拟访问
    size_t lineAlign = addr & ~(CACHE_LINE_SIZE - 1);
    size_t endAddr = addr + bytes;

    while (lineAlign < endAddr) {
        m_l2Cache.access(lineAlign, false);
        lineAlign += CACHE_LINE_SIZE;
    }

    // 执行真实 memcpy（从 GMEM → dst）
    if (m_gmemColorBase != nullptr) {
        std::memcpy(dst, reinterpret_cast<const char*>(m_gmemColorBase) + offset, bytes);
    }

    recordRead(bytes);
    return true;
}

bool MemorySubsystem::writeGMEM(uint64_t offset, const void* src, size_t bytes) {
    // 检查带宽是否允许
    if (!m_bucket.tryConsume(bytes)) {
        // 带宽超限，记录事件
        m_bandwidthOverLimitCount++;
        return false;
    }
    m_accessCount++;
    m_totalWriteBytes += bytes;

    // 模拟 GMEM 写入
    uint64_t addr = offset;

    // 对每个 cache line 进行 L2 cache 模拟访问
    size_t lineAlign = addr & ~(CACHE_LINE_SIZE - 1);
    size_t endAddr = addr + bytes;

    while (lineAlign < endAddr) {
        m_l2Cache.access(lineAlign, true);
        lineAlign += CACHE_LINE_SIZE;
    }

    // 执行真实 memcpy（从 src → GMEM）
    if (m_gmemColorBase != nullptr) {
        std::memcpy(reinterpret_cast<char*>(m_gmemColorBase) + offset, src, bytes);
    }

    recordWrite(bytes);
    return true;
}

double MemorySubsystem::getBandwidthUtilization() const {
    // 基于时间的带宽利用率
    double elapsed = getCurrentTimeMs() - m_startTimeMs;
    if (elapsed <= 0.0) return 0.0;

    double totalBytes = static_cast<double>(m_totalReadBytes + m_totalWriteBytes);
    double consumedBandwidth = totalBytes / (elapsed * 1e-3);  // bytes/s
    double peakBandwidth = DEFAULT_BANDWIDTH_GBPS * 1e9;  // bytes/s

    return std::min(1.0, consumedBandwidth / peakBandwidth);
}

double MemorySubsystem::getConsumedBandwidthGBps() const {
    double elapsed = getCurrentTimeMs() - m_startTimeMs;
    if (elapsed <= 0.0) return 0.0;

    double totalBytes = static_cast<double>(m_totalReadBytes + m_totalWriteBytes);
    return (totalBytes / (elapsed * 1e-3)) / 1e9;  // GB/s
}

void MemorySubsystem::resetCounters() {
    m_totalReadBytes = 0;
    m_totalWriteBytes = 0;
    m_accessCount = 0;
    m_bandwidthOverLimitCount = 0;
    m_l2Cache.resetStats();
    m_bucket.init(DEFAULT_BANDWIDTH_GBPS);
    m_startTimeMs = getCurrentTimeMs();
    m_elapsedMs = 0.0;
}

double MemorySubsystem::getCurrentTimeMs() const {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count()
    ) / 1e6;
}

}  // namespace SoftGPU
