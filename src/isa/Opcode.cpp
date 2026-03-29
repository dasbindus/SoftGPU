#include "Opcode.hpp"
#include <array>
#include <cstring>

namespace softgpu {
namespace isa {

// ============================================================================
// Opcode Implementation
// ============================================================================

IType GetInstructionType(Opcode op)
{
    switch (op) {
        // R-type: 3 registers
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::CMP:
        case Opcode::MIN:
        case Opcode::MAX:
            return IType::R;
        
        // R4-type: 4 registers
        case Opcode::MAD:
        case Opcode::SEL:
        case Opcode::TEX:
        case Opcode::SAMPLE:
        case Opcode::SMOOTHSTEP:
            return IType::R4;
        
        // U-type: 1 register + immediate
        case Opcode::RCP:
        case Opcode::SQRT:
        case Opcode::RSQ:
        case Opcode::MOV:
        case Opcode::F2I:
        case Opcode::I2F:
        case Opcode::FRACT:
        case Opcode::LDC:
        case Opcode::NOT:
        case Opcode::FLOOR:
        case Opcode::CEIL:
        case Opcode::ABS:
        case Opcode::NEG:
        case Opcode::SHL:
        case Opcode::SHR:
            return IType::U;
        
        // I-type: LD, ST
        case Opcode::LD:
        case Opcode::ST:
            return IType::I;
        
        // B-type: BRA
        case Opcode::BRA:
            return IType::B;
        
        // J-type: JMP, CALL, RET, NOP, BAR
        case Opcode::JMP:
        case Opcode::CALL:
        case Opcode::RET:
        case Opcode::NOP:
        case Opcode::BAR:
            return IType::J;
        
        default:
            return IType::R; // Default to R-type
    }
}

const char* GetOpcodeName(Opcode op)
{
    switch (op) {
        case Opcode::NOP:   return "NOP";
        case Opcode::ADD:   return "ADD";
        case Opcode::SUB:   return "SUB";
        case Opcode::MUL:   return "MUL";
        case Opcode::DIV:   return "DIV";
        case Opcode::MAD:   return "MAD";
        case Opcode::RCP:   return "RCP";
        case Opcode::SQRT:   return "SQRT";
        case Opcode::RSQ:   return "RSQ";
        case Opcode::AND:   return "AND";
        case Opcode::OR:    return "OR";
        case Opcode::CMP:   return "CMP";
        case Opcode::SEL:   return "SEL";
        case Opcode::MIN:   return "MIN";
        case Opcode::MAX:   return "MAX";
        case Opcode::LD:    return "LD";
        case Opcode::ST:    return "ST";
        case Opcode::BRA:   return "BRA";
        case Opcode::JMP:   return "JMP";
        case Opcode::CALL:  return "CALL";
        case Opcode::RET:   return "RET";
        case Opcode::MOV:   return "MOV";
        case Opcode::F2I:   return "F2I";
        case Opcode::I2F:   return "I2F";
        case Opcode::FRACT: return "FRACT";
        case Opcode::TEX:   return "TEX";
        case Opcode::SAMPLE:return "SAMPLE";
        case Opcode::LDC:   return "LDC";
        case Opcode::BAR:   return "BAR";
        case Opcode::SHL:   return "SHL";
        case Opcode::SHR:   return "SHR";
        case Opcode::NOT:   return "NOT";
        case Opcode::FLOOR: return "FLOOR";
        case Opcode::CEIL:  return "CEIL";
        case Opcode::ABS:   return "ABS";
        case Opcode::NEG:   return "NEG";
        case Opcode::SMOOTHSTEP: return "SMOOTHSTEP";
        default:            return "INVALID";
    }
}

int GetCycles(Opcode op)
{
    switch (op) {
        case Opcode::NOP:   return 1;
        case Opcode::ADD:   return 1;
        case Opcode::SUB:   return 1;
        case Opcode::MUL:   return 1;
        case Opcode::DIV:   return 7;
        case Opcode::MAD:   return 1;
        case Opcode::RCP:   return 4;
        case Opcode::SQRT:  return 4;
        case Opcode::RSQ:   return 2;
        case Opcode::AND:   return 1;
        case Opcode::OR:    return 1;
        case Opcode::CMP:   return 1;
        case Opcode::SEL:   return 1;
        case Opcode::MIN:   return 1;
        case Opcode::MAX:   return 1;
        case Opcode::LD:    return 4;
        case Opcode::ST:    return 4;
        case Opcode::BRA:   return 1;
        case Opcode::JMP:   return 2;
        case Opcode::CALL:  return 3;
        case Opcode::RET:   return 2;
        case Opcode::MOV:   return 1;
        case Opcode::F2I:   return 1;
        case Opcode::I2F:   return 1;
        case Opcode::FRACT: return 1;
        case Opcode::TEX:   return 8;
        case Opcode::SAMPLE:return 4;
        case Opcode::LDC:   return 2;
        case Opcode::BAR:   return 1;
        // PHASE3: New instructions
        case Opcode::SHL:   return 1;   // Shift left
        case Opcode::SHR:   return 1;   // Shift right
        case Opcode::NOT:   return 1;   // Bitwise NOT
        case Opcode::FLOOR: return 1;   // Floor
        case Opcode::CEIL:  return 1;   // Ceiling
        case Opcode::ABS:   return 1;   // Absolute value
        case Opcode::NEG:   return 1;   // Negate
        case Opcode::SMOOTHSTEP: return 2; // Smoothstep
        default:            return 1;
    }
}

bool IsP0(Opcode op)
{
    switch (op) {
        // P0 instructions - must implement for v1.0
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::MAD:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::CMP:
        case Opcode::SEL:
        case Opcode::MIN:
        case Opcode::MAX:
        case Opcode::LD:
        case Opcode::ST:
        case Opcode::BRA:
        case Opcode::JMP:
        case Opcode::CALL:
        case Opcode::RET:
        case Opcode::NOP:
        case Opcode::MOV:
            return true;
        
        // P1 instructions - v1.x (including PHASE3 new instructions)
        default:
            return false;
    }
}

} // namespace isa
} // namespace softgpu
