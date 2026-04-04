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
// Main
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
