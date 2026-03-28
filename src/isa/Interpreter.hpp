#pragma once

#include "Opcode.hpp"
#include "Instruction.hpp"
#include "Decoder.hpp"
#include "RegisterFile.hpp"
#include "ExecutionUnits.hpp"
#include <cstdint>
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
    Decoder decoder_;
    ExecutionUnitPool exec_units_;
    PC pc_;
    Stats stats_;
    
    // Memory reference
    Memory memory_;

public:
    Interpreter() : memory_(1024 * 1024) {}
    
    // Initialize with program (code as vector of 32-bit words)
    void LoadProgram(const uint32_t* code, size_t word_count, uint32_t start_addr = 0)
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

    // Single step execution
    bool Step()
    {
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
            float result = (val_b != 0.0f) ? (val_a / val_b) : std::numeric_limits<float>::infinity();
            reg_file_.Write(rd, result);
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
            uint32_t addr = static_cast<uint32_t>(val_a) + offset;
            float value = memory_.Load32(addr);
            reg_file_.Write(rd, value);
            stats_.loads++;
            pc_.addr += 4;
            break;
        }
        
        case Opcode::ST: {
            uint16_t offset = inst.GetImm();
            uint32_t addr = static_cast<uint32_t>(val_a) + offset;
            memory_.Store32(addr, val_b);
            stats_.stores++;
            pc_.addr += 4;
            break;
        }
        
        // === Special Operations (Simplified) ===
        case Opcode::TEX:
        case Opcode::SAMPLE:
        case Opcode::LDC:
        case Opcode::BAR:
            // Stub implementations for v1.0
            pc_.addr += 4;
            break;
        
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
    }
    
    // Dump state for debugging
    std::string DumpState() const
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
            "PC=%08X  Cycles=%lu  Insts=%lu  Loads=%lu  Stores=%lu\n"
            "R0=%.6f R1=%.6f R2=%.6f R3=%.6f R4=%.6f R5=%.6f R6=%.6f R7=%.6f",
            pc_.addr, stats_.cycles, stats_.instructions_executed,
            stats_.loads, stats_.stores,
            reg_file_.Read(0), reg_file_.Read(1), reg_file_.Read(2),
            reg_file_.Read(3), reg_file_.Read(4), reg_file_.Read(5),
            reg_file_.Read(6), reg_file_.Read(7));
        return std::string(buf);
    }
};

} // namespace isa
} // namespace softgpu
