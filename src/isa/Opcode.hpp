#pragma once

#include <cstdint>
#include <string>

namespace softgpu {
namespace isa {

// ============================================================================
// Opcode Enumeration - 36 Instructions (PHASE3: +8 new instructions)
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

    // ---- PHASE3: Bitwise & Math Extensions (8) ----
    SHL        = 0x1D,  // Shift left (Rd = Ra << Rb)
    SHR        = 0x1E,  // Shift right (Rd = Ra >> Rb)
    NOT        = 0x1F,  // Bitwise NOT (Rd = ~Ra)
    FLOOR      = 0x20,  // Floor (Rd = floor(Ra))
    CEIL       = 0x21,  // Ceiling (Rd = ceil(Ra))
    ABS        = 0x22,  // Absolute value (Rd = abs(Ra))
    NEG        = 0x23,  // Negate (Rd = -Ra)
    SMOOTHSTEP = 0x24,  // Smoothstep (Rd = smoothstep(Ra, Rb, Rc))
    DP3        = 0x25,  // Dot product of 3-component vectors (Rd = dot(Ra.xyz, Rb.xyz))

    // ---- VS (Vertex Shader) - Phase 1 (12 instructions) ----
    // VS opcodes occupy 0x26-0x2A and 0x30-0x5F (bit5=1, except exact-match exceptions)
    // These share the unified opcode enum; decoder routes by opcode[5] or exact-match
    //
    // VS exact-match exceptions (bit5=0 but route to VS regardless):
    VS_VOUTPUT = 0x26,  // Output to rasterizer buffer (J-type, 2-cycle)
    VS_MAT_MUL = 0x28,  // 4×4 matrix multiply (R4-type, 4-cycle)
    VS_VLOAD   = 0x29,  // Load from VBO into registers (I-type, 2-cycle)
    VS_HALT    = 0x2A,  // Halt vertex shader program (J-type, 1-cycle)
    // VS NOP (0x30) is distinct from FS NOP (0x00); VS uses VS_ prefix to distinguish
    VS_NOP         = 0x30,  // VS-specific NOP — 空操作 (J-type, 1-cycle)
    VS_JUMP        = 0x32,  // Unconditional jump (J-type, 1-cycle)
    // ---- VS Phase 2 (20 instructions) ----
    VS_CBR         = 0x33,  // Conditional branch: if (Ra != 0) PC += offset*4 (B-type, 1-cycle)
    VS_ADD         = 0x34,  // Addition: Rd = Ra + Rb (R-type, 1-cycle)
    VS_SUB         = 0x35,  // Subtraction: Rd = Ra - Rb (R-type, 1-cycle)
    VS_MUL         = 0x36,  // Multiplication: Rd = Ra * Rb (R-type, 1-cycle)
    VS_DIV         = 0x37,  // Division: Rd = Ra / Rb (R-type, 7-cycle, PendingDiv queue)
    VS_MAD         = 0x38,  // Multiply-Add: Rd = Ra * Rb + Rc (R4-type, Rc in imm[9:5], 1-cycle)
    // 0x39 reserved
    VS_SQRT        = 0x3A,  // Square root: Rd = sqrt(Ra) (U-type, 1-cycle, EU_SFU)
    VS_RSQ         = 0x3B,  // Reciprocal square root: Rd = 1/sqrt(Ra) (U-type, 1-cycle, EU_SFU)
    VS_CMP         = 0x3C,  // Compare: Rd = (Ra >= 0) ? Rb : 0 (R-type, 1-cycle)
    VS_MIN         = 0x3D,  // Minimum: Rd = min(Ra, Rb) (R-type, 1-cycle)
    VS_MAX         = 0x3E,  // Maximum: Rd = max(Ra, Rb) (R-type, 1-cycle)
    VS_SETP        = 0x3F,  // Predicate set: sets internal predicate (R-type, 1-cycle, stub)
    VS_DOT3        = 0x40,  // 3-component dot product (R-type, 1-cycle)
    VS_DOT4        = 0x41,  // 4-component dot product: dot(Ra.xyzw, Rb.xyzw) (R-type, 1-cycle)
    VS_CROSS       = 0x42,  // Cross product: cross(Ra.xyz, Rb.xyz), result writes xyz (R-type, 1-cycle)
    VS_LENGTH      = 0x43,  // Vector length: sqrt(dot(Ra.xyz, Ra.xyz)) (R-type, 1-cycle)
    VS_NORMALIZE   = 0x44,  // Vector normalize: DOT3 + RSQ + 3×MUL (R-type, 5-cycle)
    VS_MAT_ADD     = 0x45,  // Matrix addition: Rd = Ra + Rb, 4×4 element-wise (R4-type, 1-cycle)
    VS_MAT_TRANSPOSE = 0x46,  // Matrix transpose: Rd = transpose(Ra) (R4-type, 4-cycle)
    // 0x47 reserved
    VS_ATTR        = 0x48,  // Attribute read: request interpolated attribute (I-type, stub, 1-cycle)
    // 0x49 reserved
    VS_VSTORE      = 0x4A,  // Store to VBO: VBO[Rd+offset] = Ra (I-type, 1-cycle)
    // 0x4B-0x4F reserved
    // ---- Transcendental / SFU (0x50-0x54) ----
    VS_SIN         = 0x50,  // Sine: Rd = sin(Ra) (U-type, 1-cycle, EU_SFU, polynomial)
    VS_COS         = 0x51,  // Cosine: Rd = cos(Ra) (U-type, 1-cycle, EU_SFU, polynomial)
    VS_EXPD2       = 0x52,  // Base-2 exponential: Rd = 2^Ra (U-type, 1-cycle, EU_SFU)
    VS_LOGD2       = 0x53,  // Base-2 logarithm: Rd = log2(Ra) (U-type, 1-cycle, EU_SFU)
    VS_POW         = 0x54,  // Power: Rd = Ra ^ Rb (R-type, 1-cycle, EU_SFU)
    // 0x55-0x57 reserved
    // ---- Logical Bitwise (0x58-0x5D) ----
    VS_AND         = 0x58,  // Bitwise AND: Rd = Ra & Rb (R-type, 1-cycle)
    VS_OR          = 0x59,  // Bitwise OR: Rd = Ra | Rb (R-type, 1-cycle)
    VS_XOR         = 0x5A,  // Bitwise XOR: Rd = Ra ^ Rb (R-type, 1-cycle)
    VS_NOT         = 0x5B,  // Bitwise NOT: Rd = ~Ra (U-type, 1-cycle)
    VS_SHL         = 0x5C,  // Shift left: Rd = Ra << Rb (R-type, 1-cycle)
    VS_SHR         = 0x5D,  // Shift right: Rd = Ra >> Rb (R-type, 1-cycle)
    // 0x5E-0x5F reserved
    // ---- Type Conversion (0x60-0x62) ----
    VS_CVT_F32_S32 = 0x60,  // Convert float->sint: Rd = (int32_t)Ra (U-type, 1-cycle)
    VS_CVT_F32_U32 = 0x61,  // Convert float->uint: Rd = (uint32_t)Ra (U-type, 1-cycle)
    VS_CVT_S32_F32 = 0x62,  // Convert sint->float: Rd = (float)(int32_t)Ra (U-type, 1-cycle)
    // ---- General Register (0x63-0x64) ----
    VS_MOV         = 0x63,  // Move: Rd = Ra (U-type, 1-cycle)
    VS_MOV_IMM     = 0x64,  // Move immediate: Rd = sign_ext(imm) (U-type, 1-cycle)

    // ---- Sentinel ----
    INVALID = 0xFF
};

// Instruction type classification
enum class IType
{
    R,     // 3-register: ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX, DOT3, DOT4, CROSS, LENGTH, POW, SHL, SHR, etc.
    R4,    // 4-register: MAD, SEL, TEX, SAMPLE, SMOOTHSTEP, MAT_MUL, MAT_ADD, MAT_TRANSPOSE
    U,     // 1-register: RCP, SQRT, RSQ, MOV, MOV_IMM, F2I, I2F, FRACT, LDC, NOT, FLOOR, CEIL, ABS, NEG, SHL, SHR, SIN, COS, EXPD2, LOGD2, CVT_*, VS_ATTR
    I,     // 2-reg + imm: LD, ST, VLOAD, VSTORE, ATTR
    B,     // Branch: BRA, CBR (VS CBR uses same format)
    J,     // Jump: JMP, CALL, RET, NOP, BAR, HALT, VOUTPUT, VS_VOUTPUT
    VS_J,  // VS-specific J-type: NOP, HALT, JUMP, VOUTPUT (uses VS opcode space)
    VS_R,  // VS-specific R-type: ADD, SUB, MUL, DIV, DOT3, NORMALIZE, DOT4, CROSS, LENGTH, CMP, MIN, MAX, SETP, AND, OR, XOR, SHL, SHR, POW
    VS_I,  // VS-specific I-type: VLOAD, VSTORE, ATTR
    VS_R4  // VS-specific R4-type: MAT_MUL, MAD, MAT_ADD, MAT_TRANSPOSE
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
