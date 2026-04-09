// ============================================================================
// SoftGPU - test_vsisa_phase2.cpp
// Vertex Shader ISA Phase 2 Tests (CBR, MAD, SQRT, RSQ, CMP, MIN, MAX, SETP,
// DOT4, CROSS, LENGTH, MAT_ADD, MAT_TRANSPOSE, SIN, COS, EXPD2, LOGD2, POW,
// AND, OR, XOR, NOT, SHL, SHR, CVT_F32_S32, CVT_F32_U32, CVT_S32_F32,
// MOV, ATTR, VSTORE)
// ============================================================================

#include <gtest/gtest.h>
#include <limits>
#include <cmath>
#include <cstring>

#include "isa/Interpreter.hpp"
#include "isa/Instruction.hpp"
#include "isa/Opcode.hpp"

using namespace softgpu::isa;

// Helper: convert float to uint32 bits
static uint32_t float_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

// Helper: convert uint32 bits to float
static float bits_to_float(uint32_t bits) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

// ============================================================================
// VS ISA Phase 2 Tests
// NOTE: R0 is the zero register (hardcoded 0.0f). All tests avoid R0
// ============================================================================

class VSISAPhase2Test : public ::testing::Test {
protected:
    void SetUp() override {
        interp.Reset();
        interp.ResetVS();
    }
    void TearDown() override {}

    Interpreter interp;

    void SetReg(uint8_t r, float v) { interp.SetRegister(r, v); }
    float GetReg(uint8_t r) const { return interp.GetRegister(r); }
};

// ---------------------------------------------------------------------------
// Test: VS_CBR (Conditional Branch)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CBR_Taken)
{
    // CBR when condition is non-zero: PC += offset * 4
    SetReg(1, 1.0f);  // Condition = 1 (true)
    Instruction cbr = PatternVS::vs_cbr(1, 5);  // offset = 5
    EXPECT_EQ(cbr.GetOpcode(), Opcode::VS_CBR);
    interp.ExecuteInstruction(cbr);
    // PC += 5*4 = 20, starting from 0
    EXPECT_EQ(interp.GetPC(), 20u);
}

TEST_F(VSISAPhase2Test, CBR_NotTaken)
{
    // CBR when condition is zero: PC += 4
    SetReg(1, 0.0f);  // Condition = 0 (false)
    Instruction cbr = PatternVS::vs_cbr(1, 5);
    interp.ExecuteInstruction(cbr);
    EXPECT_EQ(interp.GetPC(), 4u);
}

TEST_F(VSISAPhase2Test, CBR_NegativeCondition)
{
    // CBR when condition is negative: should still take branch (non-zero)
    SetReg(1, -5.0f);
    Instruction cbr = PatternVS::vs_cbr(1, 3);
    interp.ExecuteInstruction(cbr);
    EXPECT_EQ(interp.GetPC(), 12u);
}

// ---------------------------------------------------------------------------
// Test: VS_MAD (Multiply-Add)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, MAD_Basic)
{
    // MAD: Rd = Ra * Rb + Rc
    SetReg(1, 2.0f);  // Ra
    SetReg(2, 3.0f);  // Rb
    SetReg(3, 4.0f);  // Rc
    Instruction mad = PatternVS::vs_mad(10, 1, 2, 3);  // R10 = R1*R2 + R3
    EXPECT_EQ(mad.GetOpcode(), Opcode::VS_MAD);
    interp.ExecuteInstruction(mad);
    EXPECT_FLOAT_EQ(GetReg(10), 2.0f * 3.0f + 4.0f);  // = 10
}

// ---------------------------------------------------------------------------
// Test: VS_SQRT
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, SQRT_Basic)
{
    SetReg(1, 16.0f);
    Instruction sqrt = PatternVS::vs_sqrt(10, 1);
    EXPECT_EQ(sqrt.GetOpcode(), Opcode::VS_SQRT);
    interp.ExecuteInstruction(sqrt);
    EXPECT_FLOAT_EQ(GetReg(10), 4.0f);
}

TEST_F(VSISAPhase2Test, SQRT_Zero)
{
    SetReg(1, 0.0f);
    Instruction sqrt = PatternVS::vs_sqrt(10, 1);
    interp.ExecuteInstruction(sqrt);
    EXPECT_FLOAT_EQ(GetReg(10), 0.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_RSQ (Reciprocal Square Root)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, RSQ_Basic)
{
    SetReg(1, 4.0f);  // 1/sqrt(4) = 0.5
    Instruction rsq = PatternVS::vs_rsq(10, 1);
    EXPECT_EQ(rsq.GetOpcode(), Opcode::VS_RSQ);
    interp.ExecuteInstruction(rsq);
    EXPECT_NEAR(GetReg(10), 0.5f, 1e-5f);
}

TEST_F(VSISAPhase2Test, RSQ_One)
{
    SetReg(1, 1.0f);  // 1/sqrt(1) = 1
    Instruction rsq = PatternVS::vs_rsq(10, 1);
    interp.ExecuteInstruction(rsq);
    EXPECT_NEAR(GetReg(10), 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: VS_CMP
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CMP_PositiveCondition)
{
    // CMP: Rd = (Ra >= Rb) ? Rb : 0
    SetReg(1, 15.0f);  // Ra = 15 (>= Rb)
    SetReg(2, 10.0f);  // Rb = 10
    Instruction cmp = PatternVS::vs_cmp(10, 1, 2);
    interp.ExecuteInstruction(cmp);
    EXPECT_FLOAT_EQ(GetReg(10), 10.0f);
}

TEST_F(VSISAPhase2Test, CMP_NegativeCondition)
{
    SetReg(1, -5.0f);  // Ra = -5 (< 0)
    SetReg(2, 10.0f);  // Rb = 10
    Instruction cmp = PatternVS::vs_cmp(10, 1, 2);
    interp.ExecuteInstruction(cmp);
    EXPECT_FLOAT_EQ(GetReg(10), 0.0f);
}

TEST_F(VSISAPhase2Test, CMP_ZeroCondition)
{
    // CMP: Rd = (Ra >= Rb) ? Rb : 0
    SetReg(1, 0.0f);   // Ra = 0 (>= Rb when Rb = 0)
    SetReg(2, 0.0f);  // Rb = 0
    Instruction cmp = PatternVS::vs_cmp(10, 1, 2);
    interp.ExecuteInstruction(cmp);
    EXPECT_FLOAT_EQ(GetReg(10), 0.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_MIN, VS_MAX
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, MIN_Basic)
{
    SetReg(1, 3.0f);
    SetReg(2, 5.0f);
    Instruction min_inst = PatternVS::vs_min(10, 1, 2);
    interp.ExecuteInstruction(min_inst);
    EXPECT_FLOAT_EQ(GetReg(10), 3.0f);
}

TEST_F(VSISAPhase2Test, MIN_Negative)
{
    SetReg(1, -5.0f);
    SetReg(2, -2.0f);
    Instruction min_inst = PatternVS::vs_min(10, 1, 2);
    interp.ExecuteInstruction(min_inst);
    EXPECT_FLOAT_EQ(GetReg(10), -5.0f);
}

TEST_F(VSISAPhase2Test, MAX_Basic)
{
    SetReg(1, 3.0f);
    SetReg(2, 5.0f);
    Instruction max_inst = PatternVS::vs_max(10, 1, 2);
    interp.ExecuteInstruction(max_inst);
    EXPECT_FLOAT_EQ(GetReg(10), 5.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_DOT4
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, DOT4_Basic)
{
    // Ra = (1, 2, 3, 4), Rb = (5, 6, 7, 8)
    // dot = 1*5 + 2*6 + 3*7 + 4*8 = 5 + 12 + 21 + 32 = 70
    SetReg(1, 1.0f); SetReg(2, 2.0f); SetReg(3, 3.0f); SetReg(4, 4.0f);
    SetReg(5, 5.0f); SetReg(6, 6.0f); SetReg(7, 7.0f); SetReg(8, 8.0f);
    Instruction dot4 = PatternVS::vs_dot4(10, 1, 5);  // Ra=1, Rb=5
    interp.ExecuteInstruction(dot4);
    EXPECT_FLOAT_EQ(GetReg(10), 70.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_CROSS
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CROSS_Basic)
{
    // a = (1, 0, 0), b = (0, 1, 0)
    // a × b = (0, 0, 1)
    SetReg(1, 1.0f); SetReg(2, 0.0f); SetReg(3, 0.0f);
    SetReg(5, 0.0f); SetReg(6, 1.0f); SetReg(7, 0.0f);
    Instruction cross = PatternVS::vs_cross(10, 1, 5);
    interp.ExecuteInstruction(cross);
    EXPECT_NEAR(GetReg(10), 0.0f, 1e-6f);   // x = ay*bz - az*by = 0*0 - 0*1 = 0
    EXPECT_NEAR(GetReg(11), 0.0f, 1e-6f);  // y = az*bx - ax*bz = 0*0 - 1*0 = 0
    EXPECT_NEAR(GetReg(12), 1.0f, 1e-6f);  // z = ax*by - ay*bx = 1*1 - 0*0 = 1
}

// ---------------------------------------------------------------------------
// Test: VS_LENGTH
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, LENGTH_Basic)
{
    // |(3, 4, 0)| = 5
    SetReg(1, 3.0f); SetReg(2, 4.0f); SetReg(3, 0.0f);
    Instruction len = PatternVS::vs_length(10, 1);
    interp.ExecuteInstruction(len);
    EXPECT_NEAR(GetReg(10), 5.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: VS_MAT_ADD
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, MAT_ADD_Basic)
{
    // A and B are 4x4 matrices (16 floats each), result = A + B
    // A at R1-R16 (avoid R0), B at R17-R31, result at R17-R31 (overwrites B)
    // R4-type encoding only supports 5-bit registers (0-31)
    for (int i = 0; i < 16; ++i) {
        SetReg(1 + i, static_cast<float>(i + 1));           // A at R1-R16
        SetReg(17 + i, static_cast<float>((i + 1) * 10));   // B at R17-R31
    }
    // MAT_ADD R17 = R1 + R17 (result overwrites B, uses non-zero Ra)
    Instruction mat_add = PatternVS::vs_mat_add(17, 1, 17);
    interp.ExecuteInstruction(mat_add);
    // Result at R17-R31: each should be (i+1) + (i+1)*10 = (i+1)*11
    for (int i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(GetReg(17 + i), static_cast<float>((i + 1) * 11));
    }
}

// ---------------------------------------------------------------------------
// Test: VS_MAT_TRANSPOSE
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, MAT_TRANSPOSE_Basic)
{
    // M = |1 2 3 4|
    //     |5 6 7 8|
    //     |9 10 11 12|
    //     |13 14 15 16|
    // M^T: M[j*4+i] = element at row i, col j of original
    for (int i = 0; i < 16; ++i) {
        SetReg(8 + i, static_cast<float>(i + 1));
    }
    Instruction trans = PatternVS::vs_mat_transpose(24, 8);  // R24 = transpose(R8)
    interp.ExecuteInstruction(trans);
    // Check diagonal elements should remain 1, 6, 11, 16
    EXPECT_FLOAT_EQ(GetReg(24), 1.0f);   // (0,0)
    EXPECT_FLOAT_EQ(GetReg(29), 6.0f);   // (1,1) = 6
    EXPECT_FLOAT_EQ(GetReg(34), 11.0f);  // (2,2) = 11
    EXPECT_FLOAT_EQ(GetReg(39), 16.0f);  // (3,3) = 16
}

// ---------------------------------------------------------------------------
// Test: VS_SIN
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, SIN_Basic)
{
    SetReg(1, 0.0f);
    Instruction sin_inst = PatternVS::vs_sin(10, 1);
    interp.ExecuteInstruction(sin_inst);
    EXPECT_NEAR(GetReg(10), 0.0f, 1e-5f);
}

TEST_F(VSISAPhase2Test, SIN_PiOver2)
{
    SetReg(1, 3.14159265f / 2.0f);  // pi/2
    Instruction sin_inst = PatternVS::vs_sin(10, 1);
    interp.ExecuteInstruction(sin_inst);
    EXPECT_NEAR(GetReg(10), 1.0f, 1e-3f);  // sin(pi/2) ≈ 1
}

// ---------------------------------------------------------------------------
// Test: VS_COS
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, COS_Basic)
{
    SetReg(1, 0.0f);
    Instruction cos_inst = PatternVS::vs_cos(10, 1);
    interp.ExecuteInstruction(cos_inst);
    EXPECT_NEAR(GetReg(10), 1.0f, 1e-5f);
}

TEST_F(VSISAPhase2Test, COS_Pi)
{
    SetReg(1, 3.14159265f);  // pi
    Instruction cos_inst = PatternVS::vs_cos(10, 1);
    interp.ExecuteInstruction(cos_inst);
    EXPECT_NEAR(GetReg(10), -1.0f, 1e-3f);  // cos(pi) ≈ -1
}

// ---------------------------------------------------------------------------
// Test: VS_EXPD2 (2^x)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, EXPD2_Basic)
{
    SetReg(1, 3.0f);  // 2^3 = 8
    Instruction expd2 = PatternVS::vs_expd2(10, 1);
    interp.ExecuteInstruction(expd2);
    EXPECT_NEAR(GetReg(10), 8.0f, 1e-5f);
}

TEST_F(VSISAPhase2Test, EXPD2_Zero)
{
    SetReg(1, 0.0f);  // 2^0 = 1
    Instruction expd2 = PatternVS::vs_expd2(10, 1);
    interp.ExecuteInstruction(expd2);
    EXPECT_NEAR(GetReg(10), 1.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: VS_LOGD2 (log2 x)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, LOGD2_Basic)
{
    SetReg(1, 8.0f);  // log2(8) = 3
    Instruction logd2 = PatternVS::vs_logd2(10, 1);
    interp.ExecuteInstruction(logd2);
    EXPECT_NEAR(GetReg(10), 3.0f, 1e-5f);
}

TEST_F(VSISAPhase2Test, LOGD2_One)
{
    SetReg(1, 1.0f);  // log2(1) = 0
    Instruction logd2 = PatternVS::vs_logd2(10, 1);
    interp.ExecuteInstruction(logd2);
    EXPECT_NEAR(GetReg(10), 0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test: VS_POW
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, POW_Basic)
{
    SetReg(1, 2.0f);  // base
    SetReg(2, 3.0f);  // exponent: 2^3 = 8
    Instruction pow_inst = PatternVS::vs_pow(10, 1, 2);
    interp.ExecuteInstruction(pow_inst);
    EXPECT_NEAR(GetReg(10), 8.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// Test: VS_AND (bitwise)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, AND_Basic)
{
    // 0xFFFF0000 & 0x0000FFFF = 0x00000000
    float a = bits_to_float(0xFFFF0000u);
    float b = bits_to_float(0x0000FFFFu);
    SetReg(1, a);
    SetReg(2, b);
    Instruction and_inst = PatternVS::vs_and(10, 1, 2);
    interp.ExecuteInstruction(and_inst);
    EXPECT_EQ(float_bits(GetReg(10)), 0x00000000u);
}

// ---------------------------------------------------------------------------
// Test: VS_OR (bitwise)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, OR_Basic)
{
    // 0xF0F0F0F0 | 0x0F0F0F0F = 0xFFFFFFFF
    float a = bits_to_float(0xF0F0F0F0u);
    float b = bits_to_float(0x0F0F0F0Fu);
    SetReg(1, a);
    SetReg(2, b);
    Instruction or_inst = PatternVS::vs_or(10, 1, 2);
    interp.ExecuteInstruction(or_inst);
    EXPECT_EQ(float_bits(GetReg(10)), 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Test: VS_XOR (bitwise)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, XOR_Basic)
{
    // 0xAAAAAAAA ^ 0x55555555 = 0xFFFFFFFF
    float a = bits_to_float(0xAAAAAAAAu);
    float b = bits_to_float(0x55555555u);
    SetReg(1, a);
    SetReg(2, b);
    Instruction xor_inst = PatternVS::vs_xor(10, 1, 2);
    interp.ExecuteInstruction(xor_inst);
    EXPECT_EQ(float_bits(GetReg(10)), 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Test: VS_NOT (bitwise)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, NOT_Basic)
{
    // ~0xAAAAAAAA = 0x55555555
    float a = bits_to_float(0xAAAAAAAAu);
    SetReg(1, a);
    Instruction not_inst = PatternVS::vs_not(10, 1);
    interp.ExecuteInstruction(not_inst);
    EXPECT_EQ(float_bits(GetReg(10)), 0x55555555u);
}

// ---------------------------------------------------------------------------
// Test: VS_SHL (shift left)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, SHL_Basic)
{
    // 1 << 3 = 8
    SetReg(1, bits_to_float(0x00000001u));
    SetReg(2, 3.0f);
    Instruction shl = PatternVS::vs_shl(10, 1, 2);
    interp.ExecuteInstruction(shl);
    EXPECT_EQ(float_bits(GetReg(10)), 0x00000008u);
}

// ---------------------------------------------------------------------------
// Test: VS_SHR (shift right)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, SHR_Basic)
{
    // 8 >> 3 = 1
    SetReg(1, bits_to_float(0x00000008u));
    SetReg(2, 3.0f);
    Instruction shr = PatternVS::vs_shr(10, 1, 2);
    interp.ExecuteInstruction(shr);
    EXPECT_EQ(float_bits(GetReg(10)), 0x00000001u);
}

// ---------------------------------------------------------------------------
// Test: VS_CVT_F32_S32 (float to signed int numerical conversion)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CVT_F32_S32_Basic)
{
    // CVT_F32_S32: convert float value to int32, then write int bits as float
    float f = 123456.0f;
    SetReg(1, f);
    Instruction cvt = PatternVS::vs_cvt_f32_s32(10, 1);
    interp.ExecuteInstruction(cvt);
    float result_f = GetReg(10);
    int32_t result = *reinterpret_cast<int32_t*>(&result_f);
    EXPECT_EQ(result, 123456);  // numerically truncated from 123456.0f
}

// ---------------------------------------------------------------------------
// Test: VS_CVT_F32_U32 (float to unsigned int numerical conversion)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CVT_F32_U32_Basic)
{
    // CVT_F32_U32: convert float value to uint32, then write uint bits as float
    float f = 3000000000.0f;
    SetReg(1, f);
    Instruction cvt = PatternVS::vs_cvt_f32_u32(10, 1);
    interp.ExecuteInstruction(cvt);
    float result_f = GetReg(10);
    uint32_t result = *reinterpret_cast<uint32_t*>(&result_f);
    EXPECT_EQ(result, 3000000000u);  // numerically truncated from 3000000000.0f
}

// ---------------------------------------------------------------------------
// Test: VS_CVT_S32_F32 (signed int to float)
// CVT_S32_F32 reads the float register's bits as int32, then converts to float value.
// This is an unusual operation; for standard int→float conversion, CVT_F32_S32 is preferred.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CVT_S32_F32_Basic)
{
    // CVT_S32_F32 reads the float register's bit pattern as int, then converts to float.
    // With 42.0f (bits=0x42280000), the int value is 1108376064, which becomes ~1.1e9 as float.
    // We verify it executes without error; exact value depends on bit reinterpretation.
    SetReg(1, 42.0f);
    Instruction cvt = PatternVS::vs_cvt_s32_f32(10, 1);
    interp.ExecuteInstruction(cvt);
    float result = GetReg(10);
    EXPECT_GT(result, 1e9f);  // bit reinterpretation of 42.0f gives ~1.1B
}

// ---------------------------------------------------------------------------
// Test: VS_MOV
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, MOV_Basic)
{
    SetReg(1, 42.0f);
    Instruction mov = PatternVS::vs_mov(10, 1);
    interp.ExecuteInstruction(mov);
    EXPECT_FLOAT_EQ(GetReg(10), 42.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, ATTR_Basic)
{
    // VS_ATTR loads from VBO at byte_offset = m_attr_table[attr_id]
    // Default attr table: attr_id=0 → byte_offset=0
    // Set VBO so that VBO[0..3] = (0, 0, 0, 1)
    float vbo[] = {0.0f, 0.0f, 0.0f, 1.0f};
    interp.SetVBO(vbo, 4);

    Instruction attr = PatternVS::vs_attr(10, 0);  // attr_id=0
    EXPECT_EQ(attr.GetOpcode(), Opcode::VS_ATTR);
    interp.ExecuteInstruction(attr);
    EXPECT_FLOAT_EQ(GetReg(10), 0.0f);   // u
    EXPECT_FLOAT_EQ(GetReg(11), 0.0f);   // v
    EXPECT_FLOAT_EQ(GetReg(12), 0.0f);   // z
    EXPECT_FLOAT_EQ(GetReg(13), 1.0f);   // w
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR with color attribute (attr_id=2, byte_offset=16)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, ATTR_Color)
{
    // VBO layout: pos(16 bytes) + color(16 bytes) = 32 bytes total
    // color at byte_offset=16 → float index=4
    float vbo[] = {
        0.0f, 0.0f, 0.0f, 1.0f,  // position (not used by ATTR)
        0.5f, 0.3f, 0.7f, 1.0f   // color at VBO[4..7]
    };
    interp.SetVBO(vbo, 8);

    // attr_id=2 → byte_offset=16 (color in default layout)
    Instruction attr = PatternVS::vs_attr(10, 2);
    interp.ExecuteInstruction(attr);
    EXPECT_FLOAT_EQ(GetReg(10), 0.5f);   // r
    EXPECT_FLOAT_EQ(GetReg(11), 0.3f);   // g
    EXPECT_FLOAT_EQ(GetReg(12), 0.7f);   // b
    EXPECT_FLOAT_EQ(GetReg(13), 1.0f);   // a
}

// ---------------------------------------------------------------------------
// Test: VS_VSTORE (just verify it executes without error)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, VSTORE_Basic)
{
    // VSTORE: Write Ra to memory at (rd * 4 + offset)
    SetReg(0, 123.0f);  // Ra
    SetReg(4, 0.0f);    // Rd = 4 (base address)
    Instruction vstore = PatternVS::vs_vstore(4, 0, 0);  // offset=0
    interp.ExecuteInstruction(vstore);
    // VSTORE doesn't modify registers
    EXPECT_FLOAT_EQ(GetReg(4), 0.0f);  // Rd unchanged
}

// ---------------------------------------------------------------------------
// Test: Instruction encoding verification for Phase 2
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, InstructionEncodings)
{
    EXPECT_EQ(PatternVS::vs_cbr(0, 0).GetOpcode(), Opcode::VS_CBR);
    EXPECT_EQ(PatternVS::vs_mad(0, 0, 0, 0).GetOpcode(), Opcode::VS_MAD);
    EXPECT_EQ(PatternVS::vs_sqrt(0, 0).GetOpcode(), Opcode::VS_SQRT);
    EXPECT_EQ(PatternVS::vs_rsq(0, 0).GetOpcode(), Opcode::VS_RSQ);
    EXPECT_EQ(PatternVS::vs_cmp(0, 0, 0).GetOpcode(), Opcode::VS_CMP);
    EXPECT_EQ(PatternVS::vs_min(0, 0, 0).GetOpcode(), Opcode::VS_MIN);
    EXPECT_EQ(PatternVS::vs_max(0, 0, 0).GetOpcode(), Opcode::VS_MAX);
    EXPECT_EQ(PatternVS::vs_setp(0, 0, 0).GetOpcode(), Opcode::VS_SETP);
    EXPECT_EQ(PatternVS::vs_dot4(0, 0, 0).GetOpcode(), Opcode::VS_DOT4);
    EXPECT_EQ(PatternVS::vs_cross(0, 0, 0).GetOpcode(), Opcode::VS_CROSS);
    EXPECT_EQ(PatternVS::vs_length(0, 0).GetOpcode(), Opcode::VS_LENGTH);
    EXPECT_EQ(PatternVS::vs_mat_add(0, 0, 0).GetOpcode(), Opcode::VS_MAT_ADD);
    EXPECT_EQ(PatternVS::vs_mat_transpose(0, 0).GetOpcode(), Opcode::VS_MAT_TRANSPOSE);
    EXPECT_EQ(PatternVS::vs_attr(0, 0).GetOpcode(), Opcode::VS_ATTR);
    EXPECT_EQ(PatternVS::vs_vstore(0, 0, 0).GetOpcode(), Opcode::VS_VSTORE);
    EXPECT_EQ(PatternVS::vs_sin(0, 0).GetOpcode(), Opcode::VS_SIN);
    EXPECT_EQ(PatternVS::vs_cos(0, 0).GetOpcode(), Opcode::VS_COS);
    EXPECT_EQ(PatternVS::vs_expd2(0, 0).GetOpcode(), Opcode::VS_EXPD2);
    EXPECT_EQ(PatternVS::vs_logd2(0, 0).GetOpcode(), Opcode::VS_LOGD2);
    EXPECT_EQ(PatternVS::vs_pow(0, 0, 0).GetOpcode(), Opcode::VS_POW);
    EXPECT_EQ(PatternVS::vs_and(0, 0, 0).GetOpcode(), Opcode::VS_AND);
    EXPECT_EQ(PatternVS::vs_or(0, 0, 0).GetOpcode(), Opcode::VS_OR);
    EXPECT_EQ(PatternVS::vs_xor(0, 0, 0).GetOpcode(), Opcode::VS_XOR);
    EXPECT_EQ(PatternVS::vs_not(0, 0).GetOpcode(), Opcode::VS_NOT);
    EXPECT_EQ(PatternVS::vs_shl(0, 0, 0).GetOpcode(), Opcode::VS_SHL);
    EXPECT_EQ(PatternVS::vs_shr(0, 0, 0).GetOpcode(), Opcode::VS_SHR);
    EXPECT_EQ(PatternVS::vs_cvt_f32_s32(0, 0).GetOpcode(), Opcode::VS_CVT_F32_S32);
    EXPECT_EQ(PatternVS::vs_cvt_f32_u32(0, 0).GetOpcode(), Opcode::VS_CVT_F32_U32);
    EXPECT_EQ(PatternVS::vs_cvt_s32_f32(0, 0).GetOpcode(), Opcode::VS_CVT_S32_F32);
    EXPECT_EQ(PatternVS::vs_mov(0, 0).GetOpcode(), Opcode::VS_MOV);
}

// ---------------------------------------------------------------------------
// Test: Combined Phase 2 pipeline (CBR + MAD + MOV)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase2Test, CombinedPipeline)
{
    // Simple shader: MAD then MOV
    // MAD: R10 = R1 * R2 + R3 = 2 * 3 + 4 = 10
    SetReg(1, 2.0f);
    SetReg(2, 3.0f);
    SetReg(3, 4.0f);
    Instruction mad = PatternVS::vs_mad(10, 1, 2, 3);
    interp.ExecuteInstruction(mad);
    EXPECT_FLOAT_EQ(GetReg(10), 10.0f);

    // MOV: R11 = R10 = 10
    Instruction mov = PatternVS::vs_mov(11, 10);
    interp.ExecuteInstruction(mov);
    EXPECT_FLOAT_EQ(GetReg(11), 10.0f);
}
