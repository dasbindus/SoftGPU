// ============================================================================
// SoftGPU - test_vsisa_phase3.cpp
// Vertex Shader ISA Phase 3 Tests (VLOAD boundary, VSTORE memory, OUTPUT_VS
// clipping, LDC, ATTR extended)
//
// Phase 3 covers:
//   VLOAD  — 4-aligned boundary, byte_offset boundary, cross-vertex pipeline
//   VSTORE — store to memory, verify data, boundary cases
//   OUTPUT_VS (VS_VOUTPUT) — clip coordinate behavior
//   LDC    — constant buffer load (stub + extended)
//   ATTR   — extended attribute extraction (normal, UV, boundary)
//
// NOTE: R0 is the zero register (hardcoded 0.0f). All tests avoid R0
// as a source of data.
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

// ============================================================================
// VS ISA Phase 3 Tests
// ============================================================================

class VSISAPhase3Test : public ::testing::Test {
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
    void SetMemory(uint32_t addr, float v) { interp.SetMemory(addr, v); }
    float GetMemory(uint32_t addr) { return interp.GetMemory(addr); }
};

// ============================================================================
// VLOAD Tests — 4-aligned load, byte_offset boundary, cross-vertex
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VS_VLOAD 4-aligned boundary
// VLOAD byte_offset must be 4-aligned; with 10-bit imm, max offset = 1020.
// Test: loading at the last valid 4-aligned position within VBO
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VLOAD_4AlignedBoundary)
{
    // VBO: 8 floats. VLOAD at byte_offset=16 loads floats at indices 4,5,6,7
    // This is the boundary between first 4 floats and second 4 floats.
    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f,   // indices 0-3
                   5.0f, 6.0f, 7.0f, 8.0f};  // indices 4-7
    SetVBO(vbo, 8);

    // VLOAD R4, #16 -> float_idx = 16/4 = 4, loads indices 4,5,6,7
    Instruction vload = PatternVS::vs_vload(4, 16);
    EXPECT_EQ(vload.GetOpcode(), Opcode::VS_VLOAD);

    interp.ExecuteInstruction(vload);

    EXPECT_FLOAT_EQ(GetReg(4), 5.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 6.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 7.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 8.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_VLOAD byte_offset at max boundary (1020 bytes = 255 float indices)
// VBO is small; this should return 0.0f for out-of-range indices
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VLOAD_ByteOffsetBeyondVBO)
{
    // VBO: only 4 floats. byte_offset=1020 → float_idx = 1020/4 = 255
    // 255 >> 4 (VBO has only 4 floats), all should return 0.0f
    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f};
    SetVBO(vbo, 4);

    // byte_offset=1020 (max 10-bit = 1023, 1020 is the last 4-aligned value)
    Instruction vload = PatternVS::vs_vload(4, 1020);
    interp.ExecuteInstruction(vload);

    // All out-of-bounds → should return 0.0f
    EXPECT_FLOAT_EQ(GetReg(4), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 0.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_VLOAD partial VBO fill (loading 4 floats but VBO has fewer)
// When VBO has fewer than 4 floats from the offset, return 0.0f for missing
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VLOAD_PartialVBOLoad)
{
    // VBO: 6 floats. byte_offset=12 → float_idx = 12/4 = 3
    // Loads indices 3,4,5 → 3 valid + 1 out-of-bounds
    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    SetVBO(vbo, 6);

    Instruction vload = PatternVS::vs_vload(4, 12);
    interp.ExecuteInstruction(vload);

    EXPECT_FLOAT_EQ(GetReg(4), 4.0f);  // index 3
    EXPECT_FLOAT_EQ(GetReg(5), 5.0f);  // index 4
    EXPECT_FLOAT_EQ(GetReg(6), 6.0f);  // index 5
    EXPECT_FLOAT_EQ(GetReg(7), 0.0f);  // index 6 → out of bounds → 0
}

// ---------------------------------------------------------------------------
// Test: VS_VLOAD cross-vertex load via RunVertexProgram
// Simulates loading position data for multiple vertices from interleaved VBO
// VBO layout: vertex0(pos+uv), vertex1(pos+uv), vertex2(pos+uv)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VLOAD_CrossVertex_InterleavedVBO)
{
    // Interleaved VBO: pos(16 bytes) + uv(8 bytes) per vertex = 24 bytes/vertex
    // Vertex 0: pos=(0,0,0,1) at byte 0,   uv=(0,0) at byte 16
    // Vertex 1: pos=(1,0,0,1) at byte 24,  uv=(1,0) at byte 40
    // Vertex 2: pos=(0,1,0,1) at byte 48,  uv=(0,1) at byte 64
    float vbo[] = {
        // Vertex 0
        0.0f, 0.0f, 0.0f, 1.0f,   // byte 0-15: position
        0.0f, 0.0f, 1.0f, 1.0f,   // byte 16-31: uv (note: interleaved format)
        // Vertex 1
        1.0f, 0.0f, 0.0f, 1.0f,   // byte 32-47: position
        1.0f, 0.0f, 1.0f, 1.0f,   // byte 48-63: uv
        // Vertex 2
        0.0f, 1.0f, 0.0f, 1.0f,   // byte 64-79: position
        0.0f, 1.0f, 1.0f, 1.0f,   // byte 80-95: uv
    };
    SetVBO(vbo, 24);  // 24 floats

    // Build simple bytecode: VLOAD R4, #0 -> VOUTPUT R4
    // Note: RunVertexProgram resets VS per-vertex, so we can test per-vertex loads
    // For simplicity, just test sequential VLOADs (emulating per-vertex pipeline)
    // Vertex 0: load pos at byte_offset=0
    interp.ExecuteInstruction(PatternVS::vs_vload(4, 0));
    EXPECT_FLOAT_EQ(GetReg(4), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 1.0f);

    // Vertex 1: load pos at byte_offset=32
    interp.ExecuteInstruction(PatternVS::vs_vload(4, 32));
    EXPECT_FLOAT_EQ(GetReg(4), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 1.0f);

    // Vertex 2: load pos at byte_offset=64
    interp.ExecuteInstruction(PatternVS::vs_vload(4, 64));
    EXPECT_FLOAT_EQ(GetReg(4), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(5), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(6), 0.0f);
    EXPECT_FLOAT_EQ(GetReg(7), 1.0f);
}

// ============================================================================
// VSTORE Tests — store to memory, verify data, boundary
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VS_VSTORE basic memory write
// VSTORE writes Ra value to memory at address (Rd * 4 + offset)
// NOTE: Ra must be non-zero register (R0 is hardcoded zero register)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VSTORE_BasicWrite)
{
    // Ra = 5 (source register holding value 123.0f)
    // Rd = 4 (base address register = 4 → addr = 4*4 = 16)
    SetReg(5, 123.0f);   // Ra = R5 (non-zero register)
    SetReg(4, 0.0f);     // Rd = R4 (base address)

    Instruction vstore = PatternVS::vs_vstore(4, 0, 5);  // rd=4, offset=0, ra=5
    interp.ExecuteInstruction(vstore);

    // Write address = Rd * 4 + offset = 4 * 4 + 0 = 16
    EXPECT_EQ(float_bits(GetMemory(16)), float_bits(123.0f));
}

// ---------------------------------------------------------------------------
// Test: VS_VSTORE with non-zero offset
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VSTORE_WithOffset)
{
    // Ra = 99.0f at R5, Rd = R1, offset = 8
    // Address = 1*4 + 8 = 12
    SetReg(5, 99.0f);    // Ra = R5
    SetReg(1, 0.0f);     // Rd = R1 → base address = 1*4 = 4

    Instruction vstore = PatternVS::vs_vstore(1, 8, 5);  // rd=1, offset=8, ra=5
    interp.ExecuteInstruction(vstore);

    EXPECT_EQ(float_bits(GetMemory(12)), float_bits(99.0f));
}

// ---------------------------------------------------------------------------
// Test: VS_VSTORE with non-zero Rd base address
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VSTORE_NonZeroBase)
{
    // Ra = 77.0f at R5, Rd = R10, offset = 0
    // Address = 10*4 + 0 = 40
    SetReg(5, 77.0f);    // Ra = R5
    SetReg(10, 0.0f);    // Rd = R10 → base address = 10*4 = 40

    Instruction vstore = PatternVS::vs_vstore(10, 0, 5);  // rd=10, offset=0, ra=5
    interp.ExecuteInstruction(vstore);

    EXPECT_EQ(float_bits(GetMemory(40)), float_bits(77.0f));
}

// ---------------------------------------------------------------------------
// Test: VS_VSTORE multiple consecutive stores
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VSTORE_ConsecutiveStores)
{
    // Store multiple floats to consecutive addresses using R5 as value source
    SetReg(4, 0.0f);    // Rd = R4 → base = 16

    SetReg(5, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_vstore(4, 0, 5));   // addr 16
    SetReg(5, 2.0f);
    interp.ExecuteInstruction(PatternVS::vs_vstore(4, 4, 5));   // addr 20
    SetReg(5, 3.0f);
    interp.ExecuteInstruction(PatternVS::vs_vstore(4, 8, 5));   // addr 24
    SetReg(5, 4.0f);
    interp.ExecuteInstruction(PatternVS::vs_vstore(4, 12, 5));  // addr 28

    EXPECT_EQ(float_bits(GetMemory(16)), float_bits(1.0f));
    EXPECT_EQ(float_bits(GetMemory(20)), float_bits(2.0f));
    EXPECT_EQ(float_bits(GetMemory(24)), float_bits(3.0f));
    EXPECT_EQ(float_bits(GetMemory(28)), float_bits(4.0f));
}

// ---------------------------------------------------------------------------
// Test: VS_VSTORE does NOT modify registers (verifies Rd is unchanged)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VSTORE_RegisterUnchanged)
{
    SetReg(5, 999.0f);  // Ra = R5
    SetReg(5, 42.0f);   // Rd = R5 - should be unchanged after VSTORE
    Instruction vstore = PatternVS::vs_vstore(5, 0, 5);
    interp.ExecuteInstruction(vstore);

    // Rd should be unchanged (still holds the value we set before VSTORE)
    EXPECT_FLOAT_EQ(GetReg(5), 42.0f);
}

// ============================================================================
// OUTPUT_VS (VS_VOUTPUT) Tests — clip coordinate behavior
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT clip coordinates in NDC range [-1, 1]
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_ClipCoords_NDC_Range)
{
    // Full NDC cube: x=0.5, y=0.5, z=0.0, w=1.0
    SetReg(4, 0.5f); SetReg(5, 0.5f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 1u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 0.5f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), 0.5f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT clip coordinates at clip space boundary
// Clip space: x/w, y/w must be in [-1, 1] for visible, but VOUTPUT
// just stores raw clip coords. Test boundary values are stored correctly.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_ClipCoords_Boundary)
{
    // Near and far plane boundaries
    // Vertex 0: x=1.0, y=0, z=-1, w=1 (right on NDC boundary)
    SetReg(4, 1.0f); SetReg(5, 0.0f); SetReg(6, -1.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 1: x=-1, y=0, z=1, w=1 (left NDC + far plane)
    SetReg(4, -1.0f); SetReg(5, 0.0f); SetReg(6, 1.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 2u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 1.0f);   // x=1.0
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), -1.0f); // z=-1.0
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 0), -1.0f); // x=-1.0
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 2), 1.0f);   // z=1.0
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT clip coordinates outside clip space (would be clipped)
// VOUTPUT just stores raw values; clipping happens in rasterizer.
// We verify raw values are stored correctly even when out of [-1,1] range.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_ClipCoords_OutsideClipSpace)
{
    // Vertex 0: x=2.5, y=-2.5, z=0.5, w=1.0 (clearly outside clip space)
    SetReg(4, 2.5f); SetReg(5, -2.5f); SetReg(6, 0.5f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 1: x=100, y=100, z=0, w=50 (very large but valid clip coords)
    SetReg(4, 100.0f); SetReg(5, 100.0f); SetReg(6, 0.0f); SetReg(7, 50.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 2u);
    // Verify raw clip coords are stored correctly
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 2.5f);    // x=2.5 (outside NDC)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), -2.5f);   // y=-2.5 (outside NDC)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 0), 100.0f); // x=100 (large)
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 3), 50.0f);  // w=50
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT with w=0 (homogeneous coordinate edge case)
// When w=0, clip coord division by zero in perspective divide.
// VOUTPUT should still store the raw values (rasterizer handles this).
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_W_Zero)
{
    SetReg(4, 0.0f); SetReg(5, 0.0f); SetReg(6, 0.0f); SetReg(7, 0.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 1u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 0.0f);  // w=0 stored as-is
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT homogeneous coordinate perspective divide example
// w=1.0 → NDC = clip/w = clip
// w=2.0 → NDC = clip/2
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_HomogeneousCoords)
{
    // Vertex 0: clip (2, 4, 0, 1) → NDC (2, 4, 0)
    SetReg(4, 2.0f); SetReg(5, 4.0f); SetReg(6, 0.0f); SetReg(7, 1.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    // Vertex 1: clip (4, 8, 0, 2) → NDC (2, 4, 0) — same as above after divide
    SetReg(4, 4.0f); SetReg(5, 8.0f); SetReg(6, 0.0f); SetReg(7, 2.0f);
    interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));

    EXPECT_EQ(interp.GetVertexCount(), 2u);
    // Raw clip coords are stored as-is
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 2.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 1.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 0), 4.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(1, 3), 2.0f);
    // After perspective divide: v0→NDC(2,4,0), v1→NDC(2,4,0) — same position
}

// ---------------------------------------------------------------------------
// Test: VS_VOUTPUT max vertices (16) — VOUTPUTBUF boundary
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VOUTPUT_MaxVertices)
{
    // Write 16 vertices (MAX_VERTICES = 16)
    for (int v = 0; v < 16; ++v) {
        SetReg(4, static_cast<float>(v));   // x = vertex index
        SetReg(5, 0.0f);
        SetReg(6, 0.0f);
        SetReg(7, 1.0f);
        interp.ExecuteInstruction(PatternVS::vs_voutput(4, 0));
    }

    EXPECT_EQ(interp.GetVertexCount(), 16u);
    // Verify last vertex was stored
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(15, 0), 15.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(15, 3), 1.0f);
}

// ============================================================================
// LDC Tests — constant buffer load (stub + extended)
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VS_LDC stub executes without error (PC advances)
// LDC is currently a stub; we verify it executes cleanly.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, LDC_StubExecutes)
{
    // LDC: Rd = cbuffer[Ra + offset]
    // Currently stub: just advances PC
    SetReg(1, 0.0f);  // Ra (cbuffer base, not used in stub)
    SetReg(4, 0.0f);  // Rd - should be unchanged by stub

    Instruction ldc = PatternVS::vs_mov_imm(4, 0);  // Use MOV_IMM encoding for LDC
    // Note: LDC uses MakeU encoding with op=LDC
    Instruction ldc_inst = Instruction::MakeU(Opcode::LDC, 4, 1, 0);  // LDC R4, R1, #0
    EXPECT_EQ(ldc_inst.GetOpcode(), Opcode::LDC);

    interp.ExecuteInstruction(ldc_inst);

    // Stub should advance PC without error
    EXPECT_EQ(interp.GetPC(), 4u);
    // Rd should be unchanged (stub doesn't write)
    EXPECT_FLOAT_EQ(GetReg(4), 0.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_LDC with various offsets
// Even as stub, should advance PC correctly for each offset value
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, LDC_VariousOffsets)
{
    // LDC with various 10-bit offsets (0, 100, 500, 1023)
    // PC accumulates, so we check the final PC after all 4 LDCs
    for (uint16_t off : {0u, 100u, 500u, 1023u}) {
        Instruction ldc = Instruction::MakeU(Opcode::LDC, 4, 1, off);
        interp.ExecuteInstruction(ldc);
    }
    // 4 LDC instructions × 4 bytes each = 16 bytes = PC at 16
    EXPECT_EQ(interp.GetPC(), 16u);
}

// ---------------------------------------------------------------------------
// Test: VS_LDC encoding verification
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, LDC_EncodingVerification)
{
    // LDC uses U-type: opcode(7) | rd(5) | ra(5) | imm(10)
    // opcode = LDC = 0x1B = 27
    Instruction ldc = Instruction::MakeU(Opcode::LDC, 10, 5, 0x3FF);  // max offset
    EXPECT_EQ(ldc.GetOpcode(), Opcode::LDC);
    EXPECT_EQ(ldc.GetRd(), 10u);
    EXPECT_EQ(ldc.GetRa(), 5u);
    EXPECT_EQ(ldc.GetImm(), 0x3FFu);  // 10-bit max = 1023

    interp.ExecuteInstruction(ldc);
    EXPECT_EQ(interp.GetPC(), 4u);
}

// ============================================================================
// ATTR Tests — extended attribute extraction
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VS_ATTR with normal attribute (attr_id=1, byte_offset=16)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_Normal)
{
    // VBO: pos(16 bytes) + normal(16 bytes) = 32 bytes
    // attr_id=1 → byte_offset=16 → normal starts at float index 4
    float vbo[] = {
        1.0f, 2.0f, 3.0f, 1.0f,  // position at byte 0-15
        0.0f, 1.0f, 0.0f, 1.0f   // normal at byte 16-23: (0, 1, 0, 1)
    };
    interp.SetVBO(vbo, 8);

    // attr_id=1 → byte_offset=16 → normal in default attr table
    Instruction attr = PatternVS::vs_attr(10, 1);
    EXPECT_EQ(attr.GetOpcode(), Opcode::VS_ATTR);
    interp.ExecuteInstruction(attr);

    EXPECT_FLOAT_EQ(GetReg(10), 0.0f);   // nx
    EXPECT_FLOAT_EQ(GetReg(11), 1.0f);   // ny
    EXPECT_FLOAT_EQ(GetReg(12), 0.0f);   // nz
    EXPECT_FLOAT_EQ(GetReg(13), 1.0f);   // w
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR with UV attribute (attr_id=3, byte_offset=24)
// Note: byte_offset=24 → float index=6. VBO layout must match.
// UV is at byte 24 in default table, but we use a custom attr table
// to place UV data at a position accessible by attr_id=3.
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_UV)
{
    // Use custom attr table so attr_id=3 → byte_offset=8 (UV data)
    interp.SetAttrTable({0, 16, 16, 8});  // pos=0, normal=16, color=16, uv=8

    // VBO: uv(8 bytes) = 2 floats only
    // But ATTR loads 4 floats from byte_offset=8: indices 2,3,4,5
    // Need at least 6 floats in VBO
    float vbo[] = {
        1.0f, 2.0f,         // unused (indices 0-1)
        0.5f, 0.75f, 0.0f, 1.0f  // uv at byte 8: indices 2-5
    };
    interp.SetVBO(vbo, 6);

    Instruction attr = PatternVS::vs_attr(10, 3);  // attr_id=3 → byte_offset=8
    interp.ExecuteInstruction(attr);

    EXPECT_FLOAT_EQ(GetReg(10), 0.5f);   // u
    EXPECT_FLOAT_EQ(GetReg(11), 0.75f);  // v
    EXPECT_FLOAT_EQ(GetReg(12), 0.0f);   // z
    EXPECT_FLOAT_EQ(GetReg(13), 1.0f);   // w
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR out-of-range attr_id → falls back to offset 0
// When attr_id >= attr_table.size(), byte_offset=0 is used (fallback)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_OutOfRangeID)
{
    // Default attr table: {0, 16, 16, 24} - size 4
    // attr_id=99 >= 4 → fallback to byte_offset=0 → reads VBO[0-3]
    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f};
    interp.SetVBO(vbo, 4);

    Instruction attr = PatternVS::vs_attr(10, 99);
    interp.ExecuteInstruction(attr);

    // Falls back to byte_offset=0 → returns VBO[0-3]
    EXPECT_FLOAT_EQ(GetReg(10), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(11), 2.0f);
    EXPECT_FLOAT_EQ(GetReg(12), 3.0f);
    EXPECT_FLOAT_EQ(GetReg(13), 4.0f);
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR custom attr table
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_CustomAttrTable)
{
    // Set custom attr table: attr_id → byte_offset
    // Only attr_id 0 and 1 are defined; others should return 0
    interp.SetAttrTable({0, 8});  // pos at 0, custom attr at 8

    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    interp.SetVBO(vbo, 6);

    // attr_id=0 → byte_offset=0 → floats 0-3
    Instruction attr0 = PatternVS::vs_attr(10, 0);
    interp.ExecuteInstruction(attr0);
    EXPECT_FLOAT_EQ(GetReg(10), 1.0f);
    EXPECT_FLOAT_EQ(GetReg(11), 2.0f);
    EXPECT_FLOAT_EQ(GetReg(12), 3.0f);
    EXPECT_FLOAT_EQ(GetReg(13), 4.0f);

    // attr_id=1 → byte_offset=8 → floats 2-5
    Instruction attr1 = PatternVS::vs_attr(10, 1);
    interp.ExecuteInstruction(attr1);
    EXPECT_FLOAT_EQ(GetReg(10), 3.0f);   // index 2
    EXPECT_FLOAT_EQ(GetReg(11), 4.0f);   // index 3
    EXPECT_FLOAT_EQ(GetReg(12), 5.0f);   // index 4
    EXPECT_FLOAT_EQ(GetReg(13), 6.0f);   // index 5
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR partial VBO load (VBO shorter than attribute size)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_PartialVBOLoad)
{
    // VBO has only 4 floats; attr at byte_offset=8 (index 2)
    // Loads indices 2,3,4,5 but VBO has only 4 floats (indices 0-3)
    float vbo[] = {10.0f, 20.0f, 30.0f, 40.0f};
    interp.SetVBO(vbo, 4);

    // Custom attr table: attr_id=0 → byte_offset=8
    interp.SetAttrTable({8});

    Instruction attr = PatternVS::vs_attr(10, 0);
    interp.ExecuteInstruction(attr);

    EXPECT_FLOAT_EQ(GetReg(10), 30.0f);  // index 2 (valid)
    EXPECT_FLOAT_EQ(GetReg(11), 40.0f);  // index 3 (valid)
    EXPECT_FLOAT_EQ(GetReg(12), 0.0f);   // index 4 (OOB → 0)
    EXPECT_FLOAT_EQ(GetReg(13), 0.0f);   // index 5 (OOB → 0)
}

// ---------------------------------------------------------------------------
// Test: VS_ATTR VATTR buffer accumulation
// Verify that ATTR results are also written to the VATTR buffer
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_VATTRBufferAccumulation)
{
    float vbo[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    interp.SetVBO(vbo, 8);

    // ATTR at byte_offset=0
    Instruction attr = PatternVS::vs_attr(10, 0);
    interp.ExecuteInstruction(attr);

    // Check VATTR buffer (vertex 0, attr offset 0-3)
    EXPECT_FLOAT_EQ(interp.GetVAttrFloat(0, 0), 0.1f);
    EXPECT_FLOAT_EQ(interp.GetVAttrFloat(0, 1), 0.2f);
    EXPECT_FLOAT_EQ(interp.GetVAttrFloat(0, 2), 0.3f);
    EXPECT_FLOAT_EQ(interp.GetVAttrFloat(0, 3), 0.4f);
}

// ============================================================================
// Combined Pipeline Tests (Phase 3 specific)
// ============================================================================

// ---------------------------------------------------------------------------
// Test: VLOAD → VSTORE pipeline
// Load from VBO, modify, store back to memory
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, VLOAD_VSTORE_Pipeline)
{
    // VBO: [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
    float vbo[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    SetVBO(vbo, 8);

    // Step 1: VLOAD R4, #0 → R4-R7 = {1, 2, 3, 4}
    interp.ExecuteInstruction(PatternVS::vs_vload(4, 0));

    // Step 2: VSTORE R4, #16 → store R4 (value=1.0) to address 4*4+16=32
    // (using R4 as base, storing R4's value)
    interp.ExecuteInstruction(PatternVS::vs_vstore(4, 16, 4));

    // Check: address = 4*4 + 16 = 32
    EXPECT_EQ(float_bits(GetMemory(32)), float_bits(1.0f));
}

// ---------------------------------------------------------------------------
// Test: ATTR → VOUTPUT pipeline
// Load attribute, output as clip coordinates
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, ATTR_VOUTPUT_Pipeline)
{
    float vbo[] = {0.5f, 0.5f, 0.0f, 1.0f};
    interp.SetVBO(vbo, 4);

    // ATTR: load position into R10-R13
    interp.ExecuteInstruction(PatternVS::vs_attr(10, 0));

    // VOUTPUT: output R10-R13 as vertex clip coordinates
    interp.ExecuteInstruction(PatternVS::vs_voutput(10, 0));

    EXPECT_EQ(interp.GetVertexCount(), 1u);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 0), 0.5f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 1), 0.5f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 2), 0.0f);
    EXPECT_FLOAT_EQ(interp.GetVOutputFloat(0, 3), 1.0f);
}

// ---------------------------------------------------------------------------
// Test: LDC → MOV pipeline (stub verifying instruction sequencing)
// ---------------------------------------------------------------------------
TEST_F(VSISAPhase3Test, LDC_MOV_Pipeline)
{
    // LDC (stub) should advance PC; MOV should then copy constant to destination
    Instruction ldc = Instruction::MakeU(Opcode::LDC, 4, 0, 0);
    Instruction mov = PatternVS::vs_mov(5, 4);

    interp.ExecuteInstruction(ldc);
    EXPECT_EQ(interp.GetPC(), 4u);

    // After LDC stub, MOV copies (Rd of LDC=4 is still 0.0f since stub)
    // Note: In a real LDC, Rd would have the loaded constant value.
    // In the stub, Rd is unchanged (0.0f because R0=0.0f was used as base).
    // MOV R5, R4 → R5 = R4 = 0.0f
    interp.ExecuteInstruction(mov);
    EXPECT_EQ(interp.GetPC(), 8u);
    EXPECT_FLOAT_EQ(GetReg(5), 0.0f);  // R5 = R4 = 0.0f (R4 was never written by stub)
}

// ============================================================================
// Instruction encoding verification for Phase 3
// ============================================================================

TEST_F(VSISAPhase3Test, InstructionEncodings)
{
    EXPECT_EQ(PatternVS::vs_vload(0, 0).GetOpcode(), Opcode::VS_VLOAD);
    EXPECT_EQ(PatternVS::vs_vstore(0, 0, 0).GetOpcode(), Opcode::VS_VSTORE);
    EXPECT_EQ(PatternVS::vs_attr(0, 0).GetOpcode(), Opcode::VS_ATTR);
    EXPECT_EQ(PatternVS::vs_voutput(0, 0).GetOpcode(), Opcode::VS_VOUTPUT);
    EXPECT_EQ(Instruction::MakeU(Opcode::LDC, 0, 0, 0).GetOpcode(), Opcode::LDC);
}
