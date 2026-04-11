#pragma once

// ============================================================================
// SoftGPU ISA v2.5 - Opcode Definitions
// ============================================================================
// Unified opcode space: 0x00 - 0xFF (256 values)

#include <cstdint>
#include <string>

namespace softgpu { namespace isa { namespace v2_5 {

// ============================================================================
// Opcode
// ============================================================================
enum class Opcode : uint8_t {
    // ---- Control Flow (0x00-0x0F)
    NOP = 0x00,
    BRA = 0x01,
    CALL = 0x02,
    RET = 0x03,
    JMP = 0x04,
    BAR = 0x05,
    HALT = 0x0F,

    // ---- ALU (0x10-0x1F)
    ADD = 0x10,
    SUB = 0x11,
    MUL = 0x12,
    DIV = 0x13,
    MAD = 0x14,
    CMP = 0x15,
    MIN = 0x16,
    MAX = 0x17,
    AND = 0x18,
    OR = 0x19,
    XOR = 0x1A,
    SHL = 0x1B,
    SHR = 0x1C,
    SEL = 0x1D,
    SMOOTHSTEP = 0x1E,
    SETP = 0x1F,

    // ---- SFU (0x20-0x2F)
    RCP = 0x20,
    SQRT = 0x21,
    RSQ = 0x22,
    SIN = 0x23,
    COS = 0x24,
    EXPD2 = 0x25,
    LOGD2 = 0x26,
    POW = 0x27,
    ABS = 0x28,
    NEG = 0x29,
    FLOOR = 0x2A,
    CEIL = 0x2B,
    FRACT = 0x2C,
    F2I = 0x2D,
    I2F = 0x2E,
    NOT = 0x2F,

    // ---- Memory (0x30-0x3F)
    LD = 0x30,
    ST = 0x31,
    TEX = 0x32,
    SAMPLE = 0x33,
    OUTPUT = 0x34,

    // ---- VS-Only (0x40-0x5F)
    MOV_IMM = 0x48,
    MOV = 0x63,
    VLOAD = 0x49,
    VSTORE = 0x4A,
    OUTPUT_VS = 0x4B,
    LDC = 0x4C,
    ATTR = 0x4D,
    DOT3 = 0x4E,
    DOT4 = 0x4F,

    DEBUG_BREAK = 0xC0,
    INVALID = 0xFF,
};

// ============================================================================
// Instruction Format
// ============================================================================
enum class Format { A, B, C, D, E };

// ============================================================================
// Instruction
// ============================================================================
struct Instruction {
    uint32_t word1 = 0;
    uint32_t word2 = 0;
    bool is_dual_word = false;

    Instruction() {}
    Instruction(uint32_t w1) : word1(w1) {}
    Instruction(uint32_t w1, uint32_t w2) : word1(w1), word2(w2), is_dual_word(true) {}

    Opcode GetOpcode() const { return static_cast<Opcode>((word1 >> 24) & 0xFF); }
    uint8_t GetRd() const { return (word1 >> 17) & 0x7F; }
    uint8_t GetRa() const { return (word1 >> 10) & 0x7F; }
    uint8_t GetRb() const { return (word1 >> 3) & 0x7F; }
    // R4-type: Rc packed at bits [9:5] (same as old ISA)
    uint8_t GetRc() const { return (word1 >> 5) & 0x1F; }
    uint8_t GetRb_W2() const { return (word2 >> 12) & 0x7F; }
    uint16_t GetImm10() const { return word2 & 0x3FF; }

    int32_t GetSignedImm10() const {
        uint16_t u = word2 & 0x3FF;
        if (u & 0x200) return static_cast<int32_t>(u) - 1024;
        return static_cast<int32_t>(u);
    }

    uint16_t GetFunc() const { return (word1 >> 8) & 0x1FF; }

    static Format GetFormat(Opcode op) {
        switch (op) {
            case Opcode::NOP:
            case Opcode::RET:
            case Opcode::HALT:
            case Opcode::BAR:
                return Format::D;
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::MUL:
            case Opcode::DIV:
            case Opcode::MAD:
            case Opcode::CMP:
            case Opcode::MIN:
            case Opcode::MAX:
            case Opcode::AND:
            case Opcode::OR:
            case Opcode::XOR:
            case Opcode::SHL:
            case Opcode::SHR:
            case Opcode::SEL:
            case Opcode::SMOOTHSTEP:
            case Opcode::SETP:
            case Opcode::POW:
            case Opcode::DOT3:
            case Opcode::DOT4:
            case Opcode::TEX:
            case Opcode::SAMPLE:
                return Format::A;
            case Opcode::RCP:
            case Opcode::SQRT:
            case Opcode::RSQ:
            case Opcode::SIN:
            case Opcode::COS:
            case Opcode::EXPD2:
            case Opcode::LOGD2:
            case Opcode::ABS:
            case Opcode::NEG:
            case Opcode::FLOOR:
            case Opcode::CEIL:
            case Opcode::FRACT:
            case Opcode::F2I:
            case Opcode::I2F:
            case Opcode::NOT:
            case Opcode::MOV:
                return Format::C;
            case Opcode::BRA:
            case Opcode::CALL:
            case Opcode::JMP:
            case Opcode::LD:
            case Opcode::ST:
            case Opcode::OUTPUT:
            case Opcode::MOV_IMM:
            case Opcode::VLOAD:
            case Opcode::VSTORE:
            case Opcode::OUTPUT_VS:
            case Opcode::LDC:
            case Opcode::ATTR:
                return Format::B;
            default:
                return Format::E;
        }
    }

    Format GetFormat() const { return GetFormat(GetOpcode()); }

    // Factory: Format-D
    static Instruction MakeD(Opcode op) {
        return Instruction(static_cast<uint32_t>(op) << 24);
    }

    // Factory: Format-A (R-type, 3 registers)
    static Instruction MakeA(Opcode op, uint8_t rd, uint8_t ra, uint8_t rb) {
        uint32_t w = (static_cast<uint32_t>(op) << 24)
                   | (static_cast<uint32_t>(rd & 0x7F) << 17)
                   | (static_cast<uint32_t>(ra & 0x7F) << 10)
                   | (static_cast<uint32_t>(rb & 0x7F) << 3);
        return Instruction(w);
    }

    // Factory: Format-B (dual-word)
    // Word2 layout: imm[9:0]=bits[9:0], Rb[6:0]=bits[18:12] (gap bits[11:10] unused)
    static Instruction MakeB(Opcode op, uint8_t rd, uint8_t ra, uint8_t rb, uint16_t imm) {
        uint32_t w1 = (static_cast<uint32_t>(op) << 24)
                    | (static_cast<uint32_t>(rd & 0x7F) << 17)
                    | (static_cast<uint32_t>(ra & 0x7F) << 10);
        uint32_t w2 = (static_cast<uint32_t>(rb & 0x7F) << 12) | (imm & 0x3FF);
        return Instruction(w1, w2);
    }

    // Factory: Format-B BRA (condition register only)
    static Instruction MakeBRA(uint8_t ra, int16_t off) {
        uint32_t w1 = (static_cast<uint32_t>(Opcode::BRA) << 24)
                    | (static_cast<uint32_t>(ra & 0x7F) << 10);
        uint32_t w2 = static_cast<uint32_t>(off & 0x3FF);
        return Instruction(w1, w2);
    }

    // Factory: Format-C (single register + func)
    static Instruction MakeC(Opcode op, uint8_t rd, uint8_t ra) {
        uint32_t w = (static_cast<uint32_t>(op) << 24)
                   | (static_cast<uint32_t>(rd & 0x7F) << 17)
                   | (static_cast<uint32_t>(ra & 0x7F) << 10);
        return Instruction(w);
    }

    // Factory: Format-E (2 registers)
    static Instruction MakeE(Opcode op, uint8_t rd, uint8_t ra) {
        uint32_t w = (static_cast<uint32_t>(op) << 24)
                   | (static_cast<uint32_t>(rd & 0x7F) << 17)
                   | (static_cast<uint32_t>(ra & 0x7F) << 10);
        return Instruction(w);
    }

    // Factory: MAD (R4-type with Rc=Rb[4:0] in v2.5 encoding)
    // In v2.5, Rb occupies bits[9:3] (7 bits) and Rc occupies bits[9:5] (5 bits).
    // Since they overlap, Rc is constrained to equal Rb[4:0].
    // This factory enforces: Rc = Rb & 0x1F.
    static Instruction MakeMAD(uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc) {
        uint32_t rb_field = rb; // Rc = Rb[4:0], so Rb_field = Rb
        uint32_t w = (static_cast<uint32_t>(Opcode::MAD) << 24)
                   | (static_cast<uint32_t>(rd & 0x7F) << 17)
                   | (static_cast<uint32_t>(ra & 0x7F) << 10)
                   | (static_cast<uint32_t>(rb_field & 0x7F) << 3);
        return Instruction(w);
    }
};

// ============================================================================
// Utility
// ============================================================================
inline const char* GetOpcodeName(Opcode op) {
    switch (op) {
        case Opcode::NOP: return "NOP";
        case Opcode::BRA: return "BRA";
        case Opcode::CALL: return "CALL";
        case Opcode::RET: return "RET";
        case Opcode::JMP: return "JMP";
        case Opcode::BAR: return "BAR";
        case Opcode::HALT: return "HALT";
        case Opcode::ADD: return "ADD";
        case Opcode::SUB: return "SUB";
        case Opcode::MUL: return "MUL";
        case Opcode::DIV: return "DIV";
        case Opcode::MAD: return "MAD";
        case Opcode::CMP: return "CMP";
        case Opcode::MIN: return "MIN";
        case Opcode::MAX: return "MAX";
        case Opcode::AND: return "AND";
        case Opcode::OR: return "OR";
        case Opcode::XOR: return "XOR";
        case Opcode::SHL: return "SHL";
        case Opcode::SHR: return "SHR";
        case Opcode::SEL: return "SEL";
        case Opcode::SMOOTHSTEP: return "SMOOTHSTEP";
        case Opcode::SETP: return "SETP";
        case Opcode::RCP: return "RCP";
        case Opcode::SQRT: return "SQRT";
        case Opcode::RSQ: return "RSQ";
        case Opcode::SIN: return "SIN";
        case Opcode::COS: return "COS";
        case Opcode::EXPD2: return "EXPD2";
        case Opcode::LOGD2: return "LOGD2";
        case Opcode::POW: return "POW";
        case Opcode::ABS: return "ABS";
        case Opcode::NEG: return "NEG";
        case Opcode::FLOOR: return "FLOOR";
        case Opcode::CEIL: return "CEIL";
        case Opcode::FRACT: return "FRACT";
        case Opcode::F2I: return "F2I";
        case Opcode::I2F: return "I2F";
        case Opcode::NOT: return "NOT";
        case Opcode::MOV: return "MOV";
        case Opcode::LD: return "LD";
        case Opcode::ST: return "ST";
        case Opcode::TEX: return "TEX";
        case Opcode::SAMPLE: return "SAMPLE";
        case Opcode::OUTPUT: return "OUTPUT";
        case Opcode::MOV_IMM: return "MOV_IMM";
        case Opcode::VLOAD: return "VLOAD";
        case Opcode::VSTORE: return "VSTORE";
        case Opcode::OUTPUT_VS: return "OUTPUT_VS";
        case Opcode::LDC: return "LDC";
        case Opcode::ATTR: return "ATTR";
        case Opcode::DOT3: return "DOT3";
        case Opcode::DOT4: return "DOT4";
        default: return "INVALID";
    }
}

inline int GetCycles(Opcode op) {
    switch (op) {
        case Opcode::DIV: return 7;
        case Opcode::TEX:
        case Opcode::SAMPLE: return 10;
        case Opcode::LD:
        case Opcode::ST:
        case Opcode::VLOAD:
        case Opcode::VSTORE:
        case Opcode::OUTPUT:
        case Opcode::OUTPUT_VS:
        case Opcode::MOV_IMM:
        case Opcode::LDC:
        case Opcode::ATTR: return 2;
        default: return 1;
    }
}

}}} // namespaces
