#pragma once

#include <cstdint>
#include <string>

namespace softgpu {
namespace isa {

// ============================================================================
// Opcode Enumeration - 28 Instructions
// ============================================================================

enum class Opcode : uint8_t
{
    // ---- Control Flow (5) ----
    NOP  = 0x00,  // No operation
    JMP  = 0x12,  // Unconditional jump
    BRA  = 0x11,  // Conditional branch
    CALL = 0x13,  // Function call
    RET  = 0x14,  // Function return

    // ---- Arithmetic (8) ----
    ADD  = 0x01,  // Addition
    SUB  = 0x02,  // Subtraction
    MUL  = 0x03,  // Multiplication
    DIV  = 0x04,  // Division
    MAD  = 0x05,  // Multiply-Add
    RCP  = 0x06,  // Reciprocal (1/x)
    SQRT = 0x07,  // Square root
    RSQ  = 0x08,  // Reciprocal square root (1/sqrt(x))

    // ---- Logic & Compare (4) ----
    AND = 0x09,   // Bitwise AND
    OR  = 0x0A,   // Bitwise OR
    CMP = 0x0B,   // Compare (Ra < Rb ? 1.0 : 0.0)
    SEL = 0x0C,   // Select (Rc != 0 ? Ra : Rb)

    // ---- Math (2) ----
    MIN = 0x0D,   // Minimum
    MAX = 0x0E,   // Maximum

    // ---- Memory (2) ----
    LD  = 0x0F,   // Load
    ST  = 0x10,   // Store

    // ---- Data Conversion (4) ----
    MOV   = 0x15,  // Move
    F2I   = 0x16,  // Float to Integer
    I2F   = 0x17,  // Integer to Float
    FRACT = 0x18,  // Fractional part

    // ---- Special (4, P1/v1.x) ----
    TEX    = 0x19,  // Texture sample
    SAMPLE = 0x1A,  // Simplified texture sample
    LDC    = 0x1B,  // Load constant
    BAR    = 0x1C,  // Barrier (warp synchronization)

    // ---- Sentinel ----
    INVALID = 0xFF
};

// Instruction type classification
enum class IType
{
    R,     // 3-register: ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX
    R4,    // 4-register: MAD, SEL, TEX, SAMPLE
    U,     // 1-register: RCP, SQRT, RSQ, MOV, F2I, I2F, FRACT, LDC
    I,     // 2-reg + imm: LD, ST
    B,     // Branch: BRA
    J      // Jump: JMP, CALL, RET, NOP, BAR
};

// Get instruction type from opcode
IType GetInstructionType(Opcode op);

// Get opcode name as string
const char* GetOpcodeName(Opcode op);

// Get cycle count for an instruction
int GetCycles(Opcode op);

// Check if instruction is P0 (must implement for v1.0)
bool IsP0(Opcode op);

} // namespace isa
} // namespace softgpu
