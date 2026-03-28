#pragma once

#include <cstdint>
#include <cstring> // for memset
#include <array>
#include <string>

namespace softgpu {
namespace isa {

// ============================================================================
// Register File - 64 x 32-bit IEEE 754 float
// ============================================================================
//
// Specifications:
//   - 64 registers (R0-R63)
//   - 32-bit IEEE 754 single precision
//   - 2 read ports (for Ra, Rb simultaneous access)
//   - 1 write port (for result writeback)
//   - R0 is hardwired to 0.0f (writes to R0 are discarded)
//   - Read-after-write forwarding supported
// ============================================================================

class RegisterFile
{
public:
    static constexpr size_t NUM_REGISTERS = 64;
    static constexpr uint8_t ZERO_REG = 0; // R0 is zero register

private:
    // Register storage
    std::array<float, NUM_REGISTERS> registers_;
    
    // Pending write value (for forwarding)
    // When writeback happens same cycle as read, this provides the new value
    float pending_write_value_;
    uint8_t pending_write_reg_;
    bool has_pending_write_;

public:
    RegisterFile()
    {
        Reset();
    }

    // Reset all registers to 0.0f
    void Reset()
    {
        registers_.fill(0.0f);
        has_pending_write_ = false;
        pending_write_reg_ = 0xFF;
        pending_write_value_ = 0.0f;
    }

    // Read a register value
    // Read-after-write forwarding: if register was written last cycle, return new value
    float Read(uint8_t reg) const
    {
        if (reg >= NUM_REGISTERS) {
            return 0.0f; // Out of bounds
        }
        
        // R0 is hardwired to 0.0f
        if (reg == ZERO_REG) {
            return 0.0f;
        }

        // Forwarding: return pending write value if available
        if (has_pending_write_ && pending_write_reg_ == reg) {
            return pending_write_value_;
        }

        return registers_[reg];
    }

    // Read two registers simultaneously (for ALU inputs)
    void Read2(uint8_t ra, uint8_t rb, float& val_a, float& val_b) const
    {
        val_a = Read(ra);
        val_b = Read(rb);
    }

    // Write a register value
    // Note: Writes to R0 are silently discarded (R0 = 0 always)
    void Write(uint8_t reg, float value)
    {
        if (reg >= NUM_REGISTERS || reg == ZERO_REG) {
            return; // Out of bounds or zero register
        }
        
        registers_[reg] = value;
    }

    // Write with pending (for pipeline forwarding)
    void WriteWithPending(uint8_t reg, float value)
    {
        if (reg >= NUM_REGISTERS || reg == ZERO_REG) {
            has_pending_write_ = false;
            return;
        }

        pending_write_reg_ = reg;
        pending_write_value_ = value;
        has_pending_write_ = true;
        registers_[reg] = value; // Also write directly for combinational forwarding
    }

    // Clear pending write flag (call at end of cycle)
    void ClearPendingWrite()
    {
        has_pending_write_ = false;
    }

    // Check if we have a pending write
    bool HasPendingWrite() const { return has_pending_write_; }
    uint8_t GetPendingWriteReg() const { return pending_write_reg_; }
    float GetPendingWriteValue() const { return pending_write_value_; }

    // Dump registers for debugging
    std::string Dump() const
    {
        std::string output;
        char line[128];
        
        for (int i = 0; i < 16; ++i) { // Dump first 16 registers as sample
            snprintf(line, sizeof(line), "R%-2d = %12.6f  ", i, registers_[i]);
            output += line;
            if ((i + 1) % 4 == 0) {
                output += "\n";
            }
        }
        
        return output;
    }

    // Full dump
    std::string DumpAll() const
    {
        std::string output;
        char line[128];
        
        for (int i = 0; i < 64; ++i) {
            snprintf(line, sizeof(line), "R%02d = %12.6f  ", i, registers_[i]);
            output += line;
            if ((i + 1) % 4 == 0) {
                output += "\n";
            }
        }
        
        return output;
    }

    // Direct access (for testing)
    const std::array<float, NUM_REGISTERS>& GetRegisters() const { return registers_; }
    std::array<float, NUM_REGISTERS>& GetRegisters() { return registers_; }
};

} // namespace isa
} // namespace softgpu
