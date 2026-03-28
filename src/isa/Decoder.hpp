#pragma once

#include "Instruction.hpp"
#include <functional>
#include <string>

namespace softgpu {
namespace isa {

// ============================================================================
// Instruction Decoder
// ============================================================================
// Converts raw 32-bit words into Instruction structures
// ============================================================================

class Decoder
{
public:
    Decoder() = default;

    // Decode a raw 32-bit word into an Instruction
    Instruction Decode(uint32_t word) const
    {
        return Instruction(word);
    }

    // Decode from memory at given address (for Fetch unit)
    Instruction DecodeFromMemory(const uint32_t* memory, uint32_t pc) const
    {
        return Instruction(memory[pc >> 2]); // PC is byte address, instructions are 4-byte aligned
    }

    // Batch decode for testing
    void DecodeBatch(const uint32_t* words, size_t count, Instruction* out) const
    {
        for (size_t i = 0; i < count; ++i) {
            out[i] = Instruction(words[i]);
        }
    }

    // Validate instruction encoding
    struct DecodeResult
    {
        bool valid;
        Instruction instruction;
        std::string error;

        explicit operator bool() const { return valid; }
    };

    DecodeResult Validate(uint32_t word) const
    {
        DecodeResult result;
        result.instruction = Instruction(word);
        
        Opcode op = result.instruction.GetOpcode();
        
        // Check opcode is in valid range
        if (op == Opcode::INVALID) {
            result.valid = false;
            result.error = "Invalid opcode";
            return result;
        }

        // Validate register fields (all should be < 64)
        uint8_t rd = result.instruction.GetRd();
        uint8_t ra = result.instruction.GetRa();
        uint8_t rb = result.instruction.GetRb();
        
        if (rd > 63 || ra > 63 || rb > 63) {
            result.valid = false;
            result.error = "Register index out of range";
            return result;
        }

        result.valid = true;
        return result;
    }

    // Get disassembly string
    std::string Disassemble(uint32_t word) const
    {
        return Instruction(word).Disassemble();
    }

    // Disassemble multiple instructions
    std::string DisassembleBatch(const uint32_t* words, size_t count, uint32_t basePC = 0) const
    {
        std::string output;
        for (size_t i = 0; i < count; ++i) {
            char line[128];
            snprintf(line, sizeof(line), "[%04X] %s\n", 
                     basePC + static_cast<uint32_t>(i * 4),
                     Instruction(words[i]).Disassemble().c_str());
            output += line;
        }
        return output;
    }
};

} // namespace isa
} // namespace softgpu
