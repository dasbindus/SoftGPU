/**
 * GoldenTestCases.hpp - Phase 1 P0 Instruction Golden Test Cases
 * 
 * ISA v2.5 / Implementation v1.x
 * 
 * Test case format:
 *   - name: unique identifier
 *   - description: human-readable test purpose
 *   - category: arithmetic | control_flow | memory | bitwise | scoreboard
 *   - is_p0: true for P0 priority
 *   - initial_regs: map of register# → float value
 *   - initial_mem: map of address → float value
 *   - program: list of {raw_uint32, comment}
 *   - expected: {regs, mem, cycles, instructions}
 * 
 * Coverage:
 *   - Each test case exercises a specific opcode
 *   - Coverage tracked per opcode
 * 
 * Phase 1 scope: NOP, ADD, SUB (golden testcases)
 * Phase 1 full scope (15 P0 + double-word + scoreboard): to follow
 */

#pragma once

#include "GoldenTestCase.hpp"
#include "../../../src/isa/Instruction.hpp"
#include "../../../src/isa/Interpreter.hpp"
#include <vector>
#include <string>

namespace softgpu {
namespace isa {
namespace golden {

using namespace softgpu::isa;

// ============================================================================
// Helper: Build program from instruction list
// ============================================================================

inline std::vector<ProgramInstruction> MakeProgram(
    const std::initializer_list<std::pair<uint32_t, const char*>>& instrs)
{
    std::vector<ProgramInstruction> prog;
    for (const auto& [raw, comment] : instrs) {
        prog.push_back({raw, comment ? std::string(comment) : ""});
    }
    return prog;
}

// ============================================================================
// NOP Test Cases
// ============================================================================

inline TestCase MakeNOP_Basic() {
    TestCase tc;
    tc.name = "nop_basic";
    tc.description = "NOP: PC advances by 4, no state change";
    tc.category = Category::CONTROL_FLOW;
    tc.is_p0 = true;
    
    // No initial registers needed (R0 is always 0)
    tc.program = MakeProgram({
        {Instruction::MakeNOP().raw, "NOP"},
        {Instruction::MakeNOP().raw, "NOP"},
        {Instruction::MakeR(Opcode::ADD, 1, 0, 0).raw, "ADD R1=R0+R0=0 (marker)"}
    });
    
    // After 3 NOPs + ADD, R1 should be 0 (ADD R1=R0+R0)
    tc.expected.regs[1] = FloatWithTol(0.0f);
    tc.expected.expected_cycles = 3;  // 3 instructions
    tc.expected.expected_instructions = 3;
    
    return tc;
}

inline TestCase MakeNOP_Only() {
    TestCase tc;
    tc.name = "nop_only_sequence";
    tc.description = "NOP: 5 consecutive NOPs, no register modification";
    tc.category = Category::CONTROL_FLOW;
    tc.is_p0 = true;
    
    // Pre-set some registers
    tc.initial_regs[1] = 42.0f;
    tc.initial_regs[2] = 99.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeNOP().raw, "NOP 1"},
        {Instruction::MakeNOP().raw, "NOP 2"},
        {Instruction::MakeNOP().raw, "NOP 3"},
        {Instruction::MakeNOP().raw, "NOP 4"},
        {Instruction::MakeNOP().raw, "NOP 5"},
    });
    
    // Registers should be unchanged
    tc.expected.regs[1] = FloatWithTol(42.0f);
    tc.expected.regs[2] = FloatWithTol(99.0f);
    tc.expected.expected_cycles = 5;
    tc.expected.expected_instructions = 5;
    
    return tc;
}

// ============================================================================
// ADD Test Cases
// ============================================================================

inline TestCase MakeADD_Positive() {
    TestCase tc;
    tc.name = "add_positive";
    tc.description = "ADD: R1 = 3.0 + 5.0 = 8.0 (positive, normal case)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 3.0f;
    tc.initial_regs[2] = 5.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 2).raw, "ADD R3=R1+R2"}
    });
    
    tc.expected.regs[3] = FloatWithTol(8.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeADD_Negative() {
    TestCase tc;
    tc.name = "add_negative";
    tc.description = "ADD: R1 = (-7.5) + 2.5 = -5.0 (negative result)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = -7.5f;
    tc.initial_regs[2] = 2.5f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 2).raw, "ADD R3=R1+R2"}
    });
    
    tc.expected.regs[3] = FloatWithTol(-5.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeADD_Overflow() {
    TestCase tc;
    tc.name = "add_overflow";
    tc.description = "ADD: large positive + large positive → inf (IEEE-754 overflow)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 3.4e38f;   // near max float
    tc.initial_regs[2] = 3.4e38f;  // near max float
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 2).raw, "ADD R3=R1+R2 (overflow)"}
    });
    
    tc.expected.regs[3] = FloatWithTol(std::numeric_limits<float>::infinity());
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeADD_ZeroReg() {
    TestCase tc;
    tc.name = "add_zero_reg";
    tc.description = "ADD: R0 is always 0, adding to it doesn't change result";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 123.0f;
    // R0 is hardwired to 0.0f, cannot be written
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 0).raw, "ADD R3=R1+R0 (=R1+0)"},
        {Instruction::MakeR(Opcode::ADD, 4, 0, 1).raw, "ADD R4=R0+R1 (=0+R1)"}
    });
    
    tc.expected.regs[3] = FloatWithTol(123.0f);
    tc.expected.regs[4] = FloatWithTol(123.0f);
    tc.expected.expected_cycles = 2;
    tc.expected.expected_instructions = 2;
    
    return tc;
}

inline TestCase MakeADD_NaN() {
    TestCase tc;
    tc.name = "add_nan_propagation";
    tc.description = "ADD: NaN + anything = NaN (IEEE-754 NaN propagation)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    // Create NaN by dividing 0/0
    tc.initial_regs[1] = std::nanf("");
    tc.initial_regs[2] = 42.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 2).raw, "ADD R3=NaN+42"}
    });
    
    tc.expected.regs[3] = FloatWithTol(std::nanf(""), 0.0f);  // NaN check
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeADD_Inf() {
    TestCase tc;
    tc.name = "add_inf";
    tc.description = "ADD: (+inf) + (-inf) = NaN; (+inf) + (+1) = +inf";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = std::numeric_limits<float>::infinity();
    tc.initial_regs[2] = -std::numeric_limits<float>::infinity();
    tc.initial_regs[3] = 1.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 4, 1, 2).raw, "ADD R4=inf+(-inf)=NaN"},
        {Instruction::MakeR(Opcode::ADD, 5, 1, 3).raw, "ADD R5=inf+1=inf"}
    });
    
    // +inf + -inf = NaN, +inf + 1 = +inf
    tc.expected.regs[4] = FloatWithTol(std::nanf(""), 0.0f);  // NaN
    tc.expected.regs[5] = FloatWithTol(std::numeric_limits<float>::infinity());
    tc.expected.expected_cycles = 2;
    tc.expected.expected_instructions = 2;
    
    return tc;
}

inline TestCase MakeADD_Small() {
    TestCase tc;
    tc.name = "add_small_precision";
    tc.description = "ADD: small values 1e-7 + 1e-7 = 2e-7 (preserve precision)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 1e-7f;
    tc.initial_regs[2] = 1e-7f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::ADD, 3, 1, 2).raw, "ADD R3=1e-7+1e-7"}
    });
    
    // Allow some tolerance for floating point precision
    tc.expected.regs[3] = FloatWithTol(2e-7f, 1e-8f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

// ============================================================================
// SUB Test Cases
// ============================================================================

inline TestCase MakeSUB_Positive() {
    TestCase tc;
    tc.name = "sub_positive";
    tc.description = "SUB: R1 = 10.0 - 3.0 = 7.0 (positive, normal case)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 10.0f;
    tc.initial_regs[2] = 3.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::SUB, 3, 1, 2).raw, "SUB R3=R1-R2"}
    });
    
    tc.expected.regs[3] = FloatWithTol(7.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeSUB_NegativeResult() {
    TestCase tc;
    tc.name = "sub_negative_result";
    tc.description = "SUB: R1 = 3.0 - 10.0 = -7.0 (negative result)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 3.0f;
    tc.initial_regs[2] = 10.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::SUB, 3, 1, 2).raw, "SUB R3=R1-R2"}
    });
    
    tc.expected.regs[3] = FloatWithTol(-7.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeSUB_ZeroResult() {
    TestCase tc;
    tc.name = "sub_zero_result";
    tc.description = "SUB: R1 = 5.5 - 5.5 = 0.0 (exact zero)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = 5.5f;
    tc.initial_regs[2] = 5.5f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::SUB, 3, 1, 2).raw, "SUB R3=R1-R2"}
    });
    
    tc.expected.regs[3] = FloatWithTol(0.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

inline TestCase MakeSUB_Inf() {
    TestCase tc;
    tc.name = "sub_inf";
    tc.description = "SUB: (+inf) - (+inf) = NaN; (-inf) - (+inf) = -inf";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    tc.initial_regs[1] = std::numeric_limits<float>::infinity();
    tc.initial_regs[2] = std::numeric_limits<float>::infinity();
    tc.initial_regs[3] = -std::numeric_limits<float>::infinity();
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::SUB, 4, 1, 2).raw, "SUB R4=inf-inf=NaN"},
        {Instruction::MakeR(Opcode::SUB, 5, 3, 2).raw, "SUB R5=-inf-inf=-inf"}
    });
    
    tc.expected.regs[4] = FloatWithTol(std::nanf(""), 0.0f);  // NaN
    tc.expected.regs[5] = FloatWithTol(-std::numeric_limits<float>::infinity());
    tc.expected.expected_cycles = 2;
    tc.expected.expected_instructions = 2;
    
    return tc;
}

inline TestCase MakeSUB_ZeroFromNegative() {
    TestCase tc;
    tc.name = "sub_from_zero";
    tc.description = "SUB: R0 - x = -x (0 minus negative = positive)";
    tc.category = Category::ARITHMETIC;
    tc.is_p0 = true;
    
    // R0 is hardwired 0.0
    tc.initial_regs[1] = -5.0f;
    
    tc.program = MakeProgram({
        {Instruction::MakeR(Opcode::SUB, 2, 0, 1).raw, "SUB R2=R0-R1 = -(-5.0) = 5.0"}
    });
    
    tc.expected.regs[2] = FloatWithTol(5.0f);
    tc.expected.expected_cycles = 1;
    tc.expected.expected_instructions = 1;
    
    return tc;
}

// ============================================================================
// All Phase 1 Test Cases (for registration)
// ============================================================================

inline std::vector<TestCase> GetPhase1TestCases() {
    return {
        // --- NOP ---
        MakeNOP_Basic(),
        MakeNOP_Only(),
        
        // --- ADD ---
        MakeADD_Positive(),
        MakeADD_Negative(),
        MakeADD_Overflow(),
        MakeADD_ZeroReg(),
        MakeADD_NaN(),
        MakeADD_Inf(),
        MakeADD_Small(),
        
        // --- SUB ---
        MakeSUB_Positive(),
        MakeSUB_NegativeResult(),
        MakeSUB_ZeroResult(),
        MakeSUB_Inf(),
        MakeSUB_ZeroFromNegative(),
    };
}

// ============================================================================
// P0 Coverage Tracking
// ============================================================================

// List of P0 opcodes (from ISA v2.5 spec)
inline std::vector<std::pair<uint8_t, const char*>> GetP0Opcodes() {
    return {
        {0x00, "NOP"},
        {0x01, "ADD"},
        {0x02, "SUB"},
        {0x03, "MUL"},
        {0x04, "DIV"},
        {0x05, "MAD"},
        {0x09, "AND"},
        {0x0A, "OR"},
        {0x0B, "CMP"},
        {0x0D, "MIN"},
        {0x0E, "MAX"},
        {0x11, "BRA"},
        {0x12, "JMP"},
        {0x13, "CALL"},
        {0x14, "RET"},
        // HALT: 0x0F not implemented in current codebase (only VS_HALT at 0x2A)
    };
}

} // namespace golden
} // namespace isa
} // namespace softgpu
