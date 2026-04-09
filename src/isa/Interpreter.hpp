#pragma once

#include "Opcode.hpp"
#include "Instruction.hpp"
#include "RegisterFile.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include "../pipeline/TextureBuffer.hpp"

namespace softgpu {
namespace isa {

// ============================================================================
// ISA Interpreter
// ============================================================================
// Executes instructions sequentially in a single-threaded manner
// Used for functional simulation and testing
// ============================================================================

class Interpreter
{
public:
    // Memory interface for LD/ST
    class Memory
    {
    public:
        Memory(size_t size = 1024 * 1024) : size_(size), data_(size, 0) {}
        
        float Load32(uint32_t addr)
        {
            if (addr + 4 > size_) return 0.0f;
            float val;
            std::memcpy(&val, &data_[addr], sizeof(float));
            return val;
        }
        
        void Store32(uint32_t addr, float value)
        {
            if (addr + 4 > size_) return;
            std::memcpy(&data_[addr], &value, sizeof(float));
        }
        
        uint8_t* GetData() { return data_.data(); }
        size_t GetSize() const { return size_; }
        
    private:
        size_t size_;
        std::vector<uint8_t> data_;
    };

    // Program counter
    struct PC
    {
        uint32_t addr;
        uint32_t link; // For CALL/RET
        
        PC() : addr(0), link(0) {}
    };

    // Statistics
    struct Stats
    {
        uint64_t cycles;
        uint64_t instructions_executed;
        uint64_t branches_taken;
        uint64_t loads;
        uint64_t stores;
        
        Stats() { Reset(); }
        
        void Reset()
        {
            cycles = 0;
            instructions_executed = 0;
            branches_taken = 0;
            loads = 0;
            stores = 0;
        }
    };

private:
    RegisterFile reg_file_;
    PC pc_;
    Stats stats_;
    
    // Memory reference
    Memory memory_;

    // P0-2: DIV cycle-accurate simulation
    static constexpr uint32_t DIV_LATENCY = 7;  // DIV takes 7 cycles
    
    struct PendingDiv {
        uint8_t rd;
        float result;
        uint64_t completion_cycle;
    };
    std::vector<PendingDiv> m_pending_divs;

    // P1-3: Texture buffers (最多 4 个纹理)
    SoftGPU::TextureBuffer* m_textureBuffers[4] = {nullptr, nullptr, nullptr, nullptr};

    // VS: Vertex Buffer Object data
    std::vector<float> m_vbo_data;
    size_t m_vbo_count = 0;

    // VS: Vertex output buffer (VOUTPUTBUF) - 256 bytes = 64 floats = 16 vertices × 4 components
    static constexpr size_t MAX_VERTICES = 16;
    static constexpr size_t VOUTPUT_BUF_SIZE = MAX_VERTICES * 4;
    std::vector<float> m_voutput_buf;
    size_t m_vertex_count = 0;

public:
    Interpreter() : memory_(1024 * 1024), m_voutput_buf(VOUTPUT_BUF_SIZE, 0.0f), m_vertex_count(0) {}
    
    // Initialize with program (code as vector of 32-bit words)
    void LoadProgram([[maybe_unused]] const uint32_t* code, [[maybe_unused]] size_t word_count, uint32_t start_addr = 0)
    {
        // Simple loader: copy code to memory
        // In real implementation, this would be more sophisticated
        pc_.addr = start_addr;
        pc_.link = 0;
        reg_file_.Reset();
        stats_.Reset();
    }

    // Set a value in memory (for LD/ST testing)
    void SetMemory(uint32_t addr, float value)
    {
        memory_.Store32(addr, value);
    }
    
    float GetMemory(uint32_t addr)
    {
        return memory_.Load32(addr);
    }

    // Get register value
    float GetRegister(uint8_t reg) const { return reg_file_.Read(reg); }
    
    // Set register value
    void SetRegister(uint8_t reg, float value) { reg_file_.Write(reg, value); }

    // Get PC
    uint32_t GetPC() const { return pc_.addr; }
    
    // Get stats
    const Stats& GetStats() const { return stats_; }

    // P1-3: Set texture buffer for TEX/SAMPLE instructions
    void setTextureBuffer(int index, SoftGPU::TextureBuffer* tex) {
        if (index >= 0 && index < 4) {
            m_textureBuffers[index] = tex;
        }
    }

    // P0-2: Only drain pending DIVs, don't fetch/decode/execute
    // Called by WarpScheduler each cycle before executing instructions
    // Also advances stats_.cycles by 1 to simulate one cycle passing
    void drainPendingDIVs() {
        uint64_t current_cycle = stats_.cycles;
        for (auto it = m_pending_divs.begin(); it != m_pending_divs.end(); ) {
            if (it->completion_cycle <= current_cycle) {
                reg_file_.Write(it->rd, it->result);
                it = m_pending_divs.erase(it);
            } else {
                ++it;
            }
        }
        stats_.cycles++;  // Advance one cycle
    }

    // ========================================================================
    // VS Runner Support Methods
    // ========================================================================

    // Set VBO (Vertex Buffer Object) data for VLOAD instructions
    void SetVBO(const float* data, size_t count) {
        m_vbo_data.assign(data, data + count);
        m_vbo_count = count;
    }

    // Get the number of processed vertices
    size_t GetVertexCount() const { return m_vertex_count; }

    // Get a single float from the VOUTPUT buffer
    // vertex_idx: 0-based vertex index
    // attribute_offset: 0=x, 1=y, 2=z, 3=w
    float GetVOutputFloat(int vertex_idx, int attribute_offset) const {
        if (vertex_idx < 0 || attribute_offset < 0) return 0.0f;
        size_t idx = static_cast<size_t>(vertex_idx) * 4 + static_cast<size_t>(attribute_offset);
        if (idx >= m_voutput_buf.size()) return 0.0f;
        return m_voutput_buf[idx];
    }

    // Get pointer to VOUTPUT buffer data (const)
    const float* GetVOutputBufData() const { return m_voutput_buf.data(); }

    // Get size of VOUTPUT buffer in floats
    size_t GetVOutputBufSize() const { return m_voutput_buf.size(); }

    // Reset VS state (VBO, VOUTPUT buffer, vertex count)
    void ResetVS() {
        m_vbo_data.clear();
        m_vbo_count = 0;
        m_voutput_buf.assign(VOUTPUT_BUF_SIZE, 0.0f);
        m_vertex_count = 0;
    }

    // Single step execution
    bool Step()
    {
        // P0-2: Complete any pending DIV operations whose latency has elapsed
        drainPendingDIVs();
        
        // Fetch
        uint32_t instruction_word = 0; // Would fetch from I-cache in real impl
        Instruction inst(instruction_word); // Simplified
        
        // Decode
        Opcode op = inst.GetOpcode();
        
        if (op == Opcode::INVALID) {
            return false; // Stop on invalid instruction
        }
        
        // Execute (ExecuteInstruction advances cycles internally)
        ExecuteInstruction(inst);
        
        return (op != Opcode::RET && op != Opcode::VS_HALT); // Continue until RET or VS_HALT
    }

    // Run program until HALT or max cycles
    void Run(uint64_t max_cycles = 1000000)
    {
        while (stats_.cycles < max_cycles) {
            if (!Step()) break;
        }
    }

    // Execute a single instruction (main execution logic)
    void ExecuteInstruction(const Instruction& inst)
    {
        Opcode op = inst.GetOpcode();
        
        // Get operands
        uint8_t rd = inst.GetRd();
        uint8_t ra = inst.GetRa();
        uint8_t rb = inst.GetRb();
        float val_a = reg_file_.Read(ra);
        float val_b = reg_file_.Read(rb);
        
        stats_.instructions_executed++;
        
        switch (op) {
        // === Control Flow ===
        case Opcode::NOP:
            pc_.addr += 4;
            break;
            
        case Opcode::BRA: {
            float cond = reg_file_.Read(ra); // Rc is in Ra field for BRA
            int16_t offset = inst.GetSignedImm();
            if (cond != 0.0f) {
                pc_.addr += static_cast<uint32_t>(offset * 4);
                stats_.branches_taken++;
            } else {
                pc_.addr += 4;
            }
            break;
        }
        
        case Opcode::JMP: {
            int16_t offset = inst.GetSignedImm();
            pc_.addr += static_cast<uint32_t>(offset * 4);
            break;
        }
        
        case Opcode::CALL: {
            pc_.link = pc_.addr + 4;
            int16_t offset = inst.GetSignedImm();
            pc_.addr += static_cast<uint32_t>(offset * 4);
            reg_file_.Write(1, *reinterpret_cast<float*>(&pc_.link)); // Write LR to R1
            break;
        }
        
        case Opcode::RET:
            pc_.addr = pc_.link;
            break;
        
        // === ALU Operations ===
        case Opcode::ADD:
            reg_file_.Write(rd, val_a + val_b);
            pc_.addr += 4;
            break;
            
        case Opcode::SUB:
            reg_file_.Write(rd, val_a - val_b);
            pc_.addr += 4;
            break;
            
        case Opcode::MUL:
            reg_file_.Write(rd, val_a * val_b);
            pc_.addr += 4;
            break;
            
        case Opcode::DIV: {
            // P0-2: DIV takes DIV_LATENCY cycles (not 1)
            // Instead of immediate write-back, add to pending queue
            float result = (val_b != 0.0f) ? (val_a / val_b) : std::numeric_limits<float>::infinity();
            PendingDiv pending;
            pending.rd = rd;
            pending.result = result;
            pending.completion_cycle = stats_.cycles + DIV_LATENCY;
            m_pending_divs.push_back(pending);
            // DIV result not written yet; will be written after DIV_LATENCY cycles
            pc_.addr += 4;
            break;
        }
            
        case Opcode::MAD: {
            // MAD: Rd = Ra * Rb + Rc (Rc is stored in immediate for simplicity)
            float val_c = reg_file_.Read(inst.GetRc());
            reg_file_.Write(rd, val_a * val_b + val_c);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::AND: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            uint32_t ib = *reinterpret_cast<uint32_t*>(&val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia & ib;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::OR: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            uint32_t ib = *reinterpret_cast<uint32_t*>(&val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia | ib;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::CMP:
            reg_file_.Write(rd, (val_a < val_b) ? 1.0f : 0.0f);
            pc_.addr += 4;
            break;
        
        case Opcode::SEL: {
            // SEL: Rd = (Rc != 0) ? Ra : Rb
            float val_c = reg_file_.Read(inst.GetRc());
            reg_file_.Write(rd, (val_c != 0.0f) ? val_a : val_b);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::MIN:
            reg_file_.Write(rd, (val_a < val_b) ? val_a : val_b);
            pc_.addr += 4;
            break;
        
        case Opcode::MAX:
            reg_file_.Write(rd, (val_a > val_b) ? val_a : val_b);
            pc_.addr += 4;
            break;
        
        case Opcode::MOV:
            reg_file_.Write(rd, val_a);
            pc_.addr += 4;
            break;
        
        // === SFU Operations ===
        case Opcode::RCP:
            reg_file_.Write(rd, (val_a != 0.0f) ? (1.0f / val_a) : std::numeric_limits<float>::infinity());
            pc_.addr += 4;
            break;
        
        case Opcode::SQRT:
            reg_file_.Write(rd, (val_a >= 0.0f) ? std::sqrt(val_a) : std::nanf(""));
            pc_.addr += 4;
            break;
        
        case Opcode::RSQ:
            if (val_a > 0.0f) {
                reg_file_.Write(rd, 1.0f / std::sqrt(val_a));
            } else if (val_a == 0.0f) {
                reg_file_.Write(rd, std::numeric_limits<float>::infinity());
            } else {
                reg_file_.Write(rd, std::nanf(""));
            }
            pc_.addr += 4;
            break;
        
        case Opcode::F2I: {
            int32_t i = static_cast<int32_t>(val_a);
            reg_file_.Write(rd, *reinterpret_cast<float*>(&i));
            pc_.addr += 4;
            break;
        }
        
        case Opcode::I2F: {
            int32_t i = *reinterpret_cast<int32_t*>(&val_a);
            reg_file_.Write(rd, static_cast<float>(i));
            pc_.addr += 4;
            break;
        }
        
        case Opcode::FRACT:
            reg_file_.Write(rd, val_a - std::floor(val_a));
            pc_.addr += 4;
            break;
        
        // === Memory Operations ===
        case Opcode::LD: {
            uint16_t offset = inst.GetImm();
            // P0 Fix: Validate val_a is in range [0, SIZE_MAX] before casting
            // Check for NaN, infinity, or out-of-range values
            float value = 0.0f;
            if (std::isnan(val_a) || std::isinf(val_a) ||
                val_a < 0.0f || val_a > static_cast<float>(memory_.GetSize())) {
                // Invalid address, return 0.0f
                value = 0.0f;
            } else {
                uint32_t base = static_cast<uint32_t>(val_a);
                // P0 Fix: Check for integer overflow in address calculation
                if (base > memory_.GetSize() - 4) {
                    value = 0.0f;
                } else {
                    uint32_t addr = base + offset;
                    // P0 Fix: Ensure addr + 4 doesn't exceed memory bounds
                    if (addr + 4 > memory_.GetSize()) {
                        value = 0.0f;
                    } else {
                        value = memory_.Load32(addr);
                    }
                }
            }
            reg_file_.Write(rd, value);
            stats_.loads++;
            pc_.addr += 4;
            break;
        }
        
        case Opcode::ST: {
            uint16_t offset = inst.GetImm();
            // P0 Fix: Validate val_a is in range before casting
            if (!std::isnan(val_a) && !std::isinf(val_a) &&
                val_a >= 0.0f && val_a <= static_cast<float>(memory_.GetSize())) {
                uint32_t base = static_cast<uint32_t>(val_a);
                // P0 Fix: Check for integer overflow
                if (base <= memory_.GetSize() - 4) {
                    uint32_t addr = base + offset;
                    // P0 Fix: Ensure addr + 4 doesn't exceed memory bounds
                    if (addr + 4 <= memory_.GetSize()) {
                        memory_.Store32(addr, val_b);
                    }
                }
            }
            stats_.stores++;
            pc_.addr += 4;
            break;
        }
        
        // === Special Operations (Simplified) ===
        case Opcode::TEX:
        case Opcode::SAMPLE: {
            // TEX Rd, Ra(u), Rb(v), Rc(texture_id)
            // 真实纹理采样：如果纹理缓冲区存在且有效
            // 否则 fallback 到 checkerboard
            float u = val_a;
            float v = val_b;
            float val_c = reg_file_.Read(inst.GetRc());
            int tex_id = static_cast<int>(val_c);

            // 真实纹理采样
            if (tex_id >= 0 && tex_id < 4 && m_textureBuffers[tex_id] != nullptr) {
                auto color = m_textureBuffers[tex_id]->sampleNearest(u, v);
                reg_file_.Write(rd, color.r);
                reg_file_.Write(rd + 1, color.g);
                reg_file_.Write(rd + 2, color.b);
                reg_file_.Write(rd + 3, color.a);
            } else {
                // Fallback: checkerboard
                int cx = static_cast<int>(std::floor(u * 8.0f));
                int cy = static_cast<int>(std::floor(v * 8.0f));
                bool is_white = ((cx + cy) % 2) == 0;
                float color = is_white ? 1.0f : 0.0f;
                reg_file_.Write(rd, color);
                reg_file_.Write(rd + 1, color);
                reg_file_.Write(rd + 2, color);
                reg_file_.Write(rd + 3, 1.0f);
            }
            pc_.addr += 4;
            break;
        }
        
        case Opcode::LDC:
        case Opcode::BAR:
            // Stub implementations for v1.0
            pc_.addr += 4;
            break;
        
        // === PHASE3: Bitwise & Math Extensions ===
        case Opcode::SHL: {
            // SHL: Rd = Ra << Rb (shift left by float bits converted to int)
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            int shift = static_cast<int>(val_b) & 31;  // clamp to [0,31]
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia << shift;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::SHR: {
            // SHR: Rd = Ra >> Rb (shift right by float bits converted to int)
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            int shift = static_cast<int>(val_b) & 31;  // clamp to [0,31]
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia >> shift;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::NOT: {
            // NOT: Rd = ~Ra (bitwise NOT)
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ~ia;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::FLOOR:
            reg_file_.Write(rd, std::floor(val_a));
            pc_.addr += 4;
            break;
        
        case Opcode::CEIL:
            reg_file_.Write(rd, std::ceil(val_a));
            pc_.addr += 4;
            break;
        
        case Opcode::ABS:
            reg_file_.Write(rd, std::fabs(val_a));
            pc_.addr += 4;
            break;
        
        case Opcode::NEG:
            reg_file_.Write(rd, -val_a);
            pc_.addr += 4;
            break;
        
        case Opcode::SMOOTHSTEP: {
            // SMOOTHSTEP: Rd = smoothstep(edge0, edge1, x)
            // GLSL convention: smoothstep(edge0, edge1, x)
            // Returns 0 if x < edge0, 1 if x > edge1, smooth Hermite interpolation between
            // For R4 type: Ra=edge0, Rb=edge1, Rc=x (following MAD convention)
            float edge0 = val_a;
            float edge1 = val_b;
            float x = reg_file_.Read(inst.GetRc());
            float result;
            if (edge1 == edge0) {
                result = 0.0f;  // Avoid division by zero
            } else if (x < edge0) {
                result = 0.0f;
            } else if (x > edge1) {
                result = 1.0f;
            } else {
                float t = (x - edge0) / (edge1 - edge0);
                result = t * t * (3.0f - 2.0f * t);  // Hermite interpolation
            }
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::DP3: {
            // DP3: Rd = dot(Ra.xyz, Rb.xyz) = Ra.x*Rb.x + Ra.y*Rb.y + Ra.z*Rb.z
            // Ra and Rb must be 4-aligned (Ra % 4 == 0)
            float v0 = reg_file_.Read(ra);       // Ra   = x component
            float v1 = reg_file_.Read(ra + 1);  // Ra+1 = y component
            float v2 = reg_file_.Read(ra + 2);  // Ra+2 = z component
            float r0 = reg_file_.Read(rb);       // Rb   = x component
            float r1 = reg_file_.Read(rb + 1);  // Rb+1 = y component
            float r2 = reg_file_.Read(rb + 2);  // Rb+2 = z component
            float result = v0 * r0 + v1 * r1 + v2 * r2;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        // ====================================================================
        // VS (Vertex Shader) Instructions - Phase 1
        // ====================================================================

        // --- VS J-type: NOP, HALT, JUMP, VOUTPUT ---
        case Opcode::VS_NOP:
            // NOP: No operation, just advance PC
            pc_.addr += 4;
            break;

        case Opcode::VS_HALT:
            // HALT: Stop execution
            pc_.addr += 4;
            return;  // Exit early to avoid double-incrementing cycles (drainPendingDIVs already advanced)

        case Opcode::VS_JUMP: {
            // JUMP: PC += offset * 4 (15-bit signed offset in bits[24:10])
            int16_t offset = inst.GetSignedJumpOffset();
            pc_.addr += static_cast<uint32_t>(offset * 4);
            break;
        }

        case Opcode::VS_VOUTPUT: {
            // VOUTPUT Rd, #offset: Write clip coords to VOUTPUTBUF
            // Rd must be 4-aligned (x,y,z,w)
            // Note: offset field in instruction encoding is intentionally ignored.
            // Design decision: vertex index is determined by m_vertex_count (sequential),
            // not by the offset field. The offset parameter in vs_voutput() encoding
            // is reserved for future scatter/gather modes or explicit vertex indexing.
            (void)inst;  // offset not used (vertex index from counter)
            float x = reg_file_.Read(rd);
            float y = reg_file_.Read(rd + 1);
            float z = reg_file_.Read(rd + 2);
            float w = reg_file_.Read(rd + 3);

            size_t base = m_vertex_count * 4;
            if (base + 3 < m_voutput_buf.size()) {
                m_voutput_buf[base + 0] = x;
                m_voutput_buf[base + 1] = y;
                m_voutput_buf[base + 2] = z;
                m_voutput_buf[base + 3] = w;
            }
            m_vertex_count++;
            pc_.addr += 4;
            break;
        }

        // --- VS R-type: ADD, SUB, MUL, DIV, DOT3, NORMALIZE ---
        case Opcode::VS_ADD:
            reg_file_.Write(rd, val_a + val_b);
            pc_.addr += 4;
            break;

        case Opcode::VS_SUB:
            reg_file_.Write(rd, val_a - val_b);
            pc_.addr += 4;
            break;

        case Opcode::VS_MUL:
            reg_file_.Write(rd, val_a * val_b);
            pc_.addr += 4;
            break;

        case Opcode::VS_DIV: {
            // VS_DIV: 7-cycle latency via PendingDiv queue
            float result = (val_b != 0.0f) ? (val_a / val_b) : std::numeric_limits<float>::infinity();
            PendingDiv pending;
            pending.rd = rd;
            pending.result = result;
            pending.completion_cycle = stats_.cycles + DIV_LATENCY;
            m_pending_divs.push_back(pending);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_DOT3: {
            // VS_DOT3: Rd = dot(Ra.xyz, Rb.xyz), Ra and Rb must be 4-aligned
            float v0 = reg_file_.Read(ra);
            float v1 = reg_file_.Read(ra + 1);
            float v2 = reg_file_.Read(ra + 2);
            float r0 = reg_file_.Read(rb);
            float r1 = reg_file_.Read(rb + 1);
            float r2 = reg_file_.Read(rb + 2);
            float result = v0 * r0 + v1 * r1 + v2 * r2;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_NORMALIZE: {
            // VS_NORMALIZE: 5-cycle (DOT3 + RSQ + MUL×3)
            // Reads Ra.xyz (Ra must be 4-aligned), writes Rd.xyz
            // W component of Rd is intentionally left unchanged (no write to Rd+3)
            float x = reg_file_.Read(ra);
            float y = reg_file_.Read(ra + 1);
            float z = reg_file_.Read(ra + 2);
            float sq = x * x + y * y + z * z;
            float inv_len = (sq > 0.0f) ? (1.0f / std::sqrt(sq)) : std::numeric_limits<float>::infinity();
            reg_file_.Write(rd, x * inv_len);
            reg_file_.Write(rd + 1, y * inv_len);
            reg_file_.Write(rd + 2, z * inv_len);
            pc_.addr += 4;
            break;
        }

        // --- VS I-type: VLOAD ---
        case Opcode::VS_VLOAD: {
            // VS_VLOAD Rd, #byte_offset: Load 4 floats from VBO into Rd..Rd+3
            // Ra is hardcoded to R0 (ignored), Rd must be 4-aligned
            uint16_t byte_offset = inst.GetImm();  // 10-bit byte offset
            size_t num_floats = 4;
            for (size_t i = 0; i < num_floats; ++i) {
                size_t vbo_idx = byte_offset / 4 + i;
                float val = (vbo_idx < m_vbo_count) ? m_vbo_data[vbo_idx] : 0.0f;
                reg_file_.Write(rd + static_cast<uint8_t>(i), val);
            }
            stats_.loads++;
            pc_.addr += 4;
            break;
        }

        // --- VS R4-type: MAT_MUL ---
        case Opcode::VS_MAT_MUL: {
            // VS_MAT_MUL: 4×4 matrix multiply (column-major storage)
            // Matrix-vector multiply: r = M * v
            // In column-major storage: m[i*4+j] accesses row i, column j
            // r[i] = Σⱼ m[i*4+j] * v[j]

            // Read 4×4 matrix from Ra (16 floats, column-major)
            float m[16];
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    m[j * 4 + i] = reg_file_.Read(ra + j * 4 + i);
                }
            }

            // Read vector from Rb (Rb, Rb+1, Rb+2, Rb+3)
            float v[4];
            for (int i = 0; i < 4; ++i) {
                v[i] = reg_file_.Read(rb + i);
            }

            // Compute result: r[i] = Σⱼ m[i*4+j] * v[j]
            float result[4];
            for (int i = 0; i < 4; ++i) {
                result[i] = m[i * 4 + 0] * v[0] + m[i * 4 + 1] * v[1] + m[i * 4 + 2] * v[2] + m[i * 4 + 3] * v[3];
            }

            // Write result to Rd..Rd+3
            for (int i = 0; i < 4; ++i) {
                reg_file_.Write(rd + i, result[i]);
            }
            pc_.addr += 4;
            break;
        }

        // ====================================================================
        // VS (Vertex Shader) Instructions - Phase 2
        // ====================================================================

        // --- VS B-type: CBR ---
        case Opcode::VS_CBR: {
            // VS_CBR: if (Ra != 0) PC += offset * 4
            // B-type: ra at bits [24:20], immediate at bits [19:10]
            uint8_t ra_field = static_cast<uint8_t>((inst.raw >> 20) & 0x1F);
            uint16_t uoffset = static_cast<uint16_t>((inst.raw >> 10) & 0x3FF);
            int16_t offset;
            if (uoffset & 0x200) {
                offset = static_cast<int16_t>(uoffset | 0xFC00);  // negative
            } else {
                offset = static_cast<int16_t>(uoffset);
            }
            float cond = reg_file_.Read(ra_field);
            if (cond != 0.0f) {
                pc_.addr += static_cast<uint32_t>(offset * 4);
                stats_.branches_taken++;
            } else {
                pc_.addr += 4;
            }
            break;
        }

        // --- VS R4-type: MAD ---
        case Opcode::VS_MAD: {
            // VS_MAD: Rd = Ra * Rb + Rc (Rc in bits [9:5] of immediate field)
            float val_c = reg_file_.Read(inst.GetRc());
            reg_file_.Write(rd, val_a * val_b + val_c);
            pc_.addr += 4;
            break;
        }

        // --- VS U-type SFU: SQRT, RSQ ---
        case Opcode::VS_SQRT:
            reg_file_.Write(rd, (val_a >= 0.0f) ? std::sqrt(val_a) : std::nanf(""));
            pc_.addr += 4;
            break;

        case Opcode::VS_RSQ:
            if (val_a > 0.0f) {
                reg_file_.Write(rd, 1.0f / std::sqrt(val_a));
            } else if (val_a == 0.0f) {
                reg_file_.Write(rd, std::numeric_limits<float>::infinity());
            } else {
                reg_file_.Write(rd, std::nanf(""));
            }
            pc_.addr += 4;
            break;

        // --- VS R-type: CMP, MIN, MAX, SETP ---
        case Opcode::VS_CMP:
            // VS_CMP: Rd = (Ra >= Rb) ? Rb : 0
            reg_file_.Write(rd, (val_a >= val_b) ? val_b : 0.0f);
            pc_.addr += 4;
            break;

        case Opcode::VS_MIN:
            reg_file_.Write(rd, (val_a < val_b) ? val_a : val_b);
            pc_.addr += 4;
            break;

        case Opcode::VS_MAX:
            reg_file_.Write(rd, (val_a > val_b) ? val_a : val_b);
            pc_.addr += 4;
            break;

        case Opcode::VS_SETP: {
            // VS_SETP: stub - set predicate register (not implemented)
            // For now, just write 1.0 to rd if condition is true
            reg_file_.Write(rd, (val_a != 0.0f) ? 1.0f : 0.0f);
            pc_.addr += 4;
            break;
        }

        // --- VS R-type: DOT4, CROSS, LENGTH ---
        case Opcode::VS_DOT4: {
            // VS_DOT4: Rd = dot(Ra.xyzw, Rb.xyzw)
            float ra0 = reg_file_.Read(ra);
            float ra1 = reg_file_.Read(ra + 1);
            float ra2 = reg_file_.Read(ra + 2);
            float ra3 = reg_file_.Read(ra + 3);
            float rb0 = reg_file_.Read(rb);
            float rb1 = reg_file_.Read(rb + 1);
            float rb2 = reg_file_.Read(rb + 2);
            float rb3 = reg_file_.Read(rb + 3);
            float result = ra0 * rb0 + ra1 * rb1 + ra2 * rb2 + ra3 * rb3;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_CROSS: {
            // VS_CROSS: cross(Ra.xyz, Rb.xyz), result writes xyz to Rd
            // Ra and Rb must be 4-aligned
            // W component of Rd is intentionally left unchanged (no write to Rd+3)
            float ax = reg_file_.Read(ra);
            float ay = reg_file_.Read(ra + 1);
            float az = reg_file_.Read(ra + 2);
            float bx = reg_file_.Read(rb);
            float by = reg_file_.Read(rb + 1);
            float bz = reg_file_.Read(rb + 2);
            reg_file_.Write(rd, ay * bz - az * by);      // x
            reg_file_.Write(rd + 1, az * bx - ax * bz);  // y
            reg_file_.Write(rd + 2, ax * by - ay * bx);   // z
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_LENGTH: {
            // VS_LENGTH: sqrt(dot(Ra.xyz, Ra.xyz)), Ra must be 4-aligned
            float x = reg_file_.Read(ra);
            float y = reg_file_.Read(ra + 1);
            float z = reg_file_.Read(ra + 2);
            float result = std::sqrt(x * x + y * y + z * z);
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        // --- VS R4-type: MAT_ADD, MAT_TRANSPOSE ---
        case Opcode::VS_MAT_ADD: {
            // VS_MAT_ADD: 4×4 matrix element-wise addition
            // Rd = Ra + Rb (each 16 floats)
            for (int i = 0; i < 16; ++i) {
                float a = reg_file_.Read(ra + i);
                float b = reg_file_.Read(rb + i);
                reg_file_.Write(rd + i, a + b);
            }
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_MAT_TRANSPOSE: {
            // VS_MAT_TRANSPOSE: 4×4 matrix transpose (column-major)
            // Rd[j*4+i] = Ra[i*4+j] (swap row/col)
            // Use temp buffer to avoid in-place overwrite issues
            float tmp[16];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    tmp[j * 4 + i] = reg_file_.Read(ra + i * 4 + j);
                }
            }
            for (int i = 0; i < 16; ++i) {
                reg_file_.Write(rd + i, tmp[i]);
            }
            pc_.addr += 4;
            break;
        }

        // --- VS I-type: ATTR, VSTORE ---
        case Opcode::VS_ATTR: {
            // VS_ATTR: stub - request interpolated attribute from rasterizer
            // Returns (u, v, 0, 1) as a placeholder
            uint16_t attr_id = inst.GetImm();
            (void)attr_id;  // unused in stub
            reg_file_.Write(rd, 0.0f);
            reg_file_.Write(rd + 1, 0.0f);
            reg_file_.Write(rd + 2, 0.0f);
            reg_file_.Write(rd + 3, 1.0f);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_VSTORE: {
            // VS_VSTORE: Write Ra to VBO[Rd + offset]
            uint16_t offset = inst.GetImm();
            uint32_t addr = static_cast<uint32_t>(rd) * 4 + offset;
            if (addr + 4 <= memory_.GetSize()) {
                memory_.Store32(addr, val_a);
            }
            stats_.stores++;
            pc_.addr += 4;
            break;
        }

        // --- VS U-type SFU: SIN, COS, EXPD2, LOGD2 ---
        case Opcode::VS_SIN: {
            // Polynomial approximation of sin(x)
            // Using range reduction: sin(x) = sin(x - 2π*n)
            float x = val_a;
            // Simple Taylor-ish approximation
            float result = std::sin(x);
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_COS: {
            float result = std::cos(val_a);
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_EXPD2: {
            // EXPD2: 2^val_a
            float result = std::exp2(val_a);
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_LOGD2: {
            // LOGD2: log2(val_a)
            float result = (val_a > 0.0f) ? std::log2(val_a) : std::nanf("");
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_POW: {
            // POW: val_a ^ val_b
            float result = std::pow(val_a, val_b);
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        // --- VS R-type: AND, OR, XOR, SHL, SHR ---
        case Opcode::VS_AND: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            uint32_t ib = *reinterpret_cast<uint32_t*>(&val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia & ib;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_OR: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            uint32_t ib = *reinterpret_cast<uint32_t*>(&val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia | ib;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_XOR: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            uint32_t ib = *reinterpret_cast<uint32_t*>(&val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia ^ ib;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_SHL: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            int shift = static_cast<int>(val_b) & 31;  // clamp to [0,31]
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia << shift;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_SHR: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            int shift = static_cast<int>(val_b) & 31;  // clamp to [0,31]
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia >> shift;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        // --- VS U-type: NOT ---
        case Opcode::VS_NOT: {
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ~ia;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }

        // --- VS U-type: CVT_* ---
        case Opcode::VS_CVT_F32_S32: {
            // Float to signed int: convert float value to int32 bits, then back to float
            int32_t i = static_cast<int32_t>(val_a);
            reg_file_.Write(rd, *reinterpret_cast<float*>(&i));
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_CVT_F32_U32: {
            // Float to unsigned int: convert float value to uint32 bits, then back to float
            uint32_t u = static_cast<uint32_t>(val_a);
            reg_file_.Write(rd, *reinterpret_cast<float*>(&u));
            pc_.addr += 4;
            break;
        }

        case Opcode::VS_CVT_S32_F32: {
            // Signed int to float: read int bits as float bits, then convert to float value
            int32_t i = *reinterpret_cast<int32_t*>(&val_a);
            reg_file_.Write(rd, static_cast<float>(i));
            pc_.addr += 4;
            break;
        }

        // --- VS U-type: MOV, MOV_IMM ---
        case Opcode::VS_MOV:
            reg_file_.Write(rd, val_a);
            pc_.addr += 4;
            break;

        case Opcode::VS_MOV_IMM: {
            // VS_MOV_IMM: Rd = sign_extended_16bit(imm16)
            int16_t imm = inst.GetSignedImm16();
            reg_file_.Write(rd, static_cast<float>(imm));
            pc_.addr += 4;
            break;
        }

        default:
            pc_.addr += 4;
            break;
        }
    }

    // Get register file for inspection
    const RegisterFile& GetRegisterFile() const { return reg_file_; }
    
    // Reset interpreter
    void Reset()
    {
        reg_file_.Reset();
        pc_.addr = 0;
        pc_.link = 0;
        stats_.Reset();
        m_pending_divs.clear();  // P0-2: Clear pending DIVs on reset
        ResetVS();
    }
    
    // Dump state for debugging
    std::string DumpState() const
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "PC=%08X  Cycles=%llu  Insts=%llu  Loads=%llu  Stores=%llu\n"
            "R0=%.6f R1=%.6f R2=%.6f R3=%.6f R4=%.6f R5=%.6f R6=%.6f R7=%.6f",
            pc_.addr,
            (unsigned long long)stats_.cycles,
            (unsigned long long)stats_.instructions_executed,
            (unsigned long long)stats_.loads,
            (unsigned long long)stats_.stores,
            reg_file_.Read(0), reg_file_.Read(1), reg_file_.Read(2),
            reg_file_.Read(3), reg_file_.Read(4), reg_file_.Read(5),
            reg_file_.Read(6), reg_file_.Read(7));
        return std::string(buf);
    }
};

} // namespace isa
} // namespace softgpu
