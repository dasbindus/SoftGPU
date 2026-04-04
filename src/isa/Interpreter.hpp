#pragma once

#include "Opcode.hpp"
#include "Instruction.hpp"
#include "RegisterFile.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <functional>

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

public:
    Interpreter() : memory_(1024 * 1024) {}
    
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

    // P0-2: Only drain pending DIVs, don't fetch/decode/execute
    // Called by WarpScheduler each cycle before executing instructions
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
    }

    // Single step execution
    bool Step()
    {
        // P0-2: Complete any pending DIV operations whose latency has elapsed
        uint64_t current_cycle = stats_.cycles;
        for (auto it = m_pending_divs.begin(); it != m_pending_divs.end(); ) {
            if (it->completion_cycle <= current_cycle) {
                reg_file_.Write(it->rd, it->result);
                it = m_pending_divs.erase(it);
            } else {
                ++it;
            }
        }
        
        // Fetch
        uint32_t instruction_word = 0; // Would fetch from I-cache in real impl
        Instruction inst(instruction_word); // Simplified
        
        // Decode
        Opcode op = inst.GetOpcode();
        
        if (op == Opcode::INVALID) {
            return false; // Stop on invalid instruction
        }
        
        // Execute
        ExecuteInstruction(inst);
        
        // Advance cycle count
        stats_.cycles++;
        
        return (op != Opcode::RET); // Continue until RET
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
            // 简化：texture_id=0 使用内置 checkerboard
            // 后续扩展：m_textureBuffers[texture_id]->sampleNearest(u, v)
            // Output RGBA to Rd, Rd+1, Rd+2, Rd+3
            float u = val_a;
            float v = val_b;
            int cx = static_cast<int>(std::floor(u * 8.0f));
            int cy = static_cast<int>(std::floor(v * 8.0f));
            bool is_white = ((cx + cy) % 2) == 0;
            float color = is_white ? 1.0f : 0.0f; // Grayscale checkerboard
            reg_file_.Write(rd, color);     // R
            reg_file_.Write(rd + 1, color);  // G
            reg_file_.Write(rd + 2, color);  // B
            reg_file_.Write(rd + 3, 1.0f);  // A = 1.0 (fully opaque)
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
            int shift = static_cast<int>(val_b);
            float result;
            *reinterpret_cast<uint32_t*>(&result) = ia << shift;
            reg_file_.Write(rd, result);
            pc_.addr += 4;
            break;
        }
        
        case Opcode::SHR: {
            // SHR: Rd = Ra >> Rb (shift right by float bits converted to int)
            uint32_t ia = *reinterpret_cast<uint32_t*>(&val_a);
            int shift = static_cast<int>(val_b);
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
