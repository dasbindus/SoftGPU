/**
 * test_golden_isa.cpp - Phase 1 Golden ISA Test Suite
 * 
 * ISA v2.5 / Implementation v1.x
 * 
 * Tests P0 instructions (NOP, ADD, SUB) with comprehensive coverage:
 *   - Normal operation
 *   - Edge cases (NaN, Inf, overflow, underflow)
 *   - R0 zero-register behavior
 *   - Multi-instruction sequences
 * 
 * Phase 1 Scope:
 *   - P0: NOP, ADD, SUB
 *   - Full P0 (15 instr): ADD, SUB, MUL, DIV, MAD, CMP, MIN, MAX, AND, OR, BRA, JMP, CALL, RET, NOP
 *   - Double-word: LD, ST, VLOAD, VSTORE, LDC, MOV_IMM
 *   - Scoreboard: DIV RAW dependency, VLOAD→VSTORE pipeline
 * 
 * Usage:
 *   ./test_golden_isa          # Run all tests
 *   ./test_golden_isa --gtest_filter="*nop*:*add*:*ADD*"  # Filter by name
 */

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>

#include "golden_runner/GoldenTestRunner.hpp"
#include "golden_runner/GoldenTestCases.hpp"
#include "isa/Interpreter.hpp"
#include "isa/Instruction.hpp"
#include "isa/Opcode.hpp"

using namespace softgpu::isa;
using namespace softgpu::isa::golden;

// ============================================================================
// Test Fixture: Provides shared setup for all golden tests
// ============================================================================

class GoldenISATest : public ::testing::TestWithParam<TestCase> {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    static void PrintTo(const TestCase& tc, std::ostream* os) {
        *os << "[TestCase: " << tc.name << "] " << tc.description;
    }
};

// ============================================================================
// Parameterized Test: Run each TestCase through the GoldenTestRunner
// ============================================================================

TEST_P(GoldenISATest, Execute) {
    const TestCase& tc = GetParam();
    
    // Build the program
    std::vector<uint32_t> program_words;
    for (const auto& inst : tc.program) {
        program_words.push_back(inst.raw);
    }
    
    // Initialize interpreter
    Interpreter interp;
    for (const auto& [reg, val] : tc.initial_regs) {
        interp.SetRegister(static_cast<uint8_t>(reg), val);
    }
    for (const auto& [addr, val] : tc.initial_mem) {
        interp.SetMemory(addr, val);
    }
    
    // Execute the program
    uint32_t pc = 0;
    int steps = 0;
    const int max_steps = 10000;
    
    while (pc < program_words.size() * 4 && steps < max_steps) {
        uint32_t word_idx = pc / 4;
        if (word_idx >= program_words.size()) break;
        
        Instruction inst(program_words[word_idx]);
        Opcode op = inst.GetOpcode();
        
        if (op == Opcode::INVALID) {
            ADD_FAILURE() << "Invalid opcode at PC=0x" << std::hex << pc << std::dec;
            return;
        }
        
        interp.ExecuteInstruction(inst);
        pc = interp.GetPC();
        steps++;
        
        if (op == Opcode::VS_HALT || op == Opcode::RET) {
            break;
        }
    }
    
    // Note: stats_.cycles reflects pipeline-aware cycle count (via drainPendingDIVs).
    // When ExecuteInstruction is called directly (as here), cycles are not incremented.
    // For Phase 1 functional tests, we skip cycle count validation.
    // Only validate instructions_executed count which IS incremented by ExecuteInstruction.
    
    if (steps >= max_steps) {
        ADD_FAILURE() << "Exceeded max steps (" << max_steps << ")";
        return;
    }
    
    // Validate expected registers
    for (const auto& [reg, expected_val] : tc.expected.regs) {
        float actual = interp.GetRegister(static_cast<uint8_t>(reg));
        
        bool match = false;
        if (std::isnan(expected_val.value)) {
            match = std::isnan(actual);
        } else if (std::isinf(expected_val.value)) {
            match = std::isinf(actual) && (std::signbit(expected_val.value) == std::signbit(actual));
        } else {
            float diff = std::fabs(actual - expected_val.value);
            match = diff <= expected_val.abs_tol;
        }
        
        if (!match) {
            ADD_FAILURE() << "R" << (int)reg 
                          << ": expected=" << expected_val.value 
                          << " (tol=" << expected_val.abs_tol << ")"
                          << ", actual=" << actual;
        }
    }
    
    // Validate expected memory
    for (const auto& [addr, expected_val] : tc.expected.mem) {
        float actual = interp.GetMemory(addr);
        
        bool match = false;
        if (std::isnan(expected_val.value)) {
            match = std::isnan(actual);
        } else if (std::isinf(expected_val.value)) {
            match = std::isinf(actual) && (std::signbit(expected_val.value) == std::signbit(actual));
        } else {
            float diff = std::fabs(actual - expected_val.value);
            match = diff <= expected_val.abs_tol;
        }
        
        if (!match) {
            ADD_FAILURE() << "M[0x" << std::hex << addr << std::dec 
                          << "]: expected=" << expected_val.value 
                          << ", actual=" << actual;
        }
    }
    
    // Note: stats_.cycles validation is skipped for direct ExecuteInstruction() calls.
    // The functional golden test focuses on register/memory state correctness.
    // For pipeline-aware cycle testing, use Step() with proper m_program loading.
    
    // Validate expected instruction count (this IS incremented by ExecuteInstruction)
    if (tc.expected.expected_instructions >= 0) {
        uint64_t actual_insts = interp.GetStats().instructions_executed;
        if ((int)actual_insts != tc.expected.expected_instructions) {
            ADD_FAILURE() << "instructions: expected=" << tc.expected.expected_instructions 
                         << ", actual=" << actual_insts;
        }
    }
}

// ============================================================================
// Test Case Instantiation
// ============================================================================

INSTANTIATE_TEST_SUITE_P(
    Phase1,
    GoldenISATest,
    ::testing::ValuesIn(GetPhase1TestCases()),
    [](const ::testing::TestParamInfo<TestCase>& info) {
        return info.param.name;
    }
);

// ============================================================================
// Non-Parameterized Direct Tests (for special cases)
// ============================================================================

class DirectISATest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: NOP does not modify any registers
TEST_F(DirectISATest, NOP_PreservedRegisterState) {
    Interpreter interp;
    
    // Pre-load registers with distinct values
    interp.SetRegister(5, 3.14159f);
    interp.SetRegister(10, -2.71828f);
    interp.SetRegister(31, 1.0f);
    
    float before_5 = interp.GetRegister(5);
    float before_10 = interp.GetRegister(10);
    float before_31 = interp.GetRegister(31);
    
    // Execute NOP
    Instruction nop = Instruction::MakeNOP();
    interp.ExecuteInstruction(nop);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(5), before_5);
    EXPECT_FLOAT_EQ(interp.GetRegister(10), before_10);
    EXPECT_FLOAT_EQ(interp.GetRegister(31), before_31);
}

// Test: ADD with zero registers (R0 always 0)
TEST_F(DirectISATest, ADD_R0Hardwired) {
    Interpreter interp;
    
    // R0 should always read as 0.0 even if we try to write it
    interp.SetRegister(0, 999.0f);  // Should be silently discarded
    EXPECT_FLOAT_EQ(interp.GetRegister(0), 0.0f);
    
    // ADD with R0 as operand
    interp.SetRegister(1, 5.0f);
    
    Instruction add_r0 = Instruction::MakeR(Opcode::ADD, 2, 0, 1);  // R2 = R0 + R1
    interp.ExecuteInstruction(add_r0);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(2), 5.0f);
}

// Test: ADD commutative property
TEST_F(DirectISATest, ADD_Commutative) {
    Interpreter interp;
    
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 7.0f);
    
    // R3 = R1 + R2 = 3 + 7 = 10
    Instruction add1 = Instruction::MakeR(Opcode::ADD, 3, 1, 2);
    interp.ExecuteInstruction(add1);
    float r3 = interp.GetRegister(3);
    
    // Reset and try reverse
    interp.SetRegister(4, 7.0f);
    interp.SetRegister(5, 3.0f);
    
    // R6 = R4 + R5 = 7 + 3 = 10
    Instruction add2 = Instruction::MakeR(Opcode::ADD, 6, 4, 5);
    interp.ExecuteInstruction(add2);
    float r6 = interp.GetRegister(6);
    
    EXPECT_FLOAT_EQ(r3, 10.0f);
    EXPECT_FLOAT_EQ(r6, 10.0f);
    EXPECT_FLOAT_EQ(r3, r6);  // ADD should be commutative
}

// Test: SUB is not commutative
TEST_F(DirectISATest, SUB_NotCommutative) {
    Interpreter interp;
    
    interp.SetRegister(1, 10.0f);
    interp.SetRegister(2, 3.0f);
    
    Instruction sub1 = Instruction::MakeR(Opcode::SUB, 3, 1, 2);  // 10 - 3 = 7
    interp.ExecuteInstruction(sub1);
    
    // Try reverse
    interp.SetRegister(4, 3.0f);
    interp.SetRegister(5, 10.0f);
    
    Instruction sub2 = Instruction::MakeR(Opcode::SUB, 6, 4, 5);  // 3 - 10 = -7
    interp.ExecuteInstruction(sub2);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 7.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(6), -7.0f);
    EXPECT_NE(interp.GetRegister(3), interp.GetRegister(6));  // SUB not commutative
}

// Test: R0 is zero register - writes to R0 are ignored
TEST_F(DirectISATest, R0_WriteIgnored) {
    Interpreter interp;
    
    // Try to write non-zero to R0
    interp.SetRegister(0, 42.0f);
    
    // Try to modify R0 via ADD
    interp.SetRegister(1, 100.0f);
    Instruction add_to_r0 = Instruction::MakeR(Opcode::ADD, 0, 0, 1);  // R0 = R0 + R1
    interp.ExecuteInstruction(add_to_r0);
    
    // R0 should still be 0
    EXPECT_FLOAT_EQ(interp.GetRegister(0), 0.0f);
}

// Test: NOP opcode encoding
TEST_F(DirectISATest, NOP_OpcodeEncoding) {
    Instruction nop = Instruction::MakeNOP();
    EXPECT_EQ(nop.GetOpcode(), Opcode::NOP);
    EXPECT_EQ(nop.GetRd(), 0u);
    EXPECT_EQ(nop.GetRa(), 0u);
    EXPECT_EQ(nop.GetRb(), 0u);
    EXPECT_EQ(nop.GetImm(), 0u);
    
    // NOP should be 0x00000000 (opcode=0, all zeros)
    EXPECT_EQ(nop.raw, 0x00000000u);
}

// Test: ADD instruction encoding
TEST_F(DirectISATest, ADD_InstructionEncoding) {
    // ADD R3, R1, R2
    // Format-A: opcode@31..25 | rd@24..20 | ra@19..15 | rb@14..10 | 000@9..7 | reserved@6..0
    // opcode=0x01 (ADD)
    // rd=3 (0b00011), ra=1 (0b00001), rb=2 (0b00010)
    Instruction add = Instruction::MakeR(Opcode::ADD, 3, 1, 2);
    
    EXPECT_EQ(add.GetOpcode(), Opcode::ADD);
    EXPECT_EQ(add.GetRd(), 3u);
    EXPECT_EQ(add.GetRa(), 1u);
    EXPECT_EQ(add.GetRb(), 2u);
    
    // Verify raw encoding: 0x01<<25 = 0x02000000
    // rd=3<<20 = 0x00300000
    // ra=1<<15 = 0x00008000
    // rb=2<<10 = 0x00000800
    // Total = 0x02000000 | 0x00300000 | 0x00008000 | 0x00000800 = 0x02308800
    EXPECT_EQ(add.raw, 0x02308800u);
}

// Test: SUB instruction encoding
TEST_F(DirectISATest, SUB_InstructionEncoding) {
    // SUB R5, R4, R3
    // opcode=0x02 (SUB)
    Instruction sub = Instruction::MakeR(Opcode::SUB, 5, 4, 3);
    
    EXPECT_EQ(sub.GetOpcode(), Opcode::SUB);
    EXPECT_EQ(sub.GetRd(), 5u);
    EXPECT_EQ(sub.GetRa(), 4u);
    EXPECT_EQ(sub.GetRb(), 3u);
}

// Test: ADD chaining (multiple adds in sequence)
TEST_F(DirectISATest, ADD_Chained) {
    Interpreter interp;
    
    // Compute: R4 = ((R1 + R2) + R3) + R1
    interp.SetRegister(1, 1.0f);
    interp.SetRegister(2, 2.0f);
    interp.SetRegister(3, 3.0f);
    
    Instruction add1 = Instruction::MakeR(Opcode::ADD, 4, 1, 2);  // R4 = 1+2 = 3
    Instruction add2 = Instruction::MakeR(Opcode::ADD, 5, 4, 3);  // R5 = R4+3 = 3+3 = 6
    Instruction add3 = Instruction::MakeR(Opcode::ADD, 6, 5, 1);  // R6 = R5+1 = 6+1 = 7
    
    interp.ExecuteInstruction(add1);
    interp.ExecuteInstruction(add2);
    interp.ExecuteInstruction(add3);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(4), 3.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(5), 6.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 7.0f);
}

// Test: SUB chaining (arithmetic chain)
TEST_F(DirectISATest, SUB_Chained) {
    Interpreter interp;
    
    // Compute: R4 = ((10 - 3) - 2) - 1 = 4
    interp.SetRegister(1, 10.0f);
    interp.SetRegister(2, 3.0f);
    interp.SetRegister(3, 2.0f);
    interp.SetRegister(4, 1.0f);
    
    Instruction sub1 = Instruction::MakeR(Opcode::SUB, 5, 1, 2);  // R5 = 10-3 = 7
    Instruction sub2 = Instruction::MakeR(Opcode::SUB, 6, 5, 3);   // R6 = 7-2 = 5
    Instruction sub3 = Instruction::MakeR(Opcode::SUB, 7, 6, 4);   // R7 = 5-1 = 4
    
    interp.ExecuteInstruction(sub1);
    interp.ExecuteInstruction(sub2);
    interp.ExecuteInstruction(sub3);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(5), 7.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 5.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(7), 4.0f);
}

// Test: ADD + SUB mixed (arithmetic expression)
TEST_F(DirectISATest, ADD_SUB_Mixed) {
    Interpreter interp;
    
    // Compute: R4 = (R1 + R2) - (R3 + R1)
    // = (10 + 5) - (3 + 10) = 15 - 13 = 2
    interp.SetRegister(1, 10.0f);
    interp.SetRegister(2, 5.0f);
    interp.SetRegister(3, 3.0f);
    
    Instruction add1 = Instruction::MakeR(Opcode::ADD, 4, 1, 2);  // R4 = 10+5 = 15
    Instruction add2 = Instruction::MakeR(Opcode::ADD, 5, 3, 1);  // R5 = 3+10 = 13
    Instruction sub = Instruction::MakeR(Opcode::SUB, 6, 4, 5);  // R6 = 15-13 = 2
    
    interp.ExecuteInstruction(add1);
    interp.ExecuteInstruction(add2);
    interp.ExecuteInstruction(sub);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 2.0f);
}

// Test: NaN comparison (NaN != NaN by IEEE-754)
TEST_F(DirectISATest, ADD_NaNIdentity) {
    Interpreter interp;
    
    float nan1 = std::nanf("");
    float nan2 = std::nanf("");
    
    // NaN != NaN is always true (floating point comparison)
    EXPECT_NE(nan1, nan2);  // NaN != NaN
    
    interp.SetRegister(1, nan1);
    interp.SetRegister(2, nan2);
    
    Instruction add = Instruction::MakeR(Opcode::ADD, 3, 1, 2);
    interp.ExecuteInstruction(add);
    
    float result = interp.GetRegister(3);
    EXPECT_TRUE(std::isnan(result)) << "NaN + NaN should be NaN, got " << result;
}

// Test: ADD denormalized numbers
TEST_F(DirectISATest, ADD_Denormalized) {
    Interpreter interp;
    
    // Add smallest positive denormalized + smallest positive denormalized
    float smallest_denorm = std::numeric_limits<float>::denorm_min();
    interp.SetRegister(1, smallest_denorm);
    interp.SetRegister(2, smallest_denorm);
    
    Instruction add = Instruction::MakeR(Opcode::ADD, 3, 1, 2);
    interp.ExecuteInstruction(add);
    
    // 2 * denorm_min should be representable
    float result = interp.GetRegister(3);
    EXPECT_EQ(result, 2.0f * smallest_denorm);
}

// Test: SUB same registers = zero
TEST_F(DirectISATest, SUB_SameRegister) {
    Interpreter interp;
    
    interp.SetRegister(1, 123.456f);
    
    Instruction sub = Instruction::MakeR(Opcode::SUB, 2, 1, 1);  // R2 = R1 - R1 = 0
    interp.ExecuteInstruction(sub);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(2), 0.0f);
}

// Test: Coverage stats structure (manual)
TEST_F(DirectISATest, CoverageStats_Manual) {
    CoverageStats stats;
    
    // Simulate running tests covering NOP and ADD
    stats.RecordTest(static_cast<uint8_t>(Opcode::NOP), true);
    stats.RecordTest(static_cast<uint8_t>(Opcode::ADD), true);
    stats.RecordTest(static_cast<uint8_t>(Opcode::ADD), true);  // ADD covered twice
    stats.RecordTest(static_cast<uint8_t>(Opcode::SUB), true);
    stats.RecordTest(static_cast<uint8_t>(Opcode::SUB), false); // SUB failed
    
    EXPECT_EQ(stats.total_tests, 5);
    EXPECT_EQ(stats.passed_tests, 4);
    EXPECT_EQ(stats.failed_tests, 1);
    EXPECT_EQ(stats.opcode_coverage_count[static_cast<uint8_t>(Opcode::NOP)], 1);
    EXPECT_EQ(stats.opcode_coverage_count[static_cast<uint8_t>(Opcode::ADD)], 2);
    EXPECT_EQ(stats.opcode_coverage_count[static_cast<uint8_t>(Opcode::SUB)], 2);
}

// ============================================================================
// Coverage Report Test (runs last, summarizes coverage)
// ============================================================================

TEST(TestCoverageReport, P0OpcodeCoverageSummary) {
    std::vector<TestCase> cases = GetPhase1TestCases();
    
    std::cout << "\n========================================\n";
    std::cout << "Phase 1 ISA Coverage Report\n";
    std::cout << "========================================\n";
    std::cout << "Total test cases: " << cases.size() << "\n\n";
    
    auto p0_list = GetP0Opcodes();
    std::map<uint8_t, int> opcode_count;
    for (const auto& tc : cases) {
        if (!tc.program.empty()) {
            uint32_t raw = tc.program[0].raw;
            uint8_t opcode = static_cast<uint8_t>((raw >> 25) & 0x7F);
            opcode_count[opcode]++;
        }
    }
    
    std::cout << "P0 Opcode Coverage:\n";
    int covered = 0;
    for (const auto& [opcode, name] : p0_list) {
        int count = opcode_count[opcode];
        const char* mark = count > 0 ? "[OK]" : "[MISSING]";
        std::cout << "  " << mark << " " << name << " (0x" << std::hex << (int)opcode << std::dec << "): " << count << " tests\n";
        if (count > 0) covered++;
    }
    
    std::cout << "\nCoverage: " << covered << "/" << p0_list.size() << " P0 opcodes covered\n";
    std::cout << "========================================\n";
    
    // This test always passes - it's just informational
    EXPECT_TRUE(true);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
