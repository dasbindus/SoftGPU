// ============================================================================
// SoftGPU - WarpScheduler.hpp
// Warp Scheduler 实现
// Phase 2: Warp=8 调度器，支持多线程并行执行
// ============================================================================

#pragma once

#include "ShaderCore.hpp"
#include "core/PipelineTypes.hpp"
#include <memory>
#include <vector>
#include <array>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace SoftGPU {

// ============================================================================
// Warp - 8-thread SIMD 执行单元
// ============================================================================
class Warp {
public:
    static constexpr uint32_t WARP_SIZE = 8;
    static constexpr uint32_t INVALID_ID = 0xFFFFFFFF;
    
    // Warp 状态
    enum class State {
        IDLE,       // 空闲，未分配
        RUNNING,    // 正在执行
        WAITING,    // 等待资源（如 texture fetch）
        DONE,       // 执行完成
        KILLED      // 被 kill
    };
    
    Warp();
    ~Warp();
    
    // Warp ID
    uint32_t getId() const { return m_warp_id; }
    void setId(uint32_t id) { m_warp_id = id; }
    
    // 状态管理
    State getState() const { return m_state; }
    void setState(State s) { m_state = s; }
    
    bool isIdle() const { return m_state == State::IDLE; }
    bool isRunning() const { return m_state == State::RUNNING; }
    bool isDone() const { return m_state == State::DONE || m_state == State::KILLED; }
    
    // Fragment 上下文访问
    FragmentContext& getFragment(uint32_t lane) { return m_fragments[lane]; }
    const FragmentContext& getFragment(uint32_t lane) const { return m_fragments[lane]; }
    
    // 批量设置 fragment
    void setFragments(const std::vector<FragmentContext>& fragments);
    
    // Fragment 数量
    uint32_t getFragmentCount() const { return m_fragment_count; }
    void setFragmentCount(uint32_t count) { m_fragment_count = count; }
    
    // PC 和指令
    uint32_t getPC() const { return m_pc; }
    void setPC(uint32_t pc) { m_pc = pc; }
    void advancePC(uint32_t delta = 4) { m_pc += delta; }
    
    // Shader 关联
    void setShader(const ShaderFunction* shader) { m_shader = shader; }
    const ShaderFunction* getShader() const { return m_shader; }
    
    // 统计
    struct WarpStats {
        uint64_t cycles_active = 0;
        uint64_t cycles_stalled = 0;
        uint64_t instructions_executed = 0;
        uint64_t branches_taken = 0;
    };
    
    const WarpStats& getStats() const { return m_stats; }
    WarpStats& getStats() { return m_stats; }
    void resetStats() { m_stats = WarpStats(); }
    
    // 重置 Warp
    void reset();
    
    // 完成标记
    void finish() { m_state = State::DONE; }
    void kill() { m_state = State::KILLED; }
    
private:
    uint32_t m_warp_id = INVALID_ID;
    State m_state = State::IDLE;
    
    // 8 个 lane，每个 lane 一个 fragment
    std::array<FragmentContext, WARP_SIZE> m_fragments;
    uint32_t m_fragment_count = 0;
    
    // 当前执行的 shader
    const ShaderFunction* m_shader = nullptr;
    
    // Warp 内的 PC（所有 lane 同步执行）
    uint32_t m_pc = 0;
    
    // 统计
    WarpStats m_stats;
};

// ============================================================================
// WarpScheduler - Warp 调度器
// ============================================================================
class WarpScheduler {
public:
    // 调度器配置
    struct Config {
        uint32_t warp_count = 4;        // Warp 数量
        uint32_t warp_size = 8;         // 每个 Warp 的线程数
        uint32_t max_cycles = 1000000;  // 最大周期数（防死循环）
        bool enable_multithreading = true; // 启用多线程
        bool enable_cycle_counting = true; // 启用周期计数
    };
    
    WarpScheduler();
    explicit WarpScheduler(const Config& config);
    ~WarpScheduler();
    
    // ========================================================================
    // Warp 管理
    // ========================================================================
    
    // 获取 Warp 数量
    uint32_t getWarpCount() const { return m_config.warp_count; }
    
    // 获取 Warp
    Warp& getWarp(uint32_t index) { return m_warps[index]; }
    const Warp& getWarp(uint32_t index) const { return m_warps[index]; }
    
    // 分配一个空闲 Warp
    Warp* allocateWarp();
    
    // 释放 Warp
    void freeWarp(uint32_t warp_id);
    
    // ========================================================================
    // Fragment 调度
    // ========================================================================
    
    // 提交 fragment 列表进行调度
    void submitFragments(const std::vector<FragmentContext>& fragments,
                         const ShaderFunction& shader);
    
    // 执行所有 Warp（单次调度轮次）
    void scheduleRound();
    
    // 运行直到所有 Warp 完成
    void run();
    
    // 停止调度
    void stop();
    
    // ========================================================================
    // 周期计数
    // ========================================================================
    
    uint64_t getCycleCount() const { return m_cycle_count; }
    
    void setMaxCycles(uint64_t max) { m_config.max_cycles = max; }
    uint64_t getMaxCycles() const { return m_config.max_cycles; }
    
    // ========================================================================
    // 配置
    // ========================================================================
    
    const Config& getConfig() const { return m_config; }
    Config& getConfig() { return m_config; }
    
    void setVerbose(bool v) { m_verbose = v; }
    bool isVerbose() const { return m_verbose; }
    
    // ========================================================================
    // 统计
    // ========================================================================
    
    struct SchedulerStats {
        uint64_t total_cycles = 0;
        uint64_t total_warps_scheduled = 0;
        uint64_t total_fragments_processed = 0;
        uint64_t total_instructions_executed = 0;
        uint64_t idle_cycles = 0;
        uint64_t stall_cycles = 0;
        
        // 利用率
        double warp_utilization() const {
            uint64_t active = total_cycles - idle_cycles;
            return (double)active / (double)total_cycles;
        }
    };
    
    const SchedulerStats& getStats() const { return m_stats; }
    SchedulerStats& getStats() { return m_stats; }
    
    void resetStats();
    
    // ========================================================================
    // 回调
    // ========================================================================
    
    // 每周期回调（用于调试/可视化）
    using CycleCallback = std::function<void(uint64_t cycle, const Warp*)>;
    void setCycleCallback(CycleCallback cb) { m_cycle_callback = std::move(cb); }
    
    // Warp 完成回调
    using WarpDoneCallback = std::function<void(Warp*)>;
    void setWarpDoneCallback(WarpDoneCallback cb) { m_warp_done_callback = std::move(cb); }
    
    // ========================================================================
    // 状态查询
    // ========================================================================
    
    bool isRunning() const { return m_running; }
    bool isStopped() const { return !m_running; }
    
    // 获取待处理的 fragment 数
    size_t pendingFragmentCount() const { return m_pending_fragments.size(); }
    
    // 获取活跃 Warp 数
    uint32_t activeWarpCount() const;
    
    // 重置调度器
    void reset();
    
private:
    Config m_config;
    std::vector<Warp> m_warps;
    std::vector<FragmentContext> m_pending_fragments;
    const ShaderFunction* m_current_shader = nullptr;
    
    uint64_t m_cycle_count = 0;
    bool m_running = false;
    bool m_verbose = false;
    
    SchedulerStats m_stats;
    
    // 回调
    CycleCallback m_cycle_callback;
    WarpDoneCallback m_warp_done_callback;
    
    // 内部方法
    void initializeWarps();
    void executeWarp(Warp& warp);
    bool allWarpsDone() const;
    void updateStats();
    
    // 多线程执行（可选）
    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    
    void runMultithreaded();
    void runSinglethreaded();
};

// ============================================================================
// 辅助函数
// ============================================================================

// 创建测试 shader
ShaderFunction makeTestWarpShader();

} // namespace SoftGPU
