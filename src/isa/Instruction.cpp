#include "Instruction.hpp"
#include <cstdio>
#include <cstring>

namespace softgpu {
namespace isa {

std::string Instruction::Disassemble() const
{
    char buf[64];
    Opcode op = GetOpcode();
    
    switch (GetType()) {
        case IType::R: {
            // ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX
            snprintf(buf, sizeof(buf), "%-6s R%d, R%d, R%d",
                    GetOpcodeName(op), GetRd(), GetRa(), GetRb());
            break;
        }
        
        case IType::R4: {
            // MAD, SEL, TEX, SAMPLE
            if (op == Opcode::MAD) {
                snprintf(buf, sizeof(buf), "%-6s R%d, R%d, R%d, R%d",
                        GetOpcodeName(op), GetRd(), GetRa(), GetRb(), GetRc());
            } else if (op == Opcode::SEL) {
                snprintf(buf, sizeof(buf), "%-6s R%d, R%d, R%d, R%d",
                        GetOpcodeName(op), GetRd(), GetRa(), GetRb(), GetRc());
            } else {
                snprintf(buf, sizeof(buf), "%-6s R%d, R%d, R%d, R%d",
                        GetOpcodeName(op), GetRd(), GetRa(), GetRb(), GetRc());
            }
            break;
        }
        
        case IType::U: {
            // RCP, SQRT, RSQ, MOV, F2I, I2F, FRACT, LDC
            if (op == Opcode::LDC) {
                snprintf(buf, sizeof(buf), "%-6s R%d, C[%d+%d]",
                        GetOpcodeName(op), GetRd(), GetRa(), GetImm());
            } else {
                snprintf(buf, sizeof(buf), "%-6s R%d, R%d",
                        GetOpcodeName(op), GetRd(), GetRa());
            }
            break;
        }
        
        case IType::I: {
            // LD, ST
            if (op == Opcode::LD) {
                snprintf(buf, sizeof(buf), "%-6s R%d, [R%d+0x%03X]",
                        GetOpcodeName(op), GetRd(), GetRa(), GetImm());
            } else {
                snprintf(buf, sizeof(buf), "%-6s [R%d+0x%03X], R%d",
                        GetOpcodeName(op), GetRa(), GetImm(), GetRb());
            }
            break;
        }
        
        case IType::B: {
            // BRA
            snprintf(buf, sizeof(buf), "%-6s R%d, %+d",
                    GetOpcodeName(op), GetRa(), GetSignedImm());
            break;
        }
        
        case IType::J: {
            // JMP, CALL, RET, NOP, BAR
            if (op == Opcode::RET || op == Opcode::NOP || op == Opcode::BAR) {
                snprintf(buf, sizeof(buf), "%s", GetOpcodeName(op));
            } else if (op == Opcode::CALL) {
                snprintf(buf, sizeof(buf), "%-6s %d",
                        GetOpcodeName(op), GetSignedImm());
            } else {
                snprintf(buf, sizeof(buf), "%-6s %d",
                        GetOpcodeName(op), GetSignedImm());
            }
            break;
        }
        
        default:
            snprintf(buf, sizeof(buf), "UNKNOWN (opcode=0x%02X)", static_cast<int>(op));
            break;
    }
    
    return std::string(buf);
}

} // namespace isa
} // namespace softgpu
