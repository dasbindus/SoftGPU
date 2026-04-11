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
// Named Constants
// ============================================================================
constexpr uint16_t IMM10_MASK = 0x3FF;  // 10-bit immediate: max value 1023

// ============================================================================
// Opcode Metadata Table (DRY: format + cycles in one place)
// ============================================================================
struct OpcodeMeta {
    Format format;
    uint8_t cycles;
};

static constexpr OpcodeMeta kOpcodeTable[] = {
    // ---- Control Flow (0x00-0x0F)
    // [0x00] NOP
    { Format::D, 1 },
    // [0x01] BRA
    { Format::B, 1 },
    // [0x02] CALL
    { Format::B, 1 },
    // [0x03] RET
    { Format::D, 1 },
    // [0x04] JMP
    { Format::B, 1 },
    // [0x05] BAR
    { Format::D, 1 },
    // [0x06-0x0E] unused
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    // [0x0F] HALT
    { Format::D, 1 },

    // ---- ALU (0x10-0x1F)
    { Format::A, 1 }, // ADD
    { Format::A, 1 }, // SUB
    { Format::A, 1 }, // MUL
    { Format::A, 7 }, // DIV
    { Format::A, 1 }, // MAD
    { Format::A, 1 }, // CMP
    { Format::A, 1 }, // MIN
    { Format::A, 1 }, // MAX
    { Format::A, 1 }, // AND
    { Format::A, 1 }, // OR
    { Format::A, 1 }, // XOR
    { Format::A, 1 }, // SHL
    { Format::A, 1 }, // SHR
    { Format::A, 1 }, // SEL
    { Format::A, 1 }, // SMOOTHSTEP
    { Format::A, 1 }, // SETP

    // ---- SFU (0x20-0x2F)
    { Format::C, 1 }, // RCP
    { Format::C, 1 }, // SQRT
    { Format::C, 1 }, // RSQ
    { Format::C, 1 }, // SIN
    { Format::C, 1 }, // COS
    { Format::C, 1 }, // EXPD2
    { Format::C, 1 }, // LOGD2
    { Format::A, 1 }, // POW
    { Format::C, 1 }, // ABS
    { Format::C, 1 }, // NEG
    { Format::C, 1 }, // FLOOR
    { Format::C, 1 }, // CEIL
    { Format::C, 1 }, // FRACT
    { Format::C, 1 }, // F2I
    { Format::C, 1 }, // I2F
    { Format::C, 1 }, // NOT

    // ---- Memory (0x30-0x3F)
    { Format::B, 2 }, // LD
    { Format::B, 2 }, // ST
    { Format::A, 10 }, // TEX
    { Format::A, 10 }, // SAMPLE
    { Format::B, 2 }, // OUTPUT

    // ---- VS-Only (0x40-0x5F)
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, // 0x40-0x47
    { Format::B, 2 }, // MOV_IMM (0x48)
    { Format::B, 1 }, // VLOAD (0x49)
    { Format::B, 2 }, // VSTORE (0x4A)
    { Format::B, 2 }, // OUTPUT_VS (0x4B)
    { Format::B, 2 }, // LDC (0x4C)
    { Format::B, 2 }, // ATTR (0x4D)
    { Format::A, 1 }, // DOT3 (0x4E)
    { Format::A, 1 }, // DOT4 (0x4F)
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, // 0x50-0x5F

    // ---- VS MOV (0x63)
    { Format::C, 1 }, // MOV (0x63)

    // ---- Reserved/Invalid (0xC0-0xFE)
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 }, { Format::E, 1 },
    // [0xFF] INVALID
    { Format::E, 1 },
};

// ============================================================================
// Instruction
// ============================================================================
struct Instruction {
    uint32_t word1 = 0;
    uint32_t word2 = 0;

    Instruction() {}
    Instruction(uint32_t w1) : word1(w1) {}
    Instruction(uint32_t w1, uint32_t w2) : word1(w1), word2(w2) {}

    Opcode GetOpcode() const { return static_cast<Opcode>((word1 >> 24) & 0xFF); }
    uint8_t GetRd() const { return (word1 >> 17) & 0x7F; }
    uint8_t GetRa() const { return (word1 >> 10) & 0x7F; }
    uint8_t GetRb() const { return (word1 >> 3) & 0x7F; }
    // R4-type: Rc packed at bits [9:5] (same as old ISA)
    uint8_t GetRc() const { return (word1 >> 5) & 0x1F; }
    uint8_t GetRb_W2() const { return (word2 >> 12) & 0x7F; }
    uint16_t GetImm10() const { return word2 & 0x3FF; }

    int32_t GetSignedImm10() const {
        uint16_t u = word2 & IMM10_MASK;
        if (u & 0x200) return static_cast<int32_t>(u) - 1024;
        return static_cast<int32_t>(u);
    }

    uint16_t GetFunc() const { return (word1 >> 8) & 0x1FF; }

    static Format GetFormat(Opcode op) { return kOpcodeTable[static_cast<uint8_t>(op)].format; }
    static uint8_t GetCycles(Opcode op) { return kOpcodeTable[static_cast<uint8_t>(op)].cycles; }

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


}}} // namespaces
