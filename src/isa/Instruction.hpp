#pragma once

#include "Opcode.hpp"
#include <cstdint>
#include <string>

namespace softgpu {
namespace isa {

// ============================================================================
// Instruction Structure - 32-bit fixed length
// ============================================================================
//
// Layout:
//  31    25 24    20 19    15 14    10 9              0
// +--------+-------+-------+-------+------------------+
// | Opcode |  Rd   |  Ra   |  Rb   |   Immediate      |
// | 7 bit  | 5 bit | 5 bit | 5 bit |     10 bit       |
// +--------+-------+-------+-------+------------------+
//
// Registers: R0-R63 (5-bit field)
// R0 is hardcoded to 0.0f (zero register)
// ============================================================================

struct Instruction
{
    // Raw 32-bit encoding
    uint32_t raw;

    // Constructor from raw 32-bit value
    explicit Instruction(uint32_t raw = 0) : raw(raw) {}

    // --- Field accessors ---
    
    // Opcode: bits[31:25]
    Opcode GetOpcode() const
    {
        return static_cast<Opcode>((raw >> 25) & 0x7F);
    }

    // Rd: bits[24:20] - destination register
    uint8_t GetRd() const { return (raw >> 20) & 0x1F; }

    // Ra: bits[19:15] - source register A
    uint8_t GetRa() const { return (raw >> 15) & 0x1F; }

    // Rb: bits[14:10] - source register B
    uint8_t GetRb() const { return (raw >> 10) & 0x1F; }

    // Immediate: bits[9:0] - 10-bit immediate
    uint16_t GetImm() const { return raw & 0x3FF; }

    // Rc: for R4 type, encoded in bits [9:5] of the instruction (see MakeR4)
    // MAD: Rc = Ra field of extra word
    // SEL: Rc = Rb field of extra word
    // TEX/SAMPLE: texture/uv registers
    uint8_t GetRc() const { return (raw >> 5) & 0x1F; }

    // --- Signed immediate (for BRA) ---
    int16_t GetSignedImm() const
    {
        uint16_t uimm = GetImm();
        // Sign extend 10-bit to 16-bit
        if (uimm & 0x200) { // negative (bit 9 set)
            return static_cast<int16_t>(uimm | 0xFC00);
        }
        return static_cast<int16_t>(uimm);
    }

    // --- Signed 16-bit immediate (for VS_MOV_IMM) ---
    // Reads raw lower 16 bits directly, sign-extends to 32-bit
    int16_t GetSignedImm16() const
    {
        uint16_t uimm = raw & 0xFFFF;
        // Sign extend 16-bit to 16-bit (already 16-bit, just cast)
        return static_cast<int16_t>(uimm);
    }

    // --- Signed 15-bit jump offset (for VS_JUMP) ---
    // VS_JUMP encoding: bits[24:20] (5 bits) + bits[9:0] (10 bits) = 15-bit signed offset
    // offset is in 4-byte units (PC += offset * 4)
    int16_t GetSignedJumpOffset() const
    {
        uint16_t upper = static_cast<uint16_t>((raw >> 20) & 0x1F);  // bits[24:20]
        uint16_t lower = static_cast<uint16_t>(raw & 0x3FF);          // bits[9:0]
        uint16_t offset15 = (upper << 10) | lower;                     // combine into 15 bits
        // Sign extend 15-bit to 16-bit
        if (offset15 & 0x4000) {  // bit 14 is sign bit
            return static_cast<int16_t>(offset15 | 0x8000);
        }
        return static_cast<int16_t>(offset15);
    }

    // --- Factory methods for each instruction type ---

    // NOP: no operands
    static Instruction MakeNOP()
    {
        return Instruction(static_cast<uint32_t>(Opcode::NOP) << 25);
    }

    // R-type: ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX
    static Instruction MakeR(Opcode op, uint8_t rd, uint8_t ra, uint8_t rb)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (static_cast<uint32_t>(rd & 0x1F) << 20)
                           | (static_cast<uint32_t>(ra & 0x1F) << 15)
                           | (static_cast<uint32_t>(rb & 0x1F) << 10);
        return Instruction(encoding);
    }

    // R4-type: MAD, SEL, TEX, SAMPLE
    static Instruction MakeR4(Opcode op, uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (static_cast<uint32_t>(rd & 0x1F) << 20)
                           | (static_cast<uint32_t>(ra & 0x1F) << 15)
                           | (static_cast<uint32_t>(rb & 0x1F) << 10)
                           | (static_cast<uint32_t>(rc & 0x1F) << 5);
        return Instruction(encoding);
    }

    // U-type: RCP, SQRT, RSQ, MOV, F2I, I2F, FRACT, LDC
    static Instruction MakeU(Opcode op, uint8_t rd, uint8_t ra, uint16_t imm = 0)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (static_cast<uint32_t>(rd & 0x1F) << 20)
                           | (static_cast<uint32_t>(ra & 0x1F) << 15)
                           | (imm & 0x3FF);
        return Instruction(encoding);
    }

    // I-type: LD, ST
    static Instruction MakeI(Opcode op, uint8_t rd, uint8_t ra, uint16_t offset)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (static_cast<uint32_t>(rd & 0x1F) << 20)
                           | (static_cast<uint32_t>(ra & 0x1F) << 15)
                           | (offset & 0x3FF);
        return Instruction(encoding);
    }

    // B-type: BRA
    static Instruction MakeB(uint8_t rc, int16_t offset)
    {
        uint32_t encoding = (static_cast<uint32_t>(Opcode::BRA) << 25)
                           | (static_cast<uint32_t>(rc & 0x1F) << 20)
                           | (static_cast<uint32_t>(offset & 0x3FF) << 10);
        return Instruction(encoding);
    }

    // J-type: JMP, CALL
    static Instruction MakeJ(Opcode op, uint32_t target)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (target & 0x1FFFFF);
        return Instruction(encoding);
    }

    // VS MOV_IMM type: opcode + rd + 16-bit signed immediate at bits [15:0]
    // Used for VS_MOV_IMM which carries a full 16-bit immediate
    static Instruction MakeMovImm(Opcode op, uint8_t rd, int16_t imm)
    {
        uint32_t encoding = (static_cast<uint32_t>(op) << 25)
                           | (static_cast<uint32_t>(rd & 0x1F) << 20)
                           | (static_cast<uint32_t>(static_cast<uint16_t>(imm)));
        return Instruction(encoding);
    }

    // --- Utility ---
    
    // Disassemble to string (for debugging)
    std::string Disassemble() const;

    // Get instruction type
    IType GetType() const { return GetInstructionType(GetOpcode()); }

    // Check if this is a valid instruction
    bool IsValid() const
    {
        Opcode op = GetOpcode();
        return op != Opcode::INVALID;
    }
};

// ============================================================================
// Common instruction patterns
// ============================================================================

namespace Pattern {

// Helper to create common R-type instructions
inline Instruction add(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::ADD, rd, ra, rb); }
inline Instruction sub(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::SUB, rd, ra, rb); }
inline Instruction mul(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::MUL, rd, ra, rb); }
inline Instruction div_(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::DIV, rd, ra, rb); }
inline Instruction and_(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::AND, rd, ra, rb); }
inline Instruction or_(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::OR, rd, ra, rb); }
inline Instruction cmp(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::CMP, rd, ra, rb); }
inline Instruction min_(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::MIN, rd, ra, rb); }
inline Instruction max_(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::MAX, rd, ra, rb); }

// R4-type
inline Instruction mad(uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc) { return Instruction::MakeR4(Opcode::MAD, rd, ra, rb, rc); }
inline Instruction sel(uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc) { return Instruction::MakeR4(Opcode::SEL, rd, ra, rb, rc); }

// U-type
inline Instruction rcp(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::RCP, rd, ra); }
inline Instruction sqrt(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::SQRT, rd, ra); }
inline Instruction rsq(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::RSQ, rd, ra); }
inline Instruction mov(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::MOV, rd, ra); }
inline Instruction f2i(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::F2I, rd, ra); }
inline Instruction i2f(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::I2F, rd, ra); }
inline Instruction fract(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::FRACT, rd, ra); }
inline Instruction ldc(uint8_t rd, uint8_t cbuf, uint16_t offset) { return Instruction::MakeU(Opcode::LDC, rd, cbuf, offset); }

// I-type
inline Instruction ld(uint8_t rd, uint8_t ra, uint16_t offset) { return Instruction::MakeI(Opcode::LD, rd, ra, offset); }
inline Instruction st(uint8_t ra, uint16_t offset, uint8_t rb) { return Instruction::MakeI(Opcode::ST, rb, ra, offset); } // Note: ST uses [Ra+imm] = Rb

// B-type
inline Instruction bra(uint8_t rc, int16_t offset) { return Instruction::MakeB(rc, offset); }

// J-type
inline Instruction jmp(uint32_t target) { return Instruction::MakeJ(Opcode::JMP, target); }
inline Instruction call(uint32_t target) { return Instruction::MakeJ(Opcode::CALL, target); }

} // namespace Pattern

// ============================================================================
// VS (Vertex Shader) instruction patterns
// ============================================================================
namespace PatternVS {

// VS J-type: NOP, HALT, JUMP, VOUTPUT
inline Instruction vs_nop() { return Instruction::MakeJ(Opcode::VS_NOP, 0); }
inline Instruction vs_halt() { return Instruction::MakeJ(Opcode::VS_HALT, 0); }
inline Instruction vs_jump(int16_t offset) {
    // VS_JUMP encoding: opcode(7) | offset_hi(5) | rd(5) | ra(5) | offset_lo(10)
    // offset is 15-bit signed, split into bits[24:20] (upper 5) and bits[9:0] (lower 10)
    uint16_t off = static_cast<uint16_t>(offset & 0x7FFF);  // 15-bit
    uint8_t upper = static_cast<uint8_t>((off >> 10) & 0x1F);
    uint16_t lower = off & 0x3FF;
    uint32_t encoding = (static_cast<uint32_t>(Opcode::VS_JUMP) << 25)
                       | (static_cast<uint32_t>(upper) << 20)
                       | lower;
    return Instruction(encoding);
}

// VS R-type: ADD, SUB, MUL, DIV, DOT3, DOT4, CROSS, LENGTH, CMP, MIN, MAX, AND, OR, XOR, SHL, SHR, POW
inline Instruction vs_add(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_ADD, rd, ra, rb); }
inline Instruction vs_sub(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_SUB, rd, ra, rb); }
inline Instruction vs_mul(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_MUL, rd, ra, rb); }
inline Instruction vs_div(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_DIV, rd, ra, rb); }
inline Instruction vs_dot3(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_DOT3, rd, ra, rb); }
inline Instruction vs_dot4(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_DOT4, rd, ra, rb); }
inline Instruction vs_cross(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_CROSS, rd, ra, rb); }
inline Instruction vs_length(uint8_t rd, uint8_t ra) { return Instruction::MakeR(Opcode::VS_LENGTH, rd, ra, 0); }
inline Instruction vs_cmp(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_CMP, rd, ra, rb); }
inline Instruction vs_min(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_MIN, rd, ra, rb); }
inline Instruction vs_max(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_MAX, rd, ra, rb); }
inline Instruction vs_and(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_AND, rd, ra, rb); }
inline Instruction vs_or(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_OR, rd, ra, rb); }
inline Instruction vs_xor(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_XOR, rd, ra, rb); }
inline Instruction vs_shl(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_SHL, rd, ra, rb); }
inline Instruction vs_shr(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_SHR, rd, ra, rb); }
inline Instruction vs_pow(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_POW, rd, ra, rb); }

// VS R4-type: MAD, MAT_MUL, MAT_ADD, MAT_TRANSPOSE
inline Instruction vs_mad(uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc) { return Instruction::MakeR4(Opcode::VS_MAD, rd, ra, rb, rc); }
inline Instruction vs_mat_mul(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR4(Opcode::VS_MAT_MUL, rd, ra, rb, 0); }
inline Instruction vs_mat_add(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR4(Opcode::VS_MAT_ADD, rd, ra, rb, 0); }
inline Instruction vs_mat_transpose(uint8_t rd, uint8_t ra) { return Instruction::MakeR4(Opcode::VS_MAT_TRANSPOSE, rd, ra, 0, 0); }

// VS U-type: SQRT, RSQ, SIN, COS, EXPD2, LOGD2, NOT, MOV, MOV_IMM, CVT_*
inline Instruction vs_sqrt(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_SQRT, rd, ra); }
inline Instruction vs_rsq(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_RSQ, rd, ra); }
inline Instruction vs_sin(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_SIN, rd, ra); }
inline Instruction vs_cos(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_COS, rd, ra); }
inline Instruction vs_expd2(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_EXPD2, rd, ra); }
inline Instruction vs_logd2(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_LOGD2, rd, ra); }
inline Instruction vs_not(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_NOT, rd, ra); }
inline Instruction vs_mov(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_MOV, rd, ra); }
inline Instruction vs_mov_imm(uint8_t rd, int16_t imm) { return Instruction::MakeMovImm(Opcode::VS_MOV_IMM, rd, imm); }
inline Instruction vs_cvt_f32_s32(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_CVT_F32_S32, rd, ra); }
inline Instruction vs_cvt_f32_u32(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_CVT_F32_U32, rd, ra); }
inline Instruction vs_cvt_s32_f32(uint8_t rd, uint8_t ra) { return Instruction::MakeU(Opcode::VS_CVT_S32_F32, rd, ra); }

// VS I-type: VLOAD, VSTORE, ATTR
inline Instruction vs_vload(uint8_t rd, uint16_t byte_offset) { return Instruction::MakeI(Opcode::VS_VLOAD, rd, 0, byte_offset); }
inline Instruction vs_vstore(uint8_t rd, uint16_t byte_offset, uint8_t ra) { return Instruction::MakeI(Opcode::VS_VSTORE, rd, ra, byte_offset); }
inline Instruction vs_attr(uint8_t rd, uint16_t attr_id) { return Instruction::MakeI(Opcode::VS_ATTR, rd, 0, attr_id); }

// VS B-type: CBR
inline Instruction vs_cbr(uint8_t ra, int16_t offset) {
    uint32_t encoding = (static_cast<uint32_t>(Opcode::VS_CBR) << 25)
                       | (static_cast<uint32_t>(ra & 0x1F) << 20)
                       | (static_cast<uint32_t>(offset & 0x3FF) << 10);
    return Instruction(encoding);
}

// VS NORMALIZE: special R-type (reads 3 consecutive regs from Ra, writes 3 to Rd)
inline Instruction vs_normalize(uint8_t rd, uint8_t ra) { return Instruction::MakeR(Opcode::VS_NORMALIZE, rd, ra, 0); }

// VS VOUTPUT: J-type with offset
inline Instruction vs_voutput(uint8_t rd, uint16_t offset) {
    uint32_t encoding = (static_cast<uint32_t>(Opcode::VS_VOUTPUT) << 25)
                       | (static_cast<uint32_t>(rd & 0x1F) << 20)
                       | (static_cast<uint32_t>(offset));
    return Instruction(encoding);
}

// VS SETP: predicate set (stub)
inline Instruction vs_setp(uint8_t rd, uint8_t ra, uint8_t rb) { return Instruction::MakeR(Opcode::VS_SETP, rd, ra, rb); }

} // namespace PatternVS

} // namespace isa
} // namespace softgpu
