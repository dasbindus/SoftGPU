#pragma once

// ============================================================================
// SoftGPU ISA v2.5 - Register File
// ============================================================================
// 128 x 32-bit IEEE-754 float registers (R0-R127)
// R0 is hardwired to 0.0f (zero register, read-only)
// ============================================================================

#include <cstdint>
#include <array>
#include <string>

namespace softgpu { namespace isa { namespace v2_5 {

class RegisterFile {
public:
    static constexpr size_t NUM_REGISTERS = 128;
    static constexpr uint8_t ZERO_REG = 0;

    RegisterFile() { Reset(); }
    void Reset() { registers_.fill(0.0f); }

    float Read(uint8_t reg) const {
        if (reg == ZERO_REG) return 0.0f;
        if (reg >= NUM_REGISTERS) return 0.0f;
        return registers_[reg];
    }

    void Read2(uint8_t ra, uint8_t rb, float& va, float& vb) const {
        va = Read(ra);
        vb = Read(rb);
    }

    void Read3(uint8_t ra, uint8_t rb, uint8_t rc, float& va, float& vb, float& vc) const {
        va = Read(ra);
        vb = Read(rb);
        vc = Read(rc);
    }

    void Write(uint8_t reg, float value) {
        if (reg == ZERO_REG || reg >= NUM_REGISTERS) return;
        registers_[reg] = value;
    }

    const std::array<float, NUM_REGISTERS>& GetRegisters() const { return registers_; }
    std::array<float, NUM_REGISTERS>& GetRegisters() { return registers_; }

    std::string Dump(int first = 0, int count = 16) const {
        std::string out;
        char buf[128];
        int last = (first + count > NUM_REGISTERS) ? NUM_REGISTERS : first + count;
        for (int i = first; i < last; i += 4) {
            int end = (i + 4 > last) ? last : i + 4;
            snprintf(buf, sizeof(buf),
                "R%02d=%.4f  R%02d=%.4f  R%02d=%.4f  R%02d=%.4f\n",
                i,   registers_[i],
                i+1, (i+1 < NUM_REGISTERS) ? registers_[i+1] : 0.0f,
                i+2, (i+2 < NUM_REGISTERS) ? registers_[i+2] : 0.0f,
                i+3, (i+3 < NUM_REGISTERS) ? registers_[i+3] : 0.0f);
            out += buf;
        }
        return out;
    }

private:
    std::array<float, NUM_REGISTERS> registers_;
};

}}} // namespaces
