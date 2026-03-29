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

} // namespace isa
} // namespace softgpu
