// ============================================================================
// SoftGPU - MemorySubsystem.cpp
// GMEM Bandwidth Model + L2 Cache Simulation
// PHASE2 NEW
// ============================================================================

#include "MemorySubsystem.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

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
    using namespace std::chrono;
    // Get current wall-clock time in milliseconds
    double nowMs = static_cast<double>(
        duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count()
    ) / 1e6;

    // Calculate elapsed time since last refill
    double elapsedSeconds = (nowMs - lastRefillTime) / 1000.0;
    if (elapsedSeconds < 0) elapsedSeconds = 0;  // Guard against clock skew

    // Add tokens based on elapsed time
    double tokensToAdd = refillRate * elapsedSeconds;
    tokens = std::min(maxTokens, tokens + tokensToAdd);

    // Update last refill time
    lastRefillTime = nowMs;
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

    // 在 set 内查找匹配的 line
    for (uint32_t way = 0; way < L2_CACHE_WAYS; ++way) {
        uint32_t lineIdx = setIdx * L2_CACHE_WAYS + way;
        CacheLine& line = m_lines[lineIdx];

        if (line.valid && line.tag == tag) {
            // HIT
            m_hits++;
            line.lastUsed = m_currentTick++;
            if (isWrite) {
                line.dirty = true;
            }
            return true;
        }
    }

    // ================================================================
    // PHASE3: Non-write-allocate policy
    // Write miss: do NOT allocate cache line, just write to memory
    // ================================================================
    if (isWrite) {
        m_misses++;
        m_writeMissNoAlloc++;
        return false;
    }

    // ================================================================
    // PHASE3: Tile-aware replacement strategy
    // Prioritize evicting lines from different tiles first
    // ================================================================
    uint32_t currentTile = (address / (TILE_WIDTH * TILE_HEIGHT * 4)) & 0xFFFF;
    uint32_t lruWay = 0;
    uint32_t lruAge = 0;
    bool foundCrossTile = false;

    // First pass: look for lines from other tiles (lower priority)
    for (uint32_t way = 0; way < L2_CACHE_WAYS; ++way) {
        uint32_t lineIdx = setIdx * L2_CACHE_WAYS + way;
        CacheLine& line = m_lines[lineIdx];

        // Tile-aware: prefer evicting lines from different tiles
        if (line.tile_id != currentTile && line.valid) {
            uint32_t age = m_currentTick - line.lastUsed;
            if (!foundCrossTile || age > lruAge) {
                lruAge = age;
                lruWay = way;
                foundCrossTile = true;
            }
        }
    }

    // Second pass: if all lines are from same tile, use regular LRU
    if (!foundCrossTile) {
        lruAge = 0;
        for (uint32_t way = 0; way < L2_CACHE_WAYS; ++way) {
            uint32_t lineIdx = setIdx * L2_CACHE_WAYS + way;
            uint32_t age = m_currentTick - m_lines[lineIdx].lastUsed;
            if (age > lruAge) {
                lruAge = age;
                lruWay = way;
            }
        }
    }

    uint32_t victimIdx = setIdx * L2_CACHE_WAYS + lruWay;
    CacheLine& victim = m_lines[victimIdx];

    // Write-back 简化：如果 victim dirty，不模拟写回带宽
    victim.tag = tag;
    victim.valid = true;
    victim.dirty = isWrite;
    victim.lastUsed = m_currentTick++;
    victim.tile_id = currentTile;
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
    m_writeMissNoAlloc = 0;
    m_currentTick = 0;
    for (auto& line : m_lines) {
        line.valid = false;
        line.dirty = false;
        line.tag = 0;
        line.lastUsed = 0;
        line.tile_id = 0;
    }
}

uint32_t L2CacheSim::getSetIndex(uint64_t address) const {
    return static_cast<uint32_t>((address / CACHE_LINE_SIZE) % L2_CACHE_SETS);
}

uint64_t L2CacheSim::getTag(uint64_t address) const {
    return address / (CACHE_LINE_SIZE * L2_CACHE_SETS);
}

// ============================================================================
// MemorySubsystem Implementation
// ============================================================================
MemorySubsystem::MemorySubsystem(double bandwidthGBps) {
    m_bandwidthGBps = bandwidthGBps;
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
    m_bucket.init(m_bandwidthGBps);
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
