// ============================================================================
// SoftGPU - WarpScheduler.cpp
// Warp Scheduler 实现
// ============================================================================

#include "WarpScheduler.hpp"
#include "ShaderCore.hpp"
#include "core/PipelineTypes.hpp"
#include "stages/TilingStage.hpp"
#include "isa/interpreter_v2_5.hpp"
#include "stages/TileBuffer.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>

// ============================================================================
// Shader Register Mapping (v2.5 ISA)
// ============================================================================
namespace ShaderRegs {
    // 输入寄存器 (R1-R9)
    constexpr uint8_t FRAG_X = 1;
    constexpr uint8_t FRAG_Y = 2;
    constexpr uint8_t FRAG_Z = 3;
    constexpr uint8_t COLOR_R = 4;
    constexpr uint8_t COLOR_G = 5;
    constexpr uint8_t COLOR_B = 6;
    constexpr uint8_t COLOR_A = 7;
    constexpr uint8_t TEX_U = 8;
    constexpr uint8_t TEX_V = 9;

    // 输出寄存器 (R10-R15)
    constexpr uint8_t OUT_R = 10;
    constexpr uint8_t OUT_G = 11;
    constexpr uint8_t OUT_B = 12;
    constexpr uint8_t OUT_A = 13;
    constexpr uint8_t OUT_Z = 14;
    constexpr uint8_t KILLED = 15;

    // 临时寄存器 (R16-R31)
    constexpr uint8_t TMP0 = 16;
}

namespace SoftGPU {

// ============================================================================
// Warp Implementation
// ============================================================================

Warp::Warp() {
    reset();
}

Warp::~Warp() {
}

void Warp::reset() {
    m_warp_id = INVALID_ID;
    m_state = State::IDLE;
    m_fragment_count = 0;
    m_shader = nullptr;
    m_pc = 0;
    resetStats();
    
    // 清空 fragments
    for (auto& frag : m_fragments) {
        frag.reset();
    }
}

void Warp::setFragments(const std::vector<FragmentContext>& fragments) {
    m_fragment_count = std::min<uint32_t>(WARP_SIZE, (uint32_t)fragments.size());
    for (uint32_t i = 0; i < m_fragment_count; ++i) {
        m_fragments[i] = fragments[i];
    }
    // Fill remaining with empty
    for (uint32_t i = m_fragment_count; i < WARP_SIZE; ++i) {
        m_fragments[i].reset();
    }
}

// ============================================================================
// WarpScheduler Implementation
// ============================================================================

WarpScheduler::WarpScheduler() 
    : WarpScheduler(Config()) {
}

WarpScheduler::WarpScheduler(const Config& config)
    : m_config(config) {
    
    // 初始化 Warps
    m_warps.resize(m_config.warp_count);
    for (uint32_t i = 0; i < m_config.warp_count; ++i) {
        m_warps[i].setId(i);
    }
    
    m_running = false;
}

WarpScheduler::~WarpScheduler() {
    stop();
}

void WarpScheduler::initializeWarps() {
    for (auto& warp : m_warps) {
        warp.reset();
    }
    m_cycle_count = 0;
}

Warp* WarpScheduler::allocateWarp() {
    for (auto& warp : m_warps) {
        if (warp.isIdle()) {
            warp.setState(Warp::State::RUNNING);
            m_stats.total_warps_scheduled++;
            return &warp;
        }
    }
    return nullptr;  // 没有空闲 Warp
}

void WarpScheduler::freeWarp(uint32_t warp_id) {
    if (warp_id < m_warps.size()) {
        m_warps[warp_id].reset();
        m_warps[warp_id].setId(warp_id);
    }
}

void WarpScheduler::submitFragments(const std::vector<FragmentContext>& fragments,
                                     const ShaderFunction& shader) {
    // 将 fragments 加入待处理队列
    m_pending_fragments.insert(m_pending_fragments.end(), 
                                fragments.begin(), 
                                fragments.end());
    m_current_shader = &shader;
    m_stats.total_fragments_processed += fragments.size();
}

void WarpScheduler::executeWarp(Warp& warp) {
    if (!warp.isRunning() || warp.getShader() == nullptr) {
        return;
    }
    
    const ShaderFunction* shader = warp.getShader();
    
    // v2.5: Load program once, then execute each lane
    if (!shader->empty()) {
        m_interpreter.LoadProgram(shader->code.data(), shader->code.size(), 0);
    }
    
    // 为每个 active lane 执行 shader
    for (uint32_t lane = 0; lane < warp.getFragmentCount(); ++lane) {
        FragmentContext& frag = warp.getFragment(lane);
        
        if (shader->empty()) {
            // Passthrough
            frag.out_r = frag.color_r;
            frag.out_g = frag.color_g;
            frag.out_b = frag.color_b;
            frag.out_a = frag.color_a;
            frag.out_z = frag.pos_z;
            continue;
        }
        
        // v2.5: Reset clears registers, PC, and stats
        m_interpreter.Reset();
        
        // 设置 fragment 输入到寄存器
        m_interpreter.SetRegister(ShaderRegs::FRAG_X, frag.pos_x);
        m_interpreter.SetRegister(ShaderRegs::FRAG_Y, frag.pos_y);
        m_interpreter.SetRegister(ShaderRegs::FRAG_Z, frag.pos_z);
        m_interpreter.SetRegister(ShaderRegs::COLOR_R, frag.color_r);
        m_interpreter.SetRegister(ShaderRegs::COLOR_G, frag.color_g);
        m_interpreter.SetRegister(ShaderRegs::COLOR_B, frag.color_b);
        m_interpreter.SetRegister(ShaderRegs::COLOR_A, frag.color_a);
        m_interpreter.SetRegister(ShaderRegs::TEX_U, frag.u);
        m_interpreter.SetRegister(ShaderRegs::TEX_V, frag.v);
        m_interpreter.SetRegister(ShaderRegs::OUT_R, 0.0f);
        m_interpreter.SetRegister(ShaderRegs::OUT_G, 0.0f);
        m_interpreter.SetRegister(ShaderRegs::OUT_B, 0.0f);
        m_interpreter.SetRegister(ShaderRegs::OUT_A, 1.0f);
        m_interpreter.SetRegister(ShaderRegs::OUT_Z, frag.pos_z);
        m_interpreter.SetRegister(ShaderRegs::KILLED, 0.0f);
        
        // v2.5: Run executes the full program (manages PC internally)
        // max_cycles = 1024 prevents infinite loops
        m_interpreter.Run(1024);
        
        // 更新统计
        const auto& st = m_interpreter.GetStats();
        warp.getStats().instructions_executed += st.instructions_executed;
        
        // 捕获输出
        frag.out_r = m_interpreter.GetRegister(ShaderRegs::OUT_R);
        frag.out_g = m_interpreter.GetRegister(ShaderRegs::OUT_G);
        frag.out_b = m_interpreter.GetRegister(ShaderRegs::OUT_B);
        frag.out_a = m_interpreter.GetRegister(ShaderRegs::OUT_A);
        frag.out_z = m_interpreter.GetRegister(ShaderRegs::OUT_Z);
        frag.killed = (m_interpreter.GetRegister(ShaderRegs::KILLED) != 0.0f);
    }
    
    // Warp 完成
    warp.finish();
    warp.getStats().cycles_active++;
    
    // 调用回调
    if (m_warp_done_callback) {
        m_warp_done_callback(&warp);
    }
}

bool WarpScheduler::allWarpsDone() const {
    for (const auto& warp : m_warps) {
        if (warp.isRunning()) {
            return false;
        }
    }
    // 检查是否有待处理的 fragments
    if (!m_pending_fragments.empty()) {
        return false;
    }
    return true;
}

void WarpScheduler::scheduleRound() {
    // 1. 尝试分配空闲 Warp 给待处理的 fragments
    while (!m_pending_fragments.empty()) {
        Warp* warp = allocateWarp();
        if (warp == nullptr) {
            break;  // 没有空闲 Warp
        }
        
        // 分配 fragment (Warp 大小)
        std::vector<FragmentContext> frag_batch;
        uint32_t batch_size = std::min(Warp::WARP_SIZE, (uint32_t)m_pending_fragments.size());
        frag_batch.reserve(batch_size);
        
        for (uint32_t i = 0; i < batch_size; ++i) {
            frag_batch.push_back(m_pending_fragments.back());
            m_pending_fragments.pop_back();
        }
        
        warp->setFragments(frag_batch);
        warp->setShader(m_current_shader);
        warp->setPC(0);
        warp->setState(Warp::State::RUNNING);
    }
    
    // 2. 执行所有 Running Warp
    for (auto& warp : m_warps) {
        if (warp.isRunning()) {
            executeWarp(warp);
        }
    }
    
    // 3. 更新周期计数
    if (m_config.enable_cycle_counting) {
        m_cycle_count++;
    }
    
    // 4. 调用周期回调
    if (m_cycle_callback) {
        for (const auto& warp : m_warps) {
            m_cycle_callback(m_cycle_count, &warp);
        }
    }
}

void WarpScheduler::run() {
    if (m_running) return;
    m_running = true;
    
    initializeWarps();
    
    if (m_config.enable_multithreading && m_config.warp_count > 1) {
        runMultithreaded();
    } else {
        runSinglethreaded();
    }
}

void WarpScheduler::runSinglethreaded() {
    while (m_running && !allWarpsDone()) {
        // 检查最大周期
        if (m_config.enable_cycle_counting && m_cycle_count >= m_config.max_cycles) {
            if (m_verbose) {
                std::cerr << "WarpScheduler: max cycles " << m_config.max_cycles 
                          << " reached, stopping" << std::endl;
            }
            break;
        }
        
        scheduleRound();
    }
    
    // 收集统计
    updateStats();
}

void WarpScheduler::runMultithreaded() {
    // Note: 简化实现，实际多线程需要更复杂的同步
    // 每个 Warp 可以有自己的线程执行
    
    m_threads.clear();
    m_threads.reserve(m_warps.size());
    
    // 启动 worker 线程
    for (size_t i = 0; i < m_warps.size(); ++i) {
        m_threads.emplace_back([this, i]() {
            while (m_running) {
                Warp& warp = m_warps[i];
                
                if (warp.isRunning()) {
                    executeWarp(warp);
                } else {
                    // 短暂休眠，避免忙等待
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        });
    }
    
    // 主调度线程
    while (m_running && !allWarpsDone()) {
        if (m_config.enable_cycle_counting && m_cycle_count >= m_config.max_cycles) {
            break;
        }
        
        scheduleRound();
        
        // 短暂休眠
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // 等待所有 worker 结束
    m_running = false;
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
    
    // 收集统计
    updateStats();
}

void WarpScheduler::stop() {
    m_running = false;
    
    // 等待线程结束
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_threads.clear();
}

void WarpScheduler::updateStats() {
    m_stats.total_cycles = m_cycle_count;
    m_stats.total_warps_scheduled = m_warps.size() > 0 ? 
        std::count_if(m_warps.begin(), m_warps.end(), 
                      [](const Warp& w) { return !w.isIdle(); }) : 0;
    
    // 计算空闲周期
    m_stats.idle_cycles = 0;
    for (const auto& warp : m_warps) {
        if (warp.isIdle()) {
            m_stats.idle_cycles++;
        }
    }
}

void WarpScheduler::resetStats() {
    m_stats = SchedulerStats();
    m_cycle_count = 0;
    for (auto& warp : m_warps) {
        warp.resetStats();
    }
}

uint32_t WarpScheduler::activeWarpCount() const {
    return (uint32_t)std::count_if(m_warps.begin(), m_warps.end(),
                                    [](const Warp& w) { return w.isRunning(); });
}

void WarpScheduler::reset() {
    stop();
    initializeWarps();
    m_pending_fragments.clear();
    resetStats();
    m_current_shader = nullptr;
}

// ============================================================================
// P0-1 新增接口实现
// ============================================================================

void WarpScheduler::setTileBufferManager(TileBufferManager* tbm) {
    m_tile_buffer_manager = tbm;
}

Warp* WarpScheduler::getWarpIfActive(uint32_t index) {
    if (index >= m_warps.size()) return nullptr;
    Warp& warp = m_warps[index];
    if (warp.isRunning()) {
        return &warp;
    }
    return nullptr;
}

const Warp* WarpScheduler::getWarpIfActive(uint32_t index) const {
    if (index >= m_warps.size()) return nullptr;
    const Warp& warp = m_warps[index];
    if (warp.isRunning()) {
        return &warp;
    }
    return nullptr;
}

void WarpScheduler::resetWarp(uint32_t warp_id) {
    if (warp_id < m_warps.size()) {
        m_warps[warp_id].reset();
        m_warps[warp_id].setId(warp_id);
    }
}

WarpBatchResult WarpScheduler::executeWarpBatch(WarpBatchConfig cfg) {
    WarpBatchResult result;
    uint64_t start_cycle = m_cycle_count;
    
    // 1. 尝试分配空闲 Warp 给待处理的 fragments
    uint32_t warps_scheduled_this_batch = 0;
    while (!m_pending_fragments.empty()) {
        // 检查 max_warps_to_schedule 限制
        if (cfg.max_warps_to_schedule > 0 && 
            warps_scheduled_this_batch >= cfg.max_warps_to_schedule) {
            break;
        }
        
        Warp* warp = allocateWarp();
        if (warp == nullptr) {
            break;  // 没有空闲 Warp
        }
        
        // 分配 fragment (Warp 大小)
        std::vector<FragmentContext> frag_batch;
        uint32_t batch_size = std::min(Warp::WARP_SIZE, (uint32_t)m_pending_fragments.size());
        frag_batch.reserve(batch_size);
        
        for (uint32_t i = 0; i < batch_size; ++i) {
            frag_batch.push_back(m_pending_fragments.back());
            m_pending_fragments.pop_back();
        }
        
        warp->setFragments(frag_batch);
        warp->setShader(m_current_shader);
        warp->setPC(0);
        warp->setState(Warp::State::RUNNING);
        warps_scheduled_this_batch++;
    }
    
    // 2. 执行所有 Running Warp
    for (auto& warp : m_warps) {
        if (warp.isRunning()) {
            // 传递 tile buffer 写入开关
            executeWarpWithTileWrite(warp, cfg.enable_tile_write);
            
            if (warp.isDone()) {
                result.warps_completed++;
            }
        }
    }
    
    // 4. 更新周期计数
    if (m_config.enable_cycle_counting) {
        m_cycle_count++;
    }
    
    // 5. 检查是否 all_done
    result.all_done = allWarpsDone();
    result.cycles_this_batch = m_cycle_count - start_cycle;
    
    // 6. 调用周期回调
    if (m_cycle_callback) {
        for (const auto& warp : m_warps) {
            m_cycle_callback(m_cycle_count, &warp);
        }
    }
    
    return result;
}

// 内部：执行单个 warp 并可选写入 TileBuffer
void WarpScheduler::executeWarpWithTileWrite(Warp& warp, bool enable_tile_write) {
    if (!warp.isRunning() || warp.getShader() == nullptr) {
        return;
    }
    
    const ShaderFunction* shader = warp.getShader();
    
    // v2.5: Load program once, then execute each lane
    if (!shader->empty()) {
        m_interpreter.LoadProgram(shader->code.data(), shader->code.size(), 0);
    }
    
    // 为每个 active lane 执行 shader
    for (uint32_t lane = 0; lane < warp.getFragmentCount(); ++lane) {
        FragmentContext& frag = warp.getFragment(lane);
        
        if (shader->empty()) {
            // Passthrough
            frag.out_r = frag.color_r;
            frag.out_g = frag.color_g;
            frag.out_b = frag.color_b;
            frag.out_a = frag.color_a;
            frag.out_z = frag.pos_z;
        } else {
            // v2.5: Reset clears registers, PC, and stats
            m_interpreter.Reset();
            
            // 设置 fragment 输入到寄存器
            m_interpreter.SetRegister(ShaderRegs::FRAG_X, frag.pos_x);
            m_interpreter.SetRegister(ShaderRegs::FRAG_Y, frag.pos_y);
            m_interpreter.SetRegister(ShaderRegs::FRAG_Z, frag.pos_z);
            m_interpreter.SetRegister(ShaderRegs::COLOR_R, frag.color_r);
            m_interpreter.SetRegister(ShaderRegs::COLOR_G, frag.color_g);
            m_interpreter.SetRegister(ShaderRegs::COLOR_B, frag.color_b);
            m_interpreter.SetRegister(ShaderRegs::COLOR_A, frag.color_a);
            m_interpreter.SetRegister(ShaderRegs::TEX_U, frag.u);
            m_interpreter.SetRegister(ShaderRegs::TEX_V, frag.v);
            m_interpreter.SetRegister(ShaderRegs::OUT_R, 0.0f);
            m_interpreter.SetRegister(ShaderRegs::OUT_G, 0.0f);
            m_interpreter.SetRegister(ShaderRegs::OUT_B, 0.0f);
            m_interpreter.SetRegister(ShaderRegs::OUT_A, 1.0f);
            m_interpreter.SetRegister(ShaderRegs::OUT_Z, frag.pos_z);
            m_interpreter.SetRegister(ShaderRegs::KILLED, 0.0f);
            
            // v2.5: Run executes the full program (manages PC internally)
            // max_cycles = 1024 prevents infinite loops
            m_interpreter.Run(1024);
            
            // 更新统计
            const auto& st = m_interpreter.GetStats();
            warp.getStats().instructions_executed += st.instructions_executed;
            
            // 捕获输出
            frag.out_r = m_interpreter.GetRegister(ShaderRegs::OUT_R);
            frag.out_g = m_interpreter.GetRegister(ShaderRegs::OUT_G);
            frag.out_b = m_interpreter.GetRegister(ShaderRegs::OUT_B);
            frag.out_a = m_interpreter.GetRegister(ShaderRegs::OUT_A);
            frag.out_z = m_interpreter.GetRegister(ShaderRegs::OUT_Z);
            frag.killed = (m_interpreter.GetRegister(ShaderRegs::KILLED) != 0.0f);
        }
        
        // P0-1: 委托模式写入 TileBuffer
        if (enable_tile_write && m_tile_buffer_manager != nullptr && !frag.killed) {
            uint32_t tile_idx = frag.tile_y * NUM_TILES_X + frag.tile_x;  // row-major: tileY * NUM_TILES_X + tileX
            uint32_t local_x = static_cast<uint32_t>(frag.pos_x) - frag.tile_x * TILE_WIDTH;
            uint32_t local_y = static_cast<uint32_t>(frag.pos_y) - frag.tile_y * TILE_HEIGHT;
            
            float color[4] = {frag.out_r, frag.out_g, frag.out_b, frag.out_a};
            bool written = m_tile_buffer_manager->depthTestAndWrite(
                tile_idx, local_x, local_y, frag.out_z, color);
            
            if (written) {
                warp.getStats().instructions_executed++;  // reuse stat for written count
            }
        }
    }
    
    // Warp 完成
    warp.finish();
    warp.getStats().cycles_active++;
    
    // 调用回调
    if (m_warp_done_callback) {
        m_warp_done_callback(&warp);
    }
}

// ============================================================================
// 辅助函数
// ============================================================================

ShaderFunction makeTestWarpShader() {
    using softgpu::isa::v2_5::Instruction;
    using softgpu::isa::v2_5::Opcode;
    
    ShaderFunction shader;
    
    // Simple test shader: passthrough color (v2.5 format)
    shader.code = {
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_R, ShaderRegs::COLOR_R).word1,  // MOV OUT_R, COLOR_R
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_G, ShaderRegs::COLOR_G).word1,  // MOV OUT_G, COLOR_G
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_B, ShaderRegs::COLOR_B).word1,  // MOV OUT_B, COLOR_B
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_A, ShaderRegs::COLOR_A).word1,  // MOV OUT_A, COLOR_A
        Instruction::MakeC(Opcode::MOV, ShaderRegs::OUT_Z, ShaderRegs::FRAG_Z).word1,  // MOV OUT_Z, FRAG_Z
        Instruction::MakeD(Opcode::HALT).word1
    };
    shader.start_addr = 0;
    
    return shader;
}

} // namespace SoftGPU
