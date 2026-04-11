// ============================================================================
// SoftGPU - WarpScheduler.hpp
// Warp Scheduler 实现
// Phase 2: Warp=8 调度器，支持多线程并行执行
// ============================================================================

#pragma once

#include "ShaderCore.hpp"
#include "core/PipelineTypes.hpp"
#include "isa/interpreter_v2_5.hpp"
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
// WarpBatchConfig - executeWarpBatch() 参数
// ============================================================================
struct WarpBatchConfig {
    uint32_t max_cycles_per_warp = 1024;     // 单 warp 最大执行周期（防死循环）
    uint32_t max_warps_to_schedule = 0;      // 本批次最多调度 warp 数（0=全部）
    bool enable_tile_write = true;           // 是否将结果写入 TileBuffer
    bool yield_on_stall = false;             // stall 时是否主动 yield（多线程模式）
};

// ============================================================================
// WarpBatchResult - executeWarpBatch() 返回值
// ============================================================================
struct WarpBatchResult {
    uint32_t warps_completed = 0;     // 本批次完成的 warp 数
    uint32_t fragments_written = 0;  // 写入 TileBuffer 的 fragment 数
    uint64_t cycles_this_batch = 0;  // 本批次执行的周期数
    bool all_done = false;            // 所有 warp 是否已完成
};

// ============================================================================
// TileBufferManager 前向声明
// ============================================================================
class TileBufferManager;

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
    
    // P0-1 核心接口：执行有限调度轮次后返回（非 blocking）
    // 供 FragmentShader 主循环控制节奏
    WarpBatchResult executeWarpBatch(WarpBatchConfig cfg);
    
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
        // P2-1: Warp divergence statistics
        uint32_t divergenceCount = 0;        // 分歧发生的次数
        uint32_t divergenceThreads = 0;     // 分歧涉及的总线程数
        uint64_t divergenceLostCycles = 0; // 因分歧损失的 cycles
        
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
    
    // 查询活跃 warp（不修改状态）
    Warp* getWarpIfActive(uint32_t index);
    const Warp* getWarpIfActive(uint32_t index) const;
    
    // 待调度队列是否非空
    bool hasPendingWork() const { return !m_pending_fragments.empty(); }
    
    // 单个 warp 重置
    void resetWarp(uint32_t warp_id);
    
    // ========================================================================
    // TileBuffer 注入（P0-1 委托模式）
    // ========================================================================
    void setTileBufferManager(TileBufferManager* tbm);
    TileBufferManager* getTileBufferManager() const { return m_tile_buffer_manager; }
    
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
    
    // P0-1: TileBuffer 委托写入
    TileBufferManager* m_tile_buffer_manager = nullptr;
    
    // P0-2: Per-scheduler interpreter for DIV pending tracking
    softgpu::isa::v2_5::Interpreter m_interpreter;
    
    // 内部方法
    void initializeWarps();
    void executeWarp(Warp& warp, bool enable_tile_write);
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
