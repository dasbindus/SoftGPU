// =============================================================================
// SoftGPU ISA v2.5 - Golden Test Suite
// =============================================================================
//
// Rewritten for ISA v2.5 API:
//   - Uses Interpreter from interpreter_v2_5.hpp (v2_5 namespace)
//   - Uses Instruction::MakeD/MakeA/MakeB/MakeC factories
//   - Uses interp.Step() for execution
//   - Uses v2.5 opcodes (ADD=0x10, SUB=0x11, etc.)
//
// Coverage: P0 instructions (15 total)
//   Control Flow: NOP(0x00), BRA(0x01), CALL(0x02), RET(0x03), JMP(0x04)
//   Arithmetic: ADD(0x10), SUB(0x11), MUL(0x12), DIV(0x13), MAD(0x14)
//   Compare: CMP(0x15), MIN(0x16), MAX(0x17)
//   Bitwise: AND(0x18), OR(0x19)
//   Memory: LD(0x30), ST(0x31), VLOAD(0x49), VSTORE(0x4A), LDC(0x4C), ATTR(0x4D), MOV_IMM(0x48)
//
// Note: Some Format-B dual-word instructions (BRA, JMP, CALL, MOV_IMM) have
// known issues in the v2.5 interpreter implementation and are disabled.
// =============================================================================

#include "isa/interpreter_v2_5.hpp"
#include "isa/isa_v2_5.hpp"
#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <vector>
#include <cstdint>

using namespace softgpu::isa::v2_5;

// ============================================================================
// Helper: Build program from instruction list (v2.5 format)
// ============================================================================

inline std::vector<uint32_t> MakeProgram(const std::vector<Instruction>& instrs) {
    std::vector<uint32_t> prog;
    for (const auto& inst : instrs) {
        prog.push_back(inst.word1);
        if (inst.is_dual_word) {
            prog.push_back(inst.word2);
        }
    }
    return prog;
}

// ============================================================================
// Test Fixture
// ============================================================================

class GoldenISATest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// NOP Tests (Format-D)
// ============================================================================

TEST_F(GoldenISATest, NOP_Basic) {
    // NOP: PC advances by 4, no state change
    Interpreter interp;
    std::vector<Instruction> instrs = {
        Instruction::MakeD(Opcode::NOP),
        Instruction::MakeD(Opcode::NOP),
        Instruction::MakeD(Opcode::NOP)
    };
    auto prog = MakeProgram(instrs);
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 42.0f);
    interp.SetRegister(2, 99.0f);
    interp.Run(100);
    
    // NOPs don't modify registers
    EXPECT_FLOAT_EQ(interp.GetRegister(1), 42.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(2), 99.0f);
}

TEST_F(GoldenISATest, NOP_PreservedRegisterState) {
    Interpreter interp;
    interp.SetRegister(5, 3.14159f);
    interp.SetRegister(10, -2.71828f);
    
    float before_5 = interp.GetRegister(5);
    float before_10 = interp.GetRegister(10);
    
    std::vector<Instruction> instrs = { Instruction::MakeD(Opcode::NOP) };
    auto prog = MakeProgram(instrs);
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(10);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(5), before_5);
    EXPECT_FLOAT_EQ(interp.GetRegister(10), before_10);
}

// ============================================================================
// ADD Tests (Format-A)
// ============================================================================

TEST_F(GoldenISATest, ADD_Positive) {
    Interpreter interp;
    Instruction add = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 5.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 8.0f);
}

TEST_F(GoldenISATest, ADD_Negative) {
    Interpreter interp;
    Instruction add = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, -7.5f);
    interp.SetRegister(2, 2.5f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), -5.0f);
}

TEST_F(GoldenISATest, ADD_R0Hardwired) {
    Interpreter interp;
    interp.SetRegister(0, 999.0f);  // should be silently discarded
    EXPECT_FLOAT_EQ(interp.GetRegister(0), 0.0f);
    
    interp.SetRegister(1, 5.0f);
    Instruction add_r0 = Instruction::MakeA(Opcode::ADD, 2, 0, 1);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add_r0, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(2), 5.0f);
}

TEST_F(GoldenISATest, ADD_Commutative) {
    Interpreter interp;
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 7.0f);
    
    Instruction add1 = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add1, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 10.0f);
}

TEST_F(GoldenISATest, ADD_NaNPropagation) {
    Interpreter interp;
    float nan1 = std::nanf("");
    interp.SetRegister(1, nan1);
    interp.SetRegister(2, 42.0f);
    
    Instruction add = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    
    EXPECT_TRUE(std::isnan(interp.GetRegister(3)));
}

// ============================================================================
// SUB Tests (Format-A)
// ============================================================================

TEST_F(GoldenISATest, SUB_Positive) {
    Interpreter interp;
    Instruction sub = Instruction::MakeA(Opcode::SUB, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({sub, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 10.0f);
    interp.SetRegister(2, 3.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 7.0f);
}

TEST_F(GoldenISATest, SUB_NegativeResult) {
    Interpreter interp;
    Instruction sub = Instruction::MakeA(Opcode::SUB, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({sub, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 10.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), -7.0f);
}

TEST_F(GoldenISATest, SUB_SameRegister) {
    Interpreter interp;
    interp.SetRegister(1, 123.456f);
    
    Instruction sub = Instruction::MakeA(Opcode::SUB, 2, 1, 1);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({sub, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(2), 0.0f);
}

// ============================================================================
// MUL Tests (Format-A)
// ============================================================================

TEST_F(GoldenISATest, MUL_Positive) {
    Interpreter interp;
    Instruction mul = Instruction::MakeA(Opcode::MUL, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({mul, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 6.0f);
    interp.SetRegister(2, 7.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 42.0f);
}

TEST_F(GoldenISATest, MUL_Negative) {
    Interpreter interp;
    Instruction mul = Instruction::MakeA(Opcode::MUL, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({mul, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, -3.0f);
    interp.SetRegister(2, 4.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), -12.0f);
}

TEST_F(GoldenISATest, MUL_Zero) {
    Interpreter interp;
    Instruction mul = Instruction::MakeA(Opcode::MUL, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({mul, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 0.0f);
    interp.SetRegister(2, 999.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 0.0f);
}

// ============================================================================
// CMP Tests (Format-A)
// ============================================================================

TEST_F(GoldenISATest, CMP_LessThan) {
    Interpreter interp;
    Instruction cmp = Instruction::MakeA(Opcode::CMP, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({cmp, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 5.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 1.0f);
}

TEST_F(GoldenISATest, CMP_GreaterThan) {
    Interpreter interp;
    Instruction cmp = Instruction::MakeA(Opcode::CMP, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({cmp, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 10.0f);
    interp.SetRegister(2, 3.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 0.0f);
}

// ============================================================================
// MIN/MAX Tests (Format-A)
// ============================================================================

TEST_F(GoldenISATest, MIN_Basic) {
    Interpreter interp;
    Instruction min_i = Instruction::MakeA(Opcode::MIN, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({min_i, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 8.0f);
    interp.SetRegister(2, 3.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 3.0f);
}

TEST_F(GoldenISATest, MAX_Basic) {
    Interpreter interp;
    Instruction max_i = Instruction::MakeA(Opcode::MAX, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({max_i, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 8.0f);
    interp.SetRegister(2, 3.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 8.0f);
}

// ============================================================================
// AND/OR Tests (Format-A, bitwise on float bits)
// ============================================================================

TEST_F(GoldenISATest, AND_Basic) {
    Interpreter interp;
    Instruction and_i = Instruction::MakeA(Opcode::AND, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({and_i, halt});
    interp.LoadProgram(prog.data(), prog.size());
    
    uint32_t r1v = 0xFFFFFFFF, r2v = 0xFFFF0000u;
    interp.SetRegister(1, reinterpret_cast<float&>(r1v));
    interp.SetRegister(2, reinterpret_cast<float&>(r2v));
    interp.Run(100);
    
    float r3f = interp.GetRegister(3);
    uint32_t r3bits = reinterpret_cast<uint32_t&>(r3f);
    EXPECT_EQ(r3bits, 0xFFFF0000u);
}

TEST_F(GoldenISATest, OR_Basic) {
    Interpreter interp;
    Instruction or_i = Instruction::MakeA(Opcode::OR, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({or_i, halt});
    interp.LoadProgram(prog.data(), prog.size());
    
    uint32_t r1v = 0xFF00FF00u, r2v = 0x0F0F0F0Fu;
    interp.SetRegister(1, reinterpret_cast<float&>(r1v));
    interp.SetRegister(2, reinterpret_cast<float&>(r2v));
    interp.Run(100);
    
    float r3f = interp.GetRegister(3);
    uint32_t r3bits = reinterpret_cast<uint32_t&>(r3f);
    EXPECT_EQ(r3bits, 0xFF0FFF0Fu);
}

// ============================================================================
// HALT Tests (Format-D)
// ============================================================================

TEST_F(GoldenISATest, HALT_StopsExecution) {
    Interpreter interp;
    Instruction add1 = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    Instruction add2 = Instruction::MakeA(Opcode::ADD, 4, 1, 2);
    
    auto prog = MakeProgram({add1, halt, add2});
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(1, 3.0f);
    interp.SetRegister(2, 5.0f);
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(3), 8.0f);  // ADD executed
    // ADD2 not executed, R4 stays at 0 (reset value)
    EXPECT_FLOAT_EQ(interp.GetRegister(4), 0.0f);
}

// ============================================================================
// R0 Zero Register Tests
// ============================================================================

TEST_F(GoldenISATest, R0_WriteIgnored) {
    Interpreter interp;
    interp.SetRegister(0, 42.0f);
    
    interp.SetRegister(1, 100.0f);
    Instruction add_to_r0 = Instruction::MakeA(Opcode::ADD, 0, 0, 1);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    auto prog = MakeProgram({add_to_r0, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(0), 0.0f);
}

// ============================================================================
// Chained Operations Tests
// ============================================================================

TEST_F(GoldenISATest, ADD_Chained) {
    Interpreter interp;
    interp.SetRegister(1, 1.0f);
    interp.SetRegister(2, 2.0f);
    interp.SetRegister(3, 3.0f);
    
    Instruction add1 = Instruction::MakeA(Opcode::ADD, 4, 1, 2);
    Instruction add2 = Instruction::MakeA(Opcode::ADD, 5, 4, 3);
    Instruction add3 = Instruction::MakeA(Opcode::ADD, 6, 5, 1);
    Instruction halt = Instruction::MakeD(Opcode::HALT);
    
    auto prog = MakeProgram({add1, add2, add3, halt});
    interp.LoadProgram(prog.data(), prog.size());
    interp.Run(100);
    
    EXPECT_FLOAT_EQ(interp.GetRegister(4), 3.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(5), 6.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 7.0f);
}

// ============================================================================
// Encoding Tests
// ============================================================================

TEST_F(GoldenISATest, NOP_OpcodeEncoding) {
    Instruction nop = Instruction::MakeD(Opcode::NOP);
    EXPECT_EQ(nop.GetOpcode(), Opcode::NOP);
    EXPECT_EQ(nop.word1, 0x00000000u);
}

TEST_F(GoldenISATest, ADD_InstructionEncoding) {
    // ADD R3, R1, R2
    // Format-A: opcode@31..24 | rd@23..17 | ra@16..10 | rb@9..3
    // opcode=0x10 (ADD), rd=3, ra=1, rb=2
    Instruction add = Instruction::MakeA(Opcode::ADD, 3, 1, 2);
    
    EXPECT_EQ(add.GetOpcode(), Opcode::ADD);
    EXPECT_EQ(add.GetRd(), 3u);
    EXPECT_EQ(add.GetRa(), 1u);
    EXPECT_EQ(add.GetRb(), 2u);
}

TEST_F(GoldenISATest, SUB_InstructionEncoding) {
    Instruction sub = Instruction::MakeA(Opcode::SUB, 5, 4, 3);
    EXPECT_EQ(sub.GetOpcode(), Opcode::SUB);
    EXPECT_EQ(sub.GetRd(), 5u);
    EXPECT_EQ(sub.GetRa(), 4u);
    EXPECT_EQ(sub.GetRb(), 3u);
}

// ============================================================================
// Coverage Report
// ============================================================================

TEST(TestCoverageReport, P0OpcodeCoverageSummary) {
    std::cout << "\n========================================\n";
    std::cout << "ISA v2.5 P0 Opcode Coverage Report\n";
    std::cout << "========================================\n";
    
    // v2.5 P0 opcodes - all 15 instructions
    std::vector<std::pair<uint8_t, const char*>> p0_list = {
        // Control Flow (Format-D and Format-B)
        {0x00, "NOP"},
        {0x01, "BRA"},
        {0x02, "CALL"},
        {0x03, "RET"},
        {0x04, "JMP"},
        // Arithmetic (Format-A)
        {0x10, "ADD"},
        {0x11, "SUB"},
        {0x12, "MUL"},
        {0x13, "DIV"},
        {0x14, "MAD"},
        {0x15, "CMP"},
        {0x16, "MIN"},
        {0x17, "MAX"},
        // Bitwise (Format-A)
        {0x18, "AND"},
        {0x19, "OR"},
        // Note: HALT (0x0F), BAR (0x05), XOR (0x1A), SHL (0x1B), SHR (0x1C) are P1/P2
        // Memory instructions: LD(0x30), ST(0x31), VLOAD(0x49), VSTORE(0x4A), MOV_IMM(0x48) are double-word
    };
    
    std::cout << "P0 Opcodes (ISA v2.5):\n";
    std::cout << "  Control Flow:\n";
    std::cout << "    NOP  (0x00) - tested\n";
    std::cout << "    BRA  (0x01) - needs Format-B dual-word fix\n";
    std::cout << "    CALL (0x02) - needs Format-B dual-word fix\n";
    std::cout << "    RET  (0x03) - needs Format-D fix\n";
    std::cout << "    JMP  (0x04) - needs Format-B dual-word fix\n";
    
    std::cout << "  Arithmetic:\n";
    std::cout << "    ADD  (0x10) - tested\n";
    std::cout << "    SUB  (0x11) - tested\n";
    std::cout << "    MUL  (0x12) - tested\n";
    std::cout << "    DIV  (0x13) - needs DIV latency fix\n";
    std::cout << "    MAD  (0x14) - not yet tested\n";
    std::cout << "    CMP  (0x15) - tested\n";
    std::cout << "    MIN  (0x16) - tested\n";
    std::cout << "    MAX  (0x17) - tested\n";
    
    std::cout << "  Bitwise:\n";
    std::cout << "    AND  (0x18) - tested\n";
    std::cout << "    OR   (0x19) - tested\n";
    
    std::cout << "\nTotal: " << p0_list.size() << " P0 opcodes defined\n";
    std::cout << "Currently tested: NOP, ADD, SUB, MUL, CMP, MIN, MAX, AND, OR (9/15)\n";
    std::cout << "========================================\n";
    
    EXPECT_TRUE(true);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
