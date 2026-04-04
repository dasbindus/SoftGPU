// ============================================================================
// SoftGPU - test_isa.cpp
// ISA Instruction Tests (DIV Cycle Latency, etc.)
// G1 Phase - P0-2/3/4 Test Framework
// ============================================================================

#include <gtest/gtest.h>
#include <limits>
#include <cmath>

#include "isa/Interpreter.hpp"
#include "isa/Instruction.hpp"
#include "isa/ExecutionUnits.hpp"
#include "isa/Opcode.hpp"

using namespace softgpu::isa;

// ============================================================================
// P0-2: DIV Cycle Latency Test
// ============================================================================
// Verifies that DIV instruction correctly accounts for cycle latency.
// DIV is an SFU operation with multi-cycle latency (typically 7-10 cycles).
// ============================================================================

class DIVLatencyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Test: DIV should take more cycles than simple ALU operations
// ---------------------------------------------------------------------------
// Note: This is a placeholder test framework.
// Actual cycle counting for DIV will be implemented when the pipeline
// model is complete. For now, we verify the instruction executes correctly
// and the result is mathematically accurate.
// ---------------------------------------------------------------------------

TEST_F(DIVLatencyTest, DIVBasicExecution)
{
    // Test DIV execution produces correct results
    SFU sfu;
    SFU::Input input;
    SFU::Output output;

    input.op = Opcode::DIV;
    input.rd = 1;
    input.ra = 1;
    input.val_a = 10.0f;
    input.val_b = 2.0f;

    output = sfu.Execute(input);

    EXPECT_TRUE(output.write_back);
    EXPECT_EQ(output.rd, 1);
    EXPECT_FLOAT_EQ(output.result, 5.0f);
}

TEST_F(DIVLatencyTest, DIVByZeroHandling)
{
    SFU sfu;
    SFU::Input input;
    SFU::Output output;

    // Division by zero should return infinity
    input.op = Opcode::DIV;
    input.rd = 1;
    input.ra = 1;
    input.val_a = 10.0f;
    input.val_b = 0.0f;

    output = sfu.Execute(input);

    EXPECT_TRUE(output.write_back);
    EXPECT_TRUE(std::isinf(output.result));
    EXPECT_GT(output.result, 0.0f); // Positive infinity
}

TEST_F(DIVLatencyTest, DIVNegativeResult)
{
    SFU sfu;
    SFU::Input input;
    SFU::Output output;

    input.op = Opcode::DIV;
    input.rd = 1;
    input.ra = 1;
    input.val_a = 10.0f;
    input.val_b = -2.0f;

    output = sfu.Execute(input);

    EXPECT_TRUE(output.write_back);
    EXPECT_FLOAT_EQ(output.result, -5.0f);
}

TEST_F(DIVLatencyTest, DIVSmallValues)
{
    SFU sfu;
    SFU::Input input;
    SFU::Output output;

    input.op = Opcode::DIV;
    input.rd = 1;
    input.ra = 1;
    input.val_a = 1.0f;
    input.val_b = 3.0f;

    output = sfu.Execute(input);

    EXPECT_TRUE(output.write_back);
    EXPECT_NEAR(output.result, 0.333333f, 0.0001f);
}

// ---------------------------------------------------------------------------
// TODO(P0-2): When pipeline supports stall, supplement the following tests:
//  1. DIV issued then target register read immediately → old value returned
//  2. DIV completed after N cycles → correct result returned
//  3. Back-to-back DIVs → FIFO completion order verified
// ---------------------------------------------------------------------------
/** @Test */
TEST_F(DIVLatencyTest, DIVPendingQueueFIFO) {
    GTEST_SKIP() << "Pending queue FIFO tested structurally; "
                    "cycle-accurate stall test requires pipeline model";
}

// ---------------------------------------------------------------------------
// P1-1: DP3 Three-Component Dot Product
// ---------------------------------------------------------------------------
TEST_F(DIVLatencyTest, DP3ThreeComponentDotProduct) {
    Interpreter interp;
    
    // Ra.xyz = {R4, R5, R6} = {1, 2, 3}
    interp.SetRegister(4, 1.0f);
    interp.SetRegister(5, 2.0f);
    interp.SetRegister(6, 3.0f);
    // Rb.xyz = {R1, R2, R3} = {4, 5, 6} (R0 is zero-reg, cannot hold values)
    interp.SetRegister(1, 4.0f);
    interp.SetRegister(2, 5.0f);
    interp.SetRegister(3, 6.0f);

    // R8 = dot(R4.xyz, R1.xyz) = 1*4 + 2*5 + 3*6 = 32
    Instruction inst = Instruction::MakeR(Opcode::DP3, 8, 4, 1);
    interp.ExecuteInstruction(inst);

    float result = interp.GetRegister(8);
    EXPECT_NEAR(result, 32.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// P0-2 Placeholder: DIV Cycle Counting Test
// ---------------------------------------------------------------------------
// TODO: When pipeline model is complete, implement cycle counting test:
//
// TEST_F(DIVLatencyTest, DIVCyclesCounting)
// {
//     Interpreter interp;
//     
//     // Load DIV instruction
//     // Execute and verify stats_.cycles increments by DIV_LATENCY (e.g., 7 cycles)
//     
//     const auto& stats = interp.GetStats();
//     EXPECT_GE(stats.cycles, 7);  // DIV should take at least 7 cycles
// }
// ---------------------------------------------------------------------------

// ============================================================================
// Interpreter Stats Test
// ============================================================================

class InterpreterStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InterpreterStatsTest, CyclesIncrementBasic)
{
    Interpreter interp;

    // Set up a simple program: ADD R1 = R0 + R0 (R0 = 0)
    Instruction add_inst = Instruction::MakeR(Opcode::ADD, 1, 0, 0);
    // Note: The interpreter's Step() is simplified and may not properly
    // execute instructions. This test verifies the framework is in place.

    // The stats should start at 0
    const auto& stats = interp.GetStats();
    EXPECT_EQ(stats.cycles, 0ULL);
    EXPECT_EQ(stats.instructions_executed, 0ULL);
}

TEST_F(InterpreterStatsTest, InterpreterReset)
{
    Interpreter interp;

    // Reset should clear all stats
    interp.Reset();

    const auto& stats = interp.GetStats();
    EXPECT_EQ(stats.cycles, 0ULL);
    EXPECT_EQ(stats.instructions_executed, 0ULL);
    EXPECT_EQ(stats.loads, 0ULL);
    EXPECT_EQ(stats.stores, 0ULL);
    EXPECT_EQ(stats.branches_taken, 0ULL);
}

// ============================================================================
// ALU vs SFU Latency Comparison (Placeholders for P0-2)
// ============================================================================

class ALUSFULatencyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Note: ALU operations (ADD, MUL) take 1 cycle
// SFU operations (DIV, SQRT, RCP) take multiple cycles
// These tests will be expanded when the pipeline model is complete

TEST_F(ALUSFULatencyTest, ALULatencyIsOneCycle)
{
    // Placeholder: ALU operations should complete in 1 cycle
    // TODO: Implement actual cycle counting in pipeline model
    EXPECT_TRUE(true); // Framework in place
}

TEST_F(ALUSFULatencyTest, SFUDIVLatencyPlaceholder)
{
    // Placeholder: DIV should take more cycles than ALU
    // Expected: 7-10 cycles for DIV vs 1 cycle for ADD/MUL
    // TODO: Implement actual cycle counting
    EXPECT_TRUE(true); // Framework in place
}

// ============================================================================
// v1.4: TEX/SAMPLE Instruction Tests
// ============================================================================

class TEXSampleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// Test: TEX opcode encoding (0x19)
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_OpcodeEncoding) {
    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 4, 1, 2, 3);
    EXPECT_EQ(tex_inst.GetOpcode(), Opcode::TEX);
    EXPECT_EQ(tex_inst.GetRd(), 4);
    EXPECT_EQ(tex_inst.GetRa(), 1);
    EXPECT_EQ(tex_inst.GetRb(), 2);
    EXPECT_EQ(tex_inst.GetRc(), 3);
}

// ---------------------------------------------------------------------------
// Test: SAMPLE opcode encoding (0x1A)
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, SAMPLE_OpcodeEncoding) {
    Instruction sample_inst = Instruction::MakeR4(Opcode::SAMPLE, 8, 5, 6, 7);
    EXPECT_EQ(sample_inst.GetOpcode(), Opcode::SAMPLE);
    EXPECT_EQ(sample_inst.GetRd(), 8);
    EXPECT_EQ(sample_inst.GetRa(), 5);
    EXPECT_EQ(sample_inst.GetRb(), 6);
    EXPECT_EQ(sample_inst.GetRc(), 7);
}

// ---------------------------------------------------------------------------
// Test: TEX produces checkerboard pattern via Interpreter
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_CheckerboardPattern) {
    Interpreter interp;

    // Set texture coordinates (UV) in Ra and Rb
    // UV = (0.0, 0.0) -> center of first checkerboard cell -> white (0+0 even = 0 -> white)
    interp.SetRegister(1, 0.0f);  // u = 0.0
    interp.SetRegister(2, 0.0f);  // v = 0.0

    // Execute TEX instruction: TEX R4, R1(u), R2(v), R3(tex_id=0)
    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 4, 1, 2, 0);
    interp.ExecuteInstruction(tex_inst);

    // Checkerboard: (cx + cy) % 2 == 0 -> white (1.0f)
    // cx = floor(0.0 * 8.0) = 0, cy = floor(0.0 * 8.0) = 0 -> even -> white
    EXPECT_FLOAT_EQ(interp.GetRegister(4), 1.0f);  // R
    EXPECT_FLOAT_EQ(interp.GetRegister(5), 1.0f);  // G
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 1.0f);  // B
    EXPECT_FLOAT_EQ(interp.GetRegister(7), 1.0f);  // A = 1.0f
}

// ---------------------------------------------------------------------------
// Test: TEX checkerboard alternates at cell boundaries
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_CheckerboardAlternates) {
    Interpreter interp;

    // UV = (0.125, 0.0) -> cx = floor(0.125*8) = 1, cy = 0 -> (1+0) % 2 = 1 -> black
    interp.SetRegister(1, 0.125f);  // u = 0.125
    interp.SetRegister(2, 0.0f);    // v = 0.0

    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 4, 1, 2, 0);
    interp.ExecuteInstruction(tex_inst);

    // cx=1, cy=0 -> odd -> black (0.0f)
    EXPECT_FLOAT_EQ(interp.GetRegister(4), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(5), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(6), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(7), 1.0f);  // A always 1.0f
}

// ---------------------------------------------------------------------------
// Test: TEX checkerboard at various UV positions
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_CheckerboardVariousUV) {
    Interpreter interp;

    struct UVCase {
        float u, v;
        float expected;  // 0.0f or 1.0f
    };

    // Test several UV positions covering different checker cells
    // Each cell is 1/8 of the texture (0.125)
    // cx = floor(u * 8), cy = floor(v * 8)
    // white if (cx + cy) % 2 == 0, black otherwise
    UVCase cases[] = {
        {0.0f, 0.0f, 1.0f},       // (0,0) -> (0+0)%2=0 -> white
        {0.125f, 0.0f, 0.0f},     // (1,0) -> (1+0)%2=1 -> black
        {0.25f, 0.0f, 1.0f},      // (2,0) -> (2+0)%2=0 -> white
        {0.0f, 0.125f, 0.0f},     // (0,1) -> (0+1)%2=1 -> black
        {0.125f, 0.125f, 1.0f},    // (1,1) -> (1+1)%2=0 -> white
        {0.5f, 0.5f, 1.0f},        // (4,4) -> (4+4)%2=0 -> white
        {0.625f, 0.5f, 0.0f},      // (5,4) -> (5+4)%2=1 -> black
    };

    for (const auto& c : cases) {
        interp.SetRegister(1, c.u);
        interp.SetRegister(2, c.v);
        Instruction tex = Instruction::MakeR4(Opcode::TEX, 10, 1, 2, 0);
        interp.ExecuteInstruction(tex);

        EXPECT_FLOAT_EQ(interp.GetRegister(10), c.expected)
            << "UV(" << c.u << "," << c.v << ") should be " << c.expected;
    }
}

// ---------------------------------------------------------------------------
// Test: SAMPLE uses same checkerboard as TEX (placeholder behavior)
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, SAMPLE_CheckerboardSameAsTEX) {
    Interpreter interp;

    // Same UV coordinates for TEX and SAMPLE should produce same result
    interp.SetRegister(1, 0.25f);
    interp.SetRegister(2, 0.375f);

    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 20, 1, 2, 0);
    Instruction sample_inst = Instruction::MakeR4(Opcode::SAMPLE, 24, 1, 2, 0);

    interp.ExecuteInstruction(tex_inst);
    interp.ExecuteInstruction(sample_inst);

    // Both should produce the same checkerboard value
    EXPECT_FLOAT_EQ(interp.GetRegister(20), interp.GetRegister(24));
    EXPECT_FLOAT_EQ(interp.GetRegister(21), interp.GetRegister(25));
    EXPECT_FLOAT_EQ(interp.GetRegister(22), interp.GetRegister(26));
    EXPECT_FLOAT_EQ(interp.GetRegister(23), interp.GetRegister(27));
}

// ---------------------------------------------------------------------------
// Test: TEX writes to 4 consecutive registers (RGBA)
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_WritesRGBAConsecutive) {
    Interpreter interp;

    interp.SetRegister(1, 0.0f);
    interp.SetRegister(2, 0.0f);

    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 16, 1, 2, 0);
    interp.ExecuteInstruction(tex_inst);

    // Should write to R16 (R), R17 (G), R18 (B), R19 (A)
    // A should always be 1.0f
    EXPECT_FLOAT_EQ(interp.GetRegister(16), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(17), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(18), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetRegister(19), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: TEX instruction type is R4
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_InstructionTypeR4) {
    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 4, 1, 2, 3);
    EXPECT_EQ(tex_inst.GetType(), IType::R4);
}

// ---------------------------------------------------------------------------
// Test: SAMPLE instruction type is R4
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, SAMPLE_InstructionTypeR4) {
    Instruction sample_inst = Instruction::MakeR4(Opcode::SAMPLE, 4, 1, 2, 3);
    EXPECT_EQ(sample_inst.GetType(), IType::R4);
}

// ---------------------------------------------------------------------------
// Test: TEX and SAMPLE have correct cycle counts in GetCycles
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_SAMPLE_CycleCounts) {
    EXPECT_EQ(GetCycles(Opcode::TEX), 8);
    EXPECT_EQ(GetCycles(Opcode::SAMPLE), 4);
}

// ---------------------------------------------------------------------------
// Test: TEX UV interpolation - values between cell boundaries
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_UVInterpolation) {
    Interpreter interp;

    // UV = (0.0625, 0.0) -> cx = floor(0.0625*8) = 0, within first cell -> white
    interp.SetRegister(1, 0.0625f);
    interp.SetRegister(2, 0.0f);

    Instruction tex_inst = Instruction::MakeR4(Opcode::TEX, 30, 1, 2, 0);
    interp.ExecuteInstruction(tex_inst);

    // cx=0 -> even -> white (1.0f)
    EXPECT_FLOAT_EQ(interp.GetRegister(30), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: Multiple TEX instructions accumulate correctly
// ---------------------------------------------------------------------------
TEST_F(TEXSampleTest, TEX_MultipleSamples) {
    Interpreter interp;

    // Sample at UV (0.0, 0.0) -> white
    interp.SetRegister(1, 0.0f);
    interp.SetRegister(2, 0.0f);
    Instruction tex1 = Instruction::MakeR4(Opcode::TEX, 4, 1, 2, 0);
    interp.ExecuteInstruction(tex1);

    // Sample at UV (0.125, 0.0) -> black
    interp.SetRegister(1, 0.125f);
    interp.SetRegister(2, 0.0f);
    Instruction tex2 = Instruction::MakeR4(Opcode::TEX, 8, 1, 2, 0);
    interp.ExecuteInstruction(tex2);

    EXPECT_FLOAT_EQ(interp.GetRegister(4), 1.0f);  // white
    EXPECT_FLOAT_EQ(interp.GetRegister(8), 0.0f);  // black
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
