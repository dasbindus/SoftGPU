// ============================================================================
// SoftGPU - WarpScheduler.cpp
// Warp Scheduler 实现
// ============================================================================

#include "WarpScheduler.hpp"
#include "ShaderCore.hpp"
#include "core/PipelineTypes.hpp"
#include "stages/TilingStage.hpp"
#include "isa/ISA.hpp"
#include "stages/TileBuffer.hpp"
#include <cstring>
#include <algorithm>
#include <iostream>

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
    
    // PHASE3: Reuse Interpreter for ISA execution
    // Each warp uses the shared Interpreter from WarpScheduler
    // thread_local ensures each thread has its own Interpreter instance
    // This fixes non-determinism bug in multi-threaded mode where multiple
    // threads were sharing the same static Interpreter causing race conditions
    thread_local static softgpu::isa::Interpreter warpInterpreter;
    warpInterpreter.Reset();
    
    // 为每个 active lane 执行 shader
    for (uint32_t lane = 0; lane < warp.getFragmentCount(); ++lane) {
        FragmentContext& frag = warp.getFragment(lane);
        
        // 获取 shader 代码
        if (shader->empty()) {
            // Passthrough
            frag.out_r = frag.color_r;
            frag.out_g = frag.color_g;
            frag.out_b = frag.color_b;
            frag.out_a = frag.color_a;
            frag.out_z = frag.pos_z;
            continue;
        }
        
        // 设置 fragment 输入到寄存器
        warpInterpreter.SetRegister(1, frag.pos_x);
        warpInterpreter.SetRegister(2, frag.pos_y);
        warpInterpreter.SetRegister(3, frag.pos_z);
        warpInterpreter.SetRegister(4, frag.color_r);
        warpInterpreter.SetRegister(5, frag.color_g);
        warpInterpreter.SetRegister(6, frag.color_b);
        warpInterpreter.SetRegister(7, frag.color_a);
        warpInterpreter.SetRegister(8, frag.u);
        warpInterpreter.SetRegister(9, frag.v);
        warpInterpreter.SetRegister(10, 0.0f);  // OUT_R
        warpInterpreter.SetRegister(11, 0.0f);  // OUT_G
        warpInterpreter.SetRegister(12, 0.0f);  // OUT_B
        warpInterpreter.SetRegister(13, 1.0f);  // OUT_A
        warpInterpreter.SetRegister(14, frag.pos_z);  // OUT_Z
        warpInterpreter.SetRegister(15, 0.0f);  // KILLED
        
        // 执行 shader 指令序列
        uint32_t max_instr = 256;
        uint32_t instr_count = 0;
        uint32_t pc = 0;
        
        while (instr_count < max_instr) {
            if (pc / 4 >= shader->code.size()) {
                break;
            }
            
            uint32_t instr_word = shader->code[pc / 4];
            softgpu::isa::Instruction inst(instr_word);
            softgpu::isa::Opcode op = inst.GetOpcode();
            
            if (op == softgpu::isa::Opcode::INVALID || op == softgpu::isa::Opcode::NOP) {
                break;
            }
            
            // 使用 Interpreter 执行指令
            warpInterpreter.ExecuteInstruction(inst);
            warpInterpreter.SetRegister(0, 0.0f);  // R0 = zero
            
            pc += 4;
            warp.getStats().instructions_executed++;
            instr_count++;
        }
        
        // 捕获输出
        frag.out_r = warpInterpreter.GetRegister(10);
        frag.out_g = warpInterpreter.GetRegister(11);
        frag.out_b = warpInterpreter.GetRegister(12);
        frag.out_a = warpInterpreter.GetRegister(13);
        frag.out_z = warpInterpreter.GetRegister(14);
        frag.killed = (warpInterpreter.GetRegister(15) != 0.0f);
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
    
    // 3. 更新周期计数
    if (m_config.enable_cycle_counting) {
        m_cycle_count++;
    }
    
    // 4. 检查是否 all_done
    result.all_done = allWarpsDone();
    result.cycles_this_batch = m_cycle_count - start_cycle;
    
    // 5. 调用周期回调
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
    
    thread_local static softgpu::isa::Interpreter warpInterpreter;
    warpInterpreter.Reset();
    
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
            // 设置 fragment 输入到寄存器
            warpInterpreter.SetRegister(1, frag.pos_x);
            warpInterpreter.SetRegister(2, frag.pos_y);
            warpInterpreter.SetRegister(3, frag.pos_z);
            warpInterpreter.SetRegister(4, frag.color_r);
            warpInterpreter.SetRegister(5, frag.color_g);
            warpInterpreter.SetRegister(6, frag.color_b);
            warpInterpreter.SetRegister(7, frag.color_a);
            warpInterpreter.SetRegister(8, frag.u);
            warpInterpreter.SetRegister(9, frag.v);
            warpInterpreter.SetRegister(10, 0.0f);  // OUT_R
            warpInterpreter.SetRegister(11, 0.0f);  // OUT_G
            warpInterpreter.SetRegister(12, 0.0f);  // OUT_B
            warpInterpreter.SetRegister(13, 1.0f);  // OUT_A
            warpInterpreter.SetRegister(14, frag.pos_z);  // OUT_Z
            warpInterpreter.SetRegister(15, 0.0f);  // KILLED
            
            // 执行 shader 指令序列
            uint32_t max_instr = 256;
            uint32_t instr_count = 0;
            uint32_t pc = 0;
            
            while (instr_count < max_instr) {
                if (pc / 4 >= shader->code.size()) {
                    break;
                }
                
                uint32_t instr_word = shader->code[pc / 4];
                softgpu::isa::Instruction inst(instr_word);
                softgpu::isa::Opcode op = inst.GetOpcode();
                
                if (op == softgpu::isa::Opcode::INVALID || op == softgpu::isa::Opcode::NOP) {
                    break;
                }
                
                warpInterpreter.ExecuteInstruction(inst);
                warpInterpreter.SetRegister(0, 0.0f);  // R0 = zero
                
                pc += 4;
                warp.getStats().instructions_executed++;
                instr_count++;
            }
            
            // 捕获输出
            frag.out_r = warpInterpreter.GetRegister(10);
            frag.out_g = warpInterpreter.GetRegister(11);
            frag.out_b = warpInterpreter.GetRegister(12);
            frag.out_a = warpInterpreter.GetRegister(13);
            frag.out_z = warpInterpreter.GetRegister(14);
            frag.killed = (warpInterpreter.GetRegister(15) != 0.0f);
        }
        
        // P0-1: 委托模式写入 TileBuffer
        if (enable_tile_write && m_tile_buffer_manager != nullptr && !frag.killed) {
            uint32_t tile_idx = frag.tile_x * NUM_TILES_X + frag.tile_y;
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
    using softgpu::isa::Instruction;
    using softgpu::isa::Opcode;
    
    ShaderFunction shader;
    
    // Simple test shader: passthrough color
    shader.code = {
        Instruction::MakeU(Opcode::MOV, 10, 4).raw,  // MOV OUT_R, COLOR_R
        Instruction::MakeU(Opcode::MOV, 11, 5).raw,  // MOV OUT_G, COLOR_G
        Instruction::MakeU(Opcode::MOV, 12, 6).raw,  // MOV OUT_B, COLOR_B
        Instruction::MakeU(Opcode::MOV, 13, 7).raw,  // MOV OUT_A, COLOR_A
        Instruction::MakeU(Opcode::MOV, 14, 3).raw,  // MOV OUT_Z, FRAG_Z
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}

} // namespace SoftGPU
