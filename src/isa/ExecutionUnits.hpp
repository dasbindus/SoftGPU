#pragma once

#include "Instruction.hpp"
#include "RegisterFile.hpp"
#include <cmath>
#include <cstdint>
#include <array>

namespace softgpu {
namespace isa {

// ============================================================================
// Execution Units - ALU, SFU, MEM, CTRL
// ============================================================================
// Each execution unit implements a subset of instructions
// ============================================================================

// Forward declarations
class MemoryInterface;

// ============================================================================
// ALU - Arithmetic Logic Unit
// ============================================================================
// Implements: ADD, SUB, MUL, MAD, AND, OR, CMP, SEL, MIN, MAX, MOV
// Latency: 1 cycle
// ============================================================================

class ALU
{
public:
    struct Input
    {
        Opcode op;
        uint8_t rd, ra, rb, rc; // rc only for R4-type
        float val_a, val_b;
    };

    struct Output
    {
        bool write_back;
        uint8_t rd;
        float result;
    };

    Output Execute(const Input& input) const
    {
        Output output;
        output.write_back = true;
        output.rd = input.rd;

        switch (input.op) {
        // --- Basic Arithmetic ---
        case Opcode::ADD:
            output.result = input.val_a + input.val_b;
            break;
        case Opcode::SUB:
            output.result = input.val_a - input.val_b;
            break;
        case Opcode::MUL:
            output.result = input.val_a * input.val_b;
            break;
        
        // --- MAD: Rd = Ra * Rb + Rc ---
        case Opcode::MAD:
            output.result = input.val_a * input.val_b; // Will add Rc in caller
            output.rd = input.rd;
            break;

        // --- Logic ---
        case Opcode::AND: {
            // Float as uint32_t for bitwise ops
            uint32_t ia = FloatToUint(input.val_a);
            uint32_t ib = FloatToUint(input.val_b);
            output.result = UintToFloat(ia & ib);
            break;
        }
        case Opcode::OR: {
            uint32_t ia = FloatToUint(input.val_a);
            uint32_t ib = FloatToUint(input.val_b);
            output.result = UintToFloat(ia | ib);
            break;
        }

        // --- Compare ---
        case Opcode::CMP:
            output.result = (input.val_a < input.val_b) ? 1.0f : 0.0f;
            break;

        // --- Select: Rd = (Rc != 0) ? Ra : Rb ---
        case Opcode::SEL:
            // Note: Rc is passed in val_b for this implementation
            output.result = (input.val_b != 0.0f) ? input.val_a : 0.0f; // Simplified
            break;

        // --- Min/Max ---
        case Opcode::MIN:
            output.result = (input.val_a < input.val_b) ? input.val_a : input.val_b;
            break;
        case Opcode::MAX:
            output.result = (input.val_a > input.val_b) ? input.val_a : input.val_b;
            break;

        // --- Move ---
        case Opcode::MOV:
            output.result = input.val_a;
            break;

        default:
            output.write_back = false;
            output.result = 0.0f;
            break;
        }

        return output;
    }

    // MAD needs special handling (4 operands)
    static float ExecuteMAD(float a, float b, float c)
    {
        return a * b + c;
    }

    // SEL needs special handling
    static float ExecuteSEL(float true_val, float false_val, float cond)
    {
        return (cond != 0.0f) ? true_val : false_val;
    }

private:
    static uint32_t FloatToUint(float f) {
        uint32_t u;
        std::memcpy(&u, &f, sizeof(float));
        return u;
    }
    
    static float UintToFloat(uint32_t u) {
        float f;
        std::memcpy(&f, &u, sizeof(float));
        return f;
    }
};

// ============================================================================
// SFU - Special Function Unit
// ============================================================================
// Implements: RCP, SQRT, RSQ, DIV, F2I, I2F, FRACT
// Latency: 2-7 cycles
// ============================================================================

class SFU
{
public:
    struct Input
    {
        Opcode op;
        uint8_t rd, ra;
        float val_a, val_b; // val_b only for DIV
    };

    struct Output
    {
        bool write_back;
        uint8_t rd;
        float result;
    };

    Output Execute(const Input& input) const
    {
        Output output;
        output.write_back = true;
        output.rd = input.rd;

        switch (input.op) {
        case Opcode::RCP:
            output.result = (input.val_a != 0.0f) ? (1.0f / input.val_a) : std::numeric_limits<float>::infinity();
            break;

        case Opcode::SQRT:
            output.result = (input.val_a >= 0.0f) ? std::sqrt(input.val_a) : std::nanf("");
            break;

        case Opcode::RSQ:
            if (input.val_a > 0.0f) {
                output.result = 1.0f / std::sqrt(input.val_a);
            } else if (input.val_a == 0.0f) {
                output.result = std::numeric_limits<float>::infinity();
            } else {
                output.result = std::nanf("");
            }
            break;

        case Opcode::DIV: {
            // Newton-Raphson for division: compute a / b as a * (1/b)
            // Newton iteration: x_{n+1} = x * (2 - b * x), converges to 1/b
            // 2 iterations gives ~1e-6 relative accuracy for float
            if (input.val_b == 0.0f) {
                output.result = std::copysign(std::numeric_limits<float>::infinity(), input.val_a);
            } else if (std::isinf(input.val_a) || std::isnan(input.val_a) || std::isnan(input.val_b)) {
                output.result = std::nanf("");
            } else {
                float b = input.val_b;
                float x = 1.0f / b;  // Initial estimate
                // Newton-Raphson iterations for reciprocal
                x = x * (2.0f - b * x);  // 1st iteration
                x = x * (2.0f - b * x);  // 2nd iteration
                output.result = input.val_a * x;
            }
            break;
        }

        case Opcode::F2I: {
            // Float to int32, round toward zero
            float val = input.val_a;
            // Clamp to int32 range
            if (val > static_cast<float>(INT32_MAX)) val = static_cast<float>(INT32_MAX);
            if (val < static_cast<float>(INT32_MIN)) val = static_cast<float>(INT32_MIN);
            if (std::isnan(val)) val = 0.0f;
            output.result = static_cast<float>(static_cast<int32_t>(val)); // Truncate toward zero
            break;
        }

        case Opcode::I2F:
            output.result = static_cast<float>(static_cast<int32_t>(input.val_a));
            break;

        case Opcode::FRACT:
            output.result = input.val_a - std::floor(input.val_a);
            break;

        default:
            output.write_back = false;
            output.result = 0.0f;
            break;
        }

        return output;
    }
};

// ============================================================================
// MEM - Memory Unit
// ============================================================================
// Implements: LD, ST
// Latency: 4 cycles (cache hit)
// ============================================================================

class MEM
{
public:
    struct Input
    {
        Opcode op;
        uint8_t rd, ra;      // rd for LD, ra=base for both
        uint16_t offset;
        float store_value;   // Value to store for ST
    };

    struct Output
    {
        bool complete;
        uint8_t rd;
        float result;
    };

    Output Execute(const Input& input, const class MemoryInterface& memory) const;

    // For interpreter simulation, we use a simple memory model
    static float Load32(const uint8_t* memory, uint32_t address);
    static void Store32(uint8_t* memory, uint32_t address, float value);
};

// ============================================================================
// CTRL - Control Unit
// ============================================================================
// Implements: BRA, JMP, CALL, RET, NOP
// Latency: 1-3 cycles
// ============================================================================

class CTRL
{
public:
    struct Input
    {
        Opcode op;
        uint8_t rc;          // Condition register for BRA
        int16_t offset;      // For BRA/JMP/CALL
        float cond;          // Condition value for BRA
    };

    struct Output
    {
        bool taken;
        uint32_t target_pc;  // Target PC if taken
        bool is_call;        // For CALL
        bool is_ret;         // For RET
        bool stall;          // Stall for multi-cycle ops
    };

    Output Execute(const Input& input, uint32_t current_pc, uint32_t link_register) const
    {
        Output output;
        output.taken = false;
        output.is_call = false;
        output.is_ret = false;
        output.stall = false;
        output.target_pc = 0;

        switch (input.op) {
        case Opcode::BRA:
            // Branch if Rc != 0
            if (input.cond != 0.0f) {
                output.taken = true;
                output.target_pc = current_pc + (static_cast<int32_t>(input.offset) * 4);
            } else {
                output.target_pc = current_pc + 4;
            }
            break;

        case Opcode::JMP:
            output.taken = true;
            output.stall = true; // JMP takes 2 cycles
            output.target_pc = current_pc + (static_cast<int32_t>(input.offset) * 4);
            break;

        case Opcode::CALL:
            output.taken = true;
            output.is_call = true;
            output.stall = true; // CALL takes 3 cycles
            output.target_pc = current_pc + (static_cast<int32_t>(input.offset) * 4);
            break;

        case Opcode::RET:
            output.taken = true;
            output.is_ret = true;
            output.stall = true; // RET takes 2 cycles
            output.target_pc = link_register;
            break;

        case Opcode::NOP:
            output.target_pc = current_pc + 4;
            break;

        default:
            output.target_pc = current_pc + 4;
            break;
        }

        return output;
    }
};

// ============================================================================
// TEX - Texture Unit (Simplified for v1.x)
// ============================================================================
// Implements: TEX, SAMPLE
// Latency: 4-8 cycles
// ============================================================================

class TEX
{
public:
    struct Input
    {
        Opcode op;
        uint8_t rd;
        uint8_t tex_idx;     // Texture index register
        uint8_t uv_reg;      // UV coordinate register (for TEX)
        float tex_value;     // Simplified: direct texture data
        float uv_u, uv_v;    // UV coordinates
    };

    struct Output
    {
        bool write_back;
        uint8_t rd;
        float result_r, result_g, result_b, result_a;
    };

    Output Execute(const Input& input) const
    {
        Output output;
        output.write_back = true;
        output.rd = input.rd;

        // Simplified texture sampling
        // In a real implementation, this would do bilinear interpolation
        // For now, we just pass through the tex_value
        output.result_r = input.tex_value;
        output.result_g = input.tex_value;
        output.result_b = input.tex_value;
        output.result_a = 1.0f;

        return output;
    }
};

// ============================================================================
// Execution Unit Pool
// ============================================================================
// Manages all execution units and routes instructions
// ============================================================================

class ExecutionUnitPool
{
public:
    ALU   alu;
    SFU   sfu;
    MEM   mem;
    CTRL  ctrl;
    TEX   tex;
};

} // namespace isa
} // namespace softgpu
