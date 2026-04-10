// =============================================================================
// SoftGPU ISA v2.5 - Interpreter Test (Phase 1, basic skeleton)
// =============================================================================

#include "src/isa/interpreter_v2_5.hpp"
#include <cstdio>
#include <cassert>
#include <cmath>
#include <vector>

using namespace softgpu::isa::v2_5;
using v2_5::Instruction;
using v2_5::Opcode;

static void test_nop_halt() {
    printf("=== test_nop_halt ===\n");
    Interpreter interp;
    uint32_t program[] = { 0x00000000u, 0x00000000u, 0x0F000000u };
    interp.LoadProgram(program, 3);
    interp.Run(100);
    assert(interp.GetStats().instructions_executed == 3);
    assert(interp.GetPC() == 0xC);
    printf("  PASS\n");
}

static void test_add() {
    printf("=== test_add ===\n");
    Interpreter interp;
    Instruction add = Instruction::MakeA(Opcode::ADD, 1, 2, 3);
    uint32_t program[] = { add.word1, 0x0F000000u };
    interp.LoadProgram(program, 2);
    interp.SetRegister(2, 3.0f);
    interp.SetRegister(3, 5.0f);
    interp.Run(100);
    float r1 = interp.GetRegister(1);
    printf("  R1 = %.4f (expected 8.0000)\n", r1);
    assert(std::fabs(r1 - 8.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_sub() {
    printf("=== test_sub ===\n");
    Interpreter interp;
    Instruction sub = Instruction::MakeA(Opcode::SUB, 1, 2, 3);
    uint32_t program[] = { sub.word1, 0x0F000000u };
    interp.LoadProgram(program, 2);
    interp.SetRegister(2, 10.0f);
    interp.SetRegister(3, 3.0f);
    interp.Run(100);
    float r1 = interp.GetRegister(1);
    printf("  R1 = %.4f (expected 7.0000)\n", r1);
    assert(std::fabs(r1 - 7.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_mul() {
    printf("=== test_mul ===\n");
    Interpreter interp;
    Instruction mul = Instruction::MakeA(Opcode::MUL, 1, 2, 3);
    uint32_t program[] = { mul.word1, 0x0F000000u };
    interp.LoadProgram(program, 2);
    interp.SetRegister(2, 6.0f);
    interp.SetRegister(3, 7.0f);
    interp.Run(100);
    float r1 = interp.GetRegister(1);
    printf("  R1 = %.4f (expected 42.0000)\n", r1);
    assert(std::fabs(r1 - 42.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_min_max() {
    printf("=== test_min_max ===\n");
    Interpreter interp;

    Instruction min_i = Instruction::MakeA(Opcode::MIN, 1, 2, 3);
    uint32_t prog_min[] = { min_i.word1, 0x0F000000u };
    interp.LoadProgram(prog_min, 2);
    interp.SetRegister(2, 8.0f);
    interp.SetRegister(3, 3.0f);
    interp.Run(100);
    printf("  MIN: R1 = %.4f (expected 3.0000) %s\n",
           interp.GetRegister(1),
           (std::fabs(interp.GetRegister(1) - 3.0f) < 0.01 ? "PASS" : "FAIL");
    assert(std::fabs(interp.GetRegister(1) - 3.0f) < 0.01f);

    Instruction max_i = Instruction::MakeA(Opcode::MAX, 1, 2, 3);
    uint32_t prog_max[] = { max_i.word1, 0x0F000000u };
    interp.LoadProgram(prog_max, 2);
    interp.SetRegister(2, 8.0f);
    interp.SetRegister(3, 3.0f);
    interp.Run(100);
    printf("  MAX: R1 = %.4f (expected 8.0000) %s\n",
           interp.GetRegister(1),
           (std::fabs(interp.GetRegister(1) - 8.0f) < 0.01 ? "PASS" : "FAIL");
    assert(std::fabs(interp.GetRegister(1) - 8.0f) < 0.01f);
    printf("  PASS\n");
}

static void test_and_or() {
    printf("=== test_and_or ===\n");
    Interpreter interp;
    Instruction and_i = Instruction::MakeA(Opcode::AND, 1, 2, 3);
    uint32_t prog[] = { and_i.word1, 0x0F000000u };
    interp.LoadProgram(prog, 2);
    uint32_t r2v = 0xFFFFFFFFu, r3v = 0xFFFF0000u;
    interp.SetRegister(2, reinterpret_cast<float&>(r2v));
    interp.SetRegister(3, reinterpret_cast<float&>(r3v));
    interp.Run(100);
    float r1f = interp.GetRegister(1);
    uint32_t r1bits = reinterpret_cast<uint32_t&>(r1f);
    printf("  AND: R1 = 0x%08X (expected 0xFFFF0000) %s\n",
           r1bits, (r1bits == 0xFFFF0000u) ? "PASS" : "FAIL");
    assert(r1bits == 0xFFFF0000u);
    printf("  PASS\n");
}

static void test_mov_imm() {
    printf("=== test_mov_imm ===\n");
    Interpreter interp;
    Instruction mov = Instruction::MakeB(Opcode::MOV_IMM, 5, 0, 0, 42);
    uint32_t prog[] = { mov.word1, mov.word2, 0x0F000000u };
    interp.LoadProgram(prog, 3);
    interp.Run(100);
    float r5 = interp.GetRegister(5);
    printf("  R5 = %.4f (expected 42.0000)\n", r5);
    assert(std::fabs(r5 - 42.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_bra_taken() {
    printf("=== test_bra_taken ===\n");
    // BRA offset=1: target = next_addr(8) + 1*4 = 12 = MUL
    // Program: BRA; HALT; MUL; HALT
    // Layout: [0,1]=BRA, [2]=HALT, [3]=MUL, [4]=HALT
    Instruction bra = Instruction::MakeBRA(2, 1);
    Instruction mul = Instruction::MakeA(Opcode::MUL, 1, 3, 4);
    std::vector<uint32_t> prog;
    prog.push_back(bra.word1);
    prog.push_back(bra.word2);
    prog.push_back(0x0F000000u);  // HALT at PC=8 (skipped)
    prog.push_back(mul.word1);     // MUL at PC=12 (jumped to)
    prog.push_back(0x0F000000u);  // HALT at PC=16
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(2, 1.0f);   // condition = true (branch taken)
    interp.SetRegister(3, 3.0f);
    interp.SetRegister(4, 4.0f);
    interp.Run(100);
    float r1 = interp.GetRegister(1);
    printf("  R1 = %.4f (expected 12.0000)\n", r1);
    assert(std::fabs(r1 - 12.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_bra_not_taken() {
    printf("=== test_bra_not_taken ===\n");
    // BRA offset=1: if not taken, fall through to PC=8 (ADD)
    Instruction bra = Instruction::MakeBRA(2, 1);
    Instruction add = Instruction::MakeA(Opcode::ADD, 1, 3, 4);
    std::vector<uint32_t> prog;
    prog.push_back(bra.word1);
    prog.push_back(bra.word2);
    prog.push_back(add.word1);     // ADD at PC=8 (executed)
    prog.push_back(0x0F000000u);  // HALT
    interp.LoadProgram(prog.data(), prog.size());
    interp.SetRegister(2, 0.0f);   // condition = false (not taken)
    interp.SetRegister(3, 3.0f);
    interp.SetRegister(4, 7.0f);
    interp.Run(100);
    float r1 = interp.GetRegister(1);
    printf("  R1 = %.4f (expected 10.0000)\n", r1);
    assert(std::fabs(r1 - 10.0f) < 0.001f);
    printf("  PASS\n");
}

static void test_u_type() {
    printf("=== test_u_type ===\n");
    Interpreter interp;
    Instruction abs_i = Instruction::MakeC(Opcode::ABS, 1, 2);
    Instruction neg_i = Instruction::MakeC(Opcode::NEG, 2, 3);
    Instruction floor_i = Instruction::MakeC(Opcode::FLOOR, 3, 4);
    uint32_t prog[] = { abs_i.word1, neg_i.word1, floor_i.word1, 0x0F000000u };
    interp.LoadProgram(prog, 4);
    interp.SetRegister(2, -5.0f);
    interp.SetRegister(3, -3.0f);
    interp.SetRegister(4, 3.7f);
    interp.Run(100);
    printf("  ABS(-5.0) = %.4f (expected 5.0000) %s\n",
           interp.GetRegister(1),
           (std::fabs(interp.GetRegister(1) - 5.0f) < 0.01 ? "PASS" : "FAIL");
    printf("  NEG(-3.0) = %.4f (expected 3.0000) %s\n",
           interp.GetRegister(2),
           (std::fabs(interp.GetRegister(2) - 3.0f) < 0.01 ? "PASS" : "FAIL");
    printf("  FLOOR(3.7) = %.4f (expected 3.0000) %s\n",
           interp.GetRegister(3),
           (std::fabs(interp.GetRegister(3) - 3.0f) < 0.01 ? "PASS" : "FAIL");
    assert(std::fabs(interp.GetRegister(1) - 5.0f) < 0.01f);
    assert(std::fabs(interp.GetRegister(2) - 3.0f) < 0.01f);
    assert(std::fabs(interp.GetRegister(3) - 3.0f) < 0.01f);
    printf("  PASS\n");
}

static void test_dot3() {
    printf("=== test_dot3 ===\n");
    Interpreter interp;
    Instruction dot3 = Instruction::MakeA(Opcode::DOT3, 1, 2, 3);
    uint32_t prog[] = { dot3.word1, 0x0F000000u };
    interp.LoadProgram(prog, 2);
    interp.SetRegister(2, 1.0f);  // R2.xyz = (1,2,3)
    interp.SetRegister(3, 2.0f);
    interp.SetRegister(4, 3.0f);
    interp.SetRegister(5, 4.0f);  // R3.xyz = (4,5,6)
    interp.SetRegister(6, 5.0f);
    interp.SetRegister(7, 6.0f);
    // DOT3 = 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    interp.Run(100);
    printf("  DOT3([1,2,3],[4,5,6]) = %.4f (expected 32.0000)\n", interp.GetRegister(1));
    assert(std::fabs(interp.GetRegister(1) - 32.0f) < 0.01f);
    printf("  PASS\n");
}

int main() {
    printf("========================================\n");
    printf("SoftGPU ISA v2.5 - Basic Tests\n");
    printf("========================================\n");
    test_nop_halt();
    test_add();
    test_sub();
    test_mul();
    test_min_max();
    test_and_or();
    test_mov_imm();
    test_bra_taken();
    test_bra_not_taken();
    test_u_type();
    test_dot3();
    printf("\nAll tests PASSED!\n");
    return 0;
}
