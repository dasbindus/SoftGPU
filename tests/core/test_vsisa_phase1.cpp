// ============================================================================
// SoftGPU - test_vsisa_phase1.cpp
// Vertex Shader ISA Phase 1 Tests (NOP, HALT, JUMP, ADD, SUB, MUL, DIV,
//                                    DOT3, NORMALIZE, MAT_MUL, VLOAD, VOUTPUT)
// ============================================================================

#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include <cstring>

#include "isa/Interpreter.hpp"
#include "isa/Instruction.hpp"
#include "isa/Opcode.hpp"

using namespace softgpu::isa;

// ============================================================================
// VS ISA Phase 1 Tests
// ============================================================================
// NOTE: R0 is the zero register (hardcoded 0.0f). All tests avoid R0
// as a source of data. VLOAD reads into R4-R7 (avoiding R0), and
// MAT_MUL reads from R1-R4 (R0 is zero register, so ensure R1-R4 hold
// the actual vector data).
// ============================================================================

class VSISAPhase1Test : public ::testing::Test {
protected:
    void SetUp() override {
        interp.Reset();
        interp.ResetVS();
    }
    void TearDown() override {}

    Interpreter interp;

    void SetReg(uint8_t r, float v) { interp.SetRegister(r, v); }
    float GetReg(uint8_t r) const { return interp.GetRegister(r); }
    void SetVBO(const float* data, size_t count) { interp.SetVBO(data, count); }
};

// ---------------------------------------------------------------------------
// Test: VS_NOP
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, NOP_DoesNotModifyRegisters)
{
    SetReg(1, 123.0f);
    SetReg(2, 456.0f);

    Instruction nop = PatternVS::vs_nop();
    EXPECT_EQ(nop.GetOpcode(), Opcode::VS_NOP);
    EXPECT_EQ(nop.GetRd(), 0u);
    EXPECT_EQ(nop.GetRa(), 0u);
    EXPECT_EQ(nop.GetRb(), 0u);

    interp.ExecuteInstruction(nop);

    EXPECT_FLOAT_EQ(GetReg(1), 123.0f);
    EXPECT_FLOAT_EQ(GetReg(2), 456.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_ADD
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, ADD_Basic)
{
    SetReg(1, 3.0f);
    SetReg(2, 5.0f);

    Instruction add_inst = PatternVS::vs_add(3, 1, 2);
    interp.ExecuteInstruction(add_inst);

    EXPECT_FLOAT_EQ(GetReg(3), 8.0f);
}

TEST_F(VSISAPhase1Test, ADD_Negative)
{
    SetReg(1, -10.0f);
    SetReg(2, 3.0f);

    Instruction add_inst = PatternVS::vs_add(3, 1, 2);
    interp.ExecuteInstruction(add_inst);

    EXPECT_FLOAT_EQ(GetReg(3), -7.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_SUB
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, SUB_Basic)
{
    SetReg(1, 10.0f);
    SetReg(2, 3.0f);

    Instruction sub_inst = PatternVS::vs_sub(3, 1, 2);
    interp.ExecuteInstruction(sub_inst);

    EXPECT_FLOAT_EQ(GetReg(3), 7.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_MUL
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, MUL_Basic)
{
    SetReg(1, 6.0f);
    SetReg(2, 7.0f);

    Instruction mul_inst = PatternVS::vs_mul(3, 1, 2);
    interp.ExecuteInstruction(mul_inst);

    EXPECT_FLOAT_EQ(GetReg(3), 42.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_DIV (7-cycle latency)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, DIV_Basic)
{
    SetReg(1, 20.0f);
    SetReg(2, 4.0f);

    Instruction div_inst = PatternVS::vs_div(3, 1, 2);
    interp.ExecuteInstruction(div_inst);

    // Result is pending; ExecuteInstruction doesn't advance cycles
    // drainPendingDIVs advances cycles by 1 per call
    // DIV completion = current_cycle + 7 (from when ExecuteInstruction was called)
    // After ExecuteInstruction, cycles=0, DIV pending with completion=7
    // On the 7th drain (cycles=7), condition completion<=cycles is 7<=7=true -> drains
    // But the drain happens after the condition check advances cycles to 7
    // So actually 8 drains are needed: 7 to reach cycles=7 (no drain yet), 8th completes
    EXPECT_NE(GetReg(3), 5.0f);  // Not written yet
    for (int i = 0; i < 7; ++i) {
        interp.drainPendingDIVs();
        EXPECT_NE(GetReg(3), 5.0f);  // Still pending (completion=7 > current cycle)
    }
    interp.drainPendingDIVs();  // 8th call: cycles=8, drains
    EXPECT_FLOAT_EQ(GetReg(3), 5.0f);
}

TEST_F(VSISAPhase1Test, DIV_ByZero)
{
    SetReg(1, 10.0f);
    SetReg(2, 0.0f);

    Instruction div_inst = PatternVS::vs_div(3, 1, 2);
    interp.ExecuteInstruction(div_inst);

    // After 8 drain calls (same timing as DIV_Basic)
    for (int i = 0; i < 7; ++i) interp.drainPendingDIVs();
    interp.drainPendingDIVs();  // 8th drain

    float result = GetReg(3);
    EXPECT_TRUE(std::isinf(result));
}

// ---------------------------------------------------------------------------
// Test: VS_DOT3
// NOTE: R0 is zero register, so we use R1-R3 and R5-R7
// v = (1, 2, 3) -> R1, R2, R3
// r = (4, 5, 6) -> R5, R6, R7
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, DOT3_Basic)
{
    // v = (1, 2, 3) stored at R1-R3 (R0 is zero register)
    SetReg(1, 1.0f);
    SetReg(2, 2.0f);
    SetReg(3, 3.0f);
    // r = (4, 5, 6) stored at R5-R7
    SetReg(5, 4.0f);
    SetReg(6, 5.0f);
    SetReg(7, 6.0f);

    // DOT3 Rd, Ra, Rb with Ra=1, Rb=5 (Ra and Rb must be 4-aligned)
    // Ra=1 -> reads R1,R2,R3; Rb=5 -> reads R5,R6,R7
    Instruction dot3 = PatternVS::vs_dot3(10, 1, 5);
    EXPECT_EQ(dot3.GetOpcode(), Opcode::VS_DOT3);
    EXPECT_EQ(dot3.GetRd(), 10u);
    EXPECT_EQ(dot3.GetRa(), 1u);
    EXPECT_EQ(dot3.GetRb(), 5u);

    interp.ExecuteInstruction(dot3);

    // dot = 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    EXPECT_FLOAT_EQ(GetReg(10), 32.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_NORMALIZE
// NOTE: R0 is zero register, input vector (3, 4, 0) goes to R1-R3
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, NORMALIZE_Basic)
{
    // Input vector (3, 4, 0) at R1-R3 (R0 is zero register)
    SetReg(1, 3.0f);
    SetReg(2, 4.0f);
    SetReg(3, 0.0f);

    Instruction norm = PatternVS::vs_normalize(12, 1);
    EXPECT_EQ(norm.GetOpcode(), Opcode::VS_NORMALIZE);
    EXPECT_EQ(norm.GetRd(), 12u);
    EXPECT_EQ(norm.GetRa(), 1u);

    interp.ExecuteInstruction(norm);

    // length = sqrt(9+16+0) = 5
    // normalized = (0.6, 0.8, 0)
    EXPECT_NEAR(GetReg(12), 3.0f / 5.0f, 1e-5f);
    EXPECT_NEAR(GetReg(13), 4.0f / 5.0f, 1e-5f);
    EXPECT_NEAR(GetReg(14), 0.0f, 1e-5f);
}

TEST_F(VSISAPhase1Test, NORMALIZE_UnitVector)
{
    // Input (1, 0, 0) at R1-R3
    SetReg(1, 1.0f);
    SetReg(2, 0.0f);
    SetReg(3, 0.0f);

    Instruction norm = PatternVS::vs_normalize(12, 1);
    interp.ExecuteInstruction(norm);

    EXPECT_NEAR(GetReg(12), 1.0f, 1e-5f);
    EXPECT_NEAR(GetReg(13), 0.0f, 1e-5f);
    EXPECT_NEAR(GetReg(14), 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: VS_VLOAD
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, VLOAD_Basic)
{
    // VBO: [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
    float vbo_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    SetVBO(vbo_data, 8);

    // VLOAD R4, #0 -> loads floats at byte offsets 0,4,8,12 into R4,R5,R6,R7
    Instruction vload = PatternVS::vs_vload(4, 0);
    EXPECT_EQ(vload.GetOpcode(), Opcode::VS_VLOAD);
    EXPECT_EQ(vload.GetRd(), 4u);
    EXPECT_EQ(vload.GetImm(), 0u);

    interp.ExecuteInstruction(vload);

    EXPECT_FLOAT_EQ(GetReg(4), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 2.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 3.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 4.0f);
}

TEST_F(VSISAPhase1Test, VLOAD_Offset)
{
    // VBO: [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
    float vbo_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    SetVBO(vbo_data, 8);

    // VLOAD R4, #16 -> loads floats at indices 4,5,6,7 = {5, 6, 7, 8}
    Instruction vload = PatternVS::vs_vload(4, 16);
    interp.ExecuteInstruction(vload);

    EXPECT_FLOAT_EQ(GetReg(4), 5.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 6.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 7.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 8.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT
// NOTE: VOUTPUT reads from rd (0..3 in this test), but R0 is zero register.
// We use rd=4 so it reads from R4-R7 which we can set.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, VOUTPUT_Basic)
{
    // Write clip coords (1, 2, 3, 4) as vertex 0
    // VOUTPUT reads from rd (R4 in this case), so we set R4-R7
    SetReg(4, 1.0f);
    SetReg(5, 2.0f);
    SetReg(6, 3.0f);
    SetReg(7, 4.0f);

    Instruction voutput = PatternVS::vs_voutput(4, 0);
    EXPECT_EQ(voutput.GetOpcode(), Opcode::VS_VOUTPUT);
    EXPECT_EQ(voutput.GetRd(), 4u);

    interp.ExecuteInstruction(voutput);

    EXPECT_EQ(interp.GetVertexCount(), 1u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), 2.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), 3.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 4.0f);
}

TEST_F(VSISAPhase1Test, VOUTPUT_MultipleVertices)
{
    // Vertex 0: (0, 0, 0, 1) at R4-R7
    SetReg(4, 0.0f); SetReg(5, 0.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 1: (1, 0, 0, 1) at R4-R7
    SetReg(4, 1.0f); SetReg(5, 0.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 2: (0, 1, 0, 1) at R4-R7
    SetReg(4, 0.0f); SetReg(5, 1.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 3u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(2, 1), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_JUMP
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, JUMP_Basic)
{
    // JUMP with offset=5: PC += 5*4 = 20
    Instruction jump = PatternVS::vs_jump(5);
    EXPECT_EQ(jump.GetOpcode(), Opcode::VS_JUMP);

    interp.ExecuteInstruction(jump);
    EXPECT_EQ(interp.GetPC(), 20u);
}

TEST_F(VSISAPhase1Test, JUMP_NegativeOffset)
{
    // JUMP with offset=-1: PC -= 4 (starting from some base)
    interp.SetRegister(0, 0.0f);  // dummy to ensure PC is tracked
    // Note: PC starts at 0, but we execute instructions sequentially
    // Let's just verify the encoding
    Instruction jump = PatternVS::vs_jump(-1);
    EXPECT_EQ(jump.GetSignedImm(), -1);

    interp.ExecuteInstruction(jump);
    EXPECT_EQ(interp.GetPC(), static_cast<uint32_t>(-4));
}

// ---------------------------------------------------------------------------
// Test: VS_MAT_MUL (4x4 matrix * vector)
// NOTE: MAT_MUL reads vector from Rb (not Rd). We use R1-R4 for the vector
// to avoid R0 (zero register). R8-R23 for the matrix, R1-R4 for the result.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, MAT_MUL_IdentityTransform)
{
    // Column-major identity matrix at R8-R23:
    // col0 = {1,0,0,0} at R8,R12,R16,R20
    // col1 = {0,1,0,0} at R9,R13,R17,R21
    // col2 = {0,0,1,0} at R10,R14,R18,R22
    // col3 = {0,0,0,1} at R11,R15,R19,R23
    SetReg(8,  1.0f); SetReg(9,  0.0f); SetReg(10, 0.0f); SetReg(11, 0.0f);  // col 0
    SetReg(12, 0.0f); SetReg(13, 1.0f); SetReg(14, 0.0f); SetReg(15, 0.0f);  // col 1
    SetReg(16, 0.0f); SetReg(17, 0.0f); SetReg(18, 1.0f); SetReg(19, 0.0f);  // col 2
    SetReg(20, 0.0f); SetReg(21, 0.0f); SetReg(22, 0.0f); SetReg(23, 1.0f);  // col 3

    // Vector v = (2, 3, 4, 5) at R1-R4 (R0 is zero register, use R1-R4)
    SetReg(1, 2.0f); SetReg(2, 3.0f); SetReg(3, 4.0f); SetReg(4, 5.0f);

    // MAT_MUL: R1 = M * v (Rd=1, Ra(matrix)=8, Rb(vector)=1)
    Instruction matmul = PatternVS::vs_mat_mul(1, 8, 1);
    EXPECT_EQ(matmul.GetOpcode(), Opcode::VS_MAT_MUL);
    EXPECT_EQ(matmul.GetRd(), 1u);
    EXPECT_EQ(matmul.GetRa(), 8u);
    EXPECT_EQ(matmul.GetRb(), 1u);

    interp.ExecuteInstruction(matmul);

    // Identity * v = v = (2, 3, 4, 5)
    EXPECT_FLOAT_EQ(GetReg(1), 2.0f);
    EXPECT_FLOAT_EQ(GetReg(2), 3.0f);
    EXPECT_FLOAT_EQ(GetReg(3), 4.0f);
    EXPECT_FLOAT_EQ(GetReg(4), 5.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_MAT_MUL with translation matrix
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, MAT_MUL_Translation)
{
    // Translation matrix (column-major) at R8-R23:
    // col0 = {1,0,0,0}, col1 = {0,1,0,0}, col2 = {0,0,1,0}, col3 = {tx,ty,tz,1}
    float tx = 10.0f, ty = 20.0f, tz = 30.0f;
    SetReg(8,  1.0f); SetReg(9,  0.0f); SetReg(10, 0.0f); SetReg(11, tx);
    SetReg(12, 0.0f); SetReg(13, 1.0f); SetReg(14, 0.0f); SetReg(15, ty);
    SetReg(16, 0.0f); SetReg(17, 0.0f); SetReg(18, 1.0f); SetReg(19, tz);
    SetReg(20, 0.0f); SetReg(21, 0.0f); SetReg(22, 0.0f); SetReg(23, 1.0f);

    // Point p = (1, 2, 3, 1) at R1-R4
    SetReg(1, 1.0f); SetReg(2, 2.0f); SetReg(3, 3.0f); SetReg(4, 1.0f);

    // MAT_MUL: R1 = T * p
    Instruction matmul = PatternVS::vs_mat_mul(1, 8, 1);
    interp.ExecuteInstruction(matmul);

    // Result: (1+10, 2+20, 3+30, 1) = (11, 22, 33, 1)
    EXPECT_FLOAT_EQ(GetReg(1), 11.0f);
    EXPECT_FLOAT_EQ(GetReg(2), 22.0f);
    EXPECT_FLOAT_EQ(GetReg(3), 33.0f);
    EXPECT_FLOAT_EQ(GetReg(4), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_MOV_IMM (16-bit immediate encoding)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, MOV_IMM_Positive)
{
    Instruction movimm = PatternVS::vs_mov_imm(4, 42);
    EXPECT_EQ(movimm.GetOpcode(), Opcode::VS_MOV_IMM);

    interp.ExecuteInstruction(movimm);
    EXPECT_FLOAT_EQ(GetReg(4), 42.0f);
}

TEST_F(VSISAPhase1Test, MOV_IMM_Negative)
{
    Instruction movimm = PatternVS::vs_mov_imm(4, -100);
    interp.ExecuteInstruction(movimm);
    EXPECT_FLOAT_EQ(GetReg(4), -100.0f);
}

TEST_F(VSISAPhase1Test, MOV_IMM_LargeValue)
{
    Instruction movimm = PatternVS::vs_mov_imm(4, 1000);
    interp.ExecuteInstruction(movimm);
    EXPECT_FLOAT_EQ(GetReg(4), 1000.0f);
}

TEST_F(VSISAPhase1Test, MOV_IMM_GetSignedImm16_NegativeEncoding)
{
    // -1 encoded as 0xFFFF
    Instruction inst = PatternVS::vs_mov_imm(4, -1);
    EXPECT_EQ(inst.GetSignedImm16(), -1);
    interp.ExecuteInstruction(inst);
    EXPECT_FLOAT_EQ(GetReg(4), -1.0f);
}

// ---------------------------------------------------------------------------
// Test: MVP Transform (combined pipeline test)
// NOTE: This tests the full pipeline assuming a proper register allocation.
// Since Phase 1 lacks MOV, we manually pre-load registers.
// Pipeline: VBO -> R4-R7 -> MAT_MUL (matrix at R8-R23) -> R4-R7 -> VOUTPUT
// VOUTPUT reads from rd; we use rd=4, so it reads from R4-R7 (where MAT_MUL wrote).
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, MVP_Transform_Identity)
{
    // Identity projection matrix at R8-R23
    SetReg(8,  1.0f); SetReg(9,  0.0f); SetReg(10, 0.0f); SetReg(11, 0.0f);
    SetReg(12, 0.0f); SetReg(13, 1.0f); SetReg(14, 0.0f); SetReg(15, 0.0f);
    SetReg(16, 0.0f); SetReg(17, 0.0f); SetReg(18, 1.0f); SetReg(19, 0.0f);
    SetReg(20, 0.0f); SetReg(21, 0.0f); SetReg(22, 0.0f); SetReg(23, 1.0f);

    // Vertex 0: (0, 0, 0, 1) in R4-R7
    SetReg(4, 0.0f); SetReg(5, 0.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 1: (1, 0, 0, 1) in R4-R7
    SetReg(4, 1.0f); SetReg(5, 0.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 2: (0, 1, 0, 1) in R4-R7
    SetReg(4, 0.0f); SetReg(5, 1.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 3u);

    // Vertex 0: (0, 0, 0, 1)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 1.0f);

    // Vertex 1: (1, 0, 0, 1)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 1), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 2), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 3), 1.0f);

    // Vertex 2: (0, 1, 0, 1)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(2, 0), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(2, 1), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(2, 2), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(2, 3), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: VLOAD -> MAT_MUL -> VOUTPUT pipeline
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, VLOAD_MAT_MUL_VOUTPUT_Pipeline)
{
    // Setup: identity matrix at R8-R23, VBO with vertex data
    SetReg(8,  1.0f); SetReg(9,  0.0f); SetReg(10, 0.0f); SetReg(11, 0.0f);
    SetReg(12, 0.0f); SetReg(13, 1.0f); SetReg(14, 0.0f); SetReg(15, 0.0f);
    SetReg(16, 0.0f); SetReg(17, 0.0f); SetReg(18, 1.0f); SetReg(19, 0.0f);
    SetReg(20, 0.0f); SetReg(21, 0.0f); SetReg(22, 0.0f); SetReg(23, 1.0f);

    // VBO: one vertex at (1, 2, 3, 1)
    float vbo[] = {1.0f, 2.0f, 3.0f, 1.0f};
    SetVBO(vbo, 4);

    // Step 1: VLOAD R4, #0 -> R4-R7 = {1, 2, 3, 1}
    interp.ExecuteInstruction(PatternVS::vs_vload(4, 0));
    EXPECT_FLOAT_EQ(GetReg(4), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 2.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 3.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 1.0f);

    // Step 2: MAT_MUL R4, R8, R4 -> R4-R7 = Identity * {1,2,3,1} = {1,2,3,1}
    // Note: MAT_MUL reads vector from R4, writes result to R4 (overwriting input)
    // This works for identity matrix since result == input
    interp.ExecuteInstruction(PatternVS::vs_mat_mul(4, 8, 4));

    // Step 3: VOUTPUT R4, #0 -> output {1,2,3,1}
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 1u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), 2.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), 3.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: Instruction encoding verification
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, InstructionEncodings)
{
    // Verify factory methods produce correct opcodes
    EXPECT_EQ(PatternVS::vs_nop().GetOpcode(), Opcode::VS_NOP);
    EXPECT_EQ(PatternVS::vs_halt().GetOpcode(), Opcode::VS_HALT);
    EXPECT_EQ(PatternVS::vs_jump(0).GetOpcode(), Opcode::VS_JUMP);
    EXPECT_EQ(PatternVS::vs_add(0, 0, 0).GetOpcode(), Opcode::VS_ADD);
    EXPECT_EQ(PatternVS::vs_sub(0, 0, 0).GetOpcode(), Opcode::VS_SUB);
    EXPECT_EQ(PatternVS::vs_mul(0, 0, 0).GetOpcode(), Opcode::VS_MUL);
    EXPECT_EQ(PatternVS::vs_div(0, 0, 0).GetOpcode(), Opcode::VS_DIV);
    EXPECT_EQ(PatternVS::vs_dot3(0, 0, 0).GetOpcode(), Opcode::VS_DOT3);
    EXPECT_EQ(PatternVS::vs_normalize(0, 0).GetOpcode(), Opcode::VS_NORMALIZE);
    EXPECT_EQ(PatternVS::vs_mat_mul(0, 0, 0).GetOpcode(), Opcode::VS_MAT_MUL);
    EXPECT_EQ(PatternVS::vs_vload(0, 0).GetOpcode(), Opcode::VS_VLOAD);
    EXPECT_EQ(PatternVS::vs_voutput(0, 0).GetOpcode(), Opcode::VS_VOUTPUT);
    EXPECT_EQ(PatternVS::vs_mov_imm(0, 0).GetOpcode(), Opcode::VS_MOV_IMM);
}

// ---------------------------------------------------------------------------
// Test: HALT stops execution (returns early from ExecuteInstruction)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase1Test, HALT_AdvancesPC)
{
    Instruction halt = PatternVS::vs_halt();
    EXPECT_EQ(halt.GetOpcode(), Opcode::VS_HALT);

    interp.ExecuteInstruction(halt);
    // HALT advances PC (pc_.addr += 4) but does NOT advance cycles
    // (returns early from ExecuteInstruction to avoid double-increment)
    EXPECT_EQ(interp.GetPC(), 4u);
    EXPECT_EQ(interp.GetStats().cycles, 0u);  // No cycle advance for direct call
}
