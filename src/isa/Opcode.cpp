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
        case Opcode::DP3:
        // VS R-type additions
        case Opcode::VS_ADD:
        case Opcode::VS_SUB:
        case Opcode::VS_MUL:
        case Opcode::VS_DIV:
        case Opcode::VS_DOT3:
        case Opcode::VS_DOT4:
        case Opcode::VS_CROSS:
        case Opcode::VS_LENGTH:
        case Opcode::VS_CMP:
        case Opcode::VS_MIN:
        case Opcode::VS_MAX:
        case Opcode::VS_SETP:
        case Opcode::VS_POW:
        case Opcode::VS_AND:
        case Opcode::VS_OR:
        case Opcode::VS_XOR:
        case Opcode::VS_SHL:
        case Opcode::VS_SHR:
        case Opcode::VS_NORMALIZE:
            return IType::R;
        
        // R4-type: 4 registers
        case Opcode::MAD:
        case Opcode::SEL:
        case Opcode::TEX:
        case Opcode::SAMPLE:
        case Opcode::SMOOTHSTEP:
        // VS R4-type
        case Opcode::VS_MAT_MUL:
        case Opcode::VS_MAD:
        case Opcode::VS_MAT_ADD:
        case Opcode::VS_MAT_TRANSPOSE:
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
        // VS U-type (Phase 2)
        case Opcode::VS_SQRT:
        case Opcode::VS_RSQ:
        case Opcode::VS_SIN:
        case Opcode::VS_COS:
        case Opcode::VS_EXPD2:
        case Opcode::VS_LOGD2:
        case Opcode::VS_NOT:
        case Opcode::VS_MOV:
        case Opcode::VS_MOV_IMM:
        case Opcode::VS_CVT_F32_S32:
        case Opcode::VS_CVT_F32_U32:
        case Opcode::VS_CVT_S32_F32:
        case Opcode::VS_ATTR:
            return IType::U;
        
        // I-type: LD, ST
        case Opcode::LD:
        case Opcode::ST:
        // VS I-type
        case Opcode::VS_VLOAD:
        case Opcode::VS_VSTORE:
            return IType::I;
        
        // B-type: BRA, VS_CBR
        case Opcode::BRA:
        case Opcode::VS_CBR:
            return IType::B;
        
        // J-type: JMP, CALL, RET, NOP, BAR
        case Opcode::JMP:
        case Opcode::CALL:
        case Opcode::RET:
        case Opcode::NOP:
        case Opcode::BAR:
        // VS J-type
        case Opcode::VS_NOP:
        case Opcode::VS_HALT:
        case Opcode::VS_JUMP:
        case Opcode::VS_VOUTPUT:
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
        case Opcode::DP3:     return "DP3";
        // VS Phase 1 opcodes
        case Opcode::VS_VOUTPUT:   return "VS_VOUTPUT";
        case Opcode::VS_MAT_MUL:   return "VS_MAT_MUL";
        case Opcode::VS_VLOAD:     return "VS_VLOAD";
        case Opcode::VS_HALT:      return "VS_HALT";
        case Opcode::VS_NOP:       return "VS_NOP";
        case Opcode::VS_JUMP:      return "VS_JUMP";
        case Opcode::VS_ADD:        return "VS_ADD";
        case Opcode::VS_SUB:       return "VS_SUB";
        case Opcode::VS_MUL:       return "VS_MUL";
        case Opcode::VS_DIV:       return "VS_DIV";
        case Opcode::VS_DOT3:      return "VS_DOT3";
        case Opcode::VS_NORMALIZE:  return "VS_NORMALIZE";
        // VS Phase 2 opcodes
        case Opcode::VS_CBR:         return "VS_CBR";
        case Opcode::VS_MAD:         return "VS_MAD";
        case Opcode::VS_SQRT:        return "VS_SQRT";
        case Opcode::VS_RSQ:         return "VS_RSQ";
        case Opcode::VS_CMP:         return "VS_CMP";
        case Opcode::VS_MIN:          return "VS_MIN";
        case Opcode::VS_MAX:         return "VS_MAX";
        case Opcode::VS_SETP:        return "VS_SETP";
        case Opcode::VS_DOT4:        return "VS_DOT4";
        case Opcode::VS_CROSS:       return "VS_CROSS";
        case Opcode::VS_LENGTH:      return "VS_LENGTH";
        case Opcode::VS_MAT_ADD:     return "VS_MAT_ADD";
        case Opcode::VS_MAT_TRANSPOSE: return "VS_MAT_TRANSPOSE";
        case Opcode::VS_ATTR:         return "VS_ATTR";
        case Opcode::VS_VSTORE:      return "VS_VSTORE";
        case Opcode::VS_SIN:         return "VS_SIN";
        case Opcode::VS_COS:         return "VS_COS";
        case Opcode::VS_EXPD2:       return "VS_EXPD2";
        case Opcode::VS_LOGD2:       return "VS_LOGD2";
        case Opcode::VS_POW:         return "VS_POW";
        case Opcode::VS_AND:         return "VS_AND";
        case Opcode::VS_OR:          return "VS_OR";
        case Opcode::VS_XOR:         return "VS_XOR";
        case Opcode::VS_NOT:         return "VS_NOT";
        case Opcode::VS_SHL:         return "VS_SHL";
        case Opcode::VS_SHR:         return "VS_SHR";
        case Opcode::VS_CVT_F32_S32: return "VS_CVT_F32_S32";
        case Opcode::VS_CVT_F32_U32: return "VS_CVT_F32_U32";
        case Opcode::VS_CVT_S32_F32: return "VS_CVT_S32_F32";
        case Opcode::VS_MOV:         return "VS_MOV";
        case Opcode::VS_MOV_IMM:     return "VS_MOV_IMM";
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
        case Opcode::DP3:     return 1;  // DP3 dot product
        // VS Phase 1 opcodes
        case Opcode::VS_VOUTPUT:      return 2;   // VOUTPUT: 2-cycle
        case Opcode::VS_MAT_MUL:     return 4;   // MAT_MUL: 4-cycle (pipeline)
        case Opcode::VS_VLOAD:       return 2;   // VLOAD: 2-cycle
        case Opcode::VS_HALT:        return 1;   // HALT: 1-cycle
        case Opcode::VS_NOP:         return 1;   // NOP: 1-cycle
        case Opcode::VS_JUMP:        return 1;   // JUMP: 1-cycle
        case Opcode::VS_ADD:         return 1;   // ADD: 1-cycle
        case Opcode::VS_SUB:         return 1;   // SUB: 1-cycle
        case Opcode::VS_MUL:         return 1;   // MUL: 1-cycle
        case Opcode::VS_DIV:         return 7;   // DIV: 7-cycle (PendingDiv queue)
        case Opcode::VS_DOT3:        return 1;   // DOT3: 1-cycle
        case Opcode::VS_NORMALIZE:   return 5;   // NORMALIZE: 5-cycle (DOT3+RSQ+MUL×3)
        // VS Phase 2 opcodes
        case Opcode::VS_CBR:           return 1;   // CBR: 1-cycle
        case Opcode::VS_MAD:           return 1;   // MAD: 1-cycle
        case Opcode::VS_SQRT:          return 1;   // SQRT: 1-cycle (EU_SFU)
        case Opcode::VS_RSQ:           return 1;   // RSQ: 1-cycle (EU_SFU)
        case Opcode::VS_CMP:           return 1;   // CMP: 1-cycle
        case Opcode::VS_MIN:           return 1;   // MIN: 1-cycle
        case Opcode::VS_MAX:           return 1;   // MAX: 1-cycle
        case Opcode::VS_SETP:          return 1;   // SETP: 1-cycle (stub)
        case Opcode::VS_DOT4:           return 1;   // DOT4: 1-cycle
        case Opcode::VS_CROSS:          return 1;   // CROSS: 1-cycle
        case Opcode::VS_LENGTH:         return 1;   // LENGTH: 1-cycle
        case Opcode::VS_MAT_ADD:       return 1;   // MAT_ADD: 1-cycle
        case Opcode::VS_MAT_TRANSPOSE:  return 4;   // MAT_TRANSPOSE: 4-cycle
        case Opcode::VS_ATTR:           return 1;   // ATTR: 1-cycle (stub)
        case Opcode::VS_VSTORE:        return 1;   // VSTORE: 1-cycle
        case Opcode::VS_SIN:            return 1;   // SIN: 1-cycle (polynomial)
        case Opcode::VS_COS:           return 1;   // COS: 1-cycle (polynomial)
        case Opcode::VS_EXPD2:          return 1;   // EXPD2: 1-cycle
        case Opcode::VS_LOGD2:          return 1;   // LOGD2: 1-cycle
        case Opcode::VS_POW:            return 1;   // POW: 1-cycle
        case Opcode::VS_AND:            return 1;   // AND: 1-cycle
        case Opcode::VS_OR:             return 1;   // OR: 1-cycle
        case Opcode::VS_XOR:            return 1;   // XOR: 1-cycle
        case Opcode::VS_NOT:            return 1;   // NOT: 1-cycle
        case Opcode::VS_SHL:            return 1;   // SHL: 1-cycle
        case Opcode::VS_SHR:            return 1;   // SHR: 1-cycle
        case Opcode::VS_CVT_F32_S32:   return 1;   // CVT_F32_S32: 1-cycle
        case Opcode::VS_CVT_F32_U32:   return 1;   // CVT_F32_U32: 1-cycle
        case Opcode::VS_CVT_S32_F32:   return 1;   // CVT_S32_F32: 1-cycle
        case Opcode::VS_MOV:            return 1;   // MOV: 1-cycle
        case Opcode::VS_MOV_IMM:        return 1;   // MOV_IMM: 1-cycle
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

        // VS Phase 1 instructions (P1)
        case Opcode::VS_NOP:
        case Opcode::VS_HALT:
        case Opcode::VS_JUMP:
        case Opcode::VS_ADD:
        case Opcode::VS_SUB:
        case Opcode::VS_MUL:
        case Opcode::VS_DIV:
        case Opcode::VS_DOT3:
        case Opcode::VS_NORMALIZE:
        case Opcode::VS_MAT_MUL:
        case Opcode::VS_VLOAD:
        case Opcode::VS_VOUTPUT:
            return true;
        
        // VS Phase 2 instructions (P2) — not P0, but valid ISA
        case Opcode::VS_CBR:
        case Opcode::VS_MAD:
        case Opcode::VS_SQRT:
        case Opcode::VS_RSQ:
        case Opcode::VS_CMP:
        case Opcode::VS_MIN:
        case Opcode::VS_MAX:
        case Opcode::VS_SETP:
        case Opcode::VS_DOT4:
        case Opcode::VS_CROSS:
        case Opcode::VS_LENGTH:
        case Opcode::VS_MAT_ADD:
        case Opcode::VS_MAT_TRANSPOSE:
        case Opcode::VS_ATTR:
        case Opcode::VS_VSTORE:
        case Opcode::VS_SIN:
        case Opcode::VS_COS:
        case Opcode::VS_EXPD2:
        case Opcode::VS_LOGD2:
        case Opcode::VS_POW:
        case Opcode::VS_AND:
        case Opcode::VS_OR:
        case Opcode::VS_XOR:
        case Opcode::VS_NOT:
        case Opcode::VS_SHL:
        case Opcode::VS_SHR:
        case Opcode::VS_CVT_F32_S32:
        case Opcode::VS_CVT_F32_U32:
        case Opcode::VS_CVT_S32_F32:
        case Opcode::VS_MOV:
        case Opcode::VS_MOV_IMM:
            return false;  // Phase 2, not P0
        
        // P1 instructions - v1.x (including PHASE3 new instructions)
        default:
            return false;
    }
}

} // namespace isa
} // namespace softgpu
