/**
 * GoldenTestRunner.hpp - Drives ISA instruction execution and validates results
 * 
 * Phase 1 Test Framework for SoftGPU ISA v2.5
 * 
 * Workflow:
 *   1. Parse test case (initial state + program + expected state)
 *   2. Initialize Interpreter with program
 *   3. Set initial register/memory state
 *   4. Execute program (Step() or Run())
 *   5. Compare actual vs expected register/memory state
 *   6. Report pass/fail with details
 */

#pragma once

#include "GoldenTestCase.hpp"
#include "../../../src/isa/Interpreter.hpp"
#include <iostream>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace softgpu {
namespace isa {
namespace golden {

// ============================================================================
// Floating Point Comparison
// ============================================================================

inline bool FloatEquals(float actual, float expected, float abs_tol, float rel_tol) {
    if (std::isnan(expected)) {
        return std::isnan(actual);
    }
    if (std::isinf(expected)) {
        return std::isinf(actual) && (std::signbit(expected) == std::signbit(actual));
    }
    float diff = std::fabs(actual - expected);
    if (diff <= abs_tol) return true;
    if (rel_tol > 0 && diff <= rel_tol * std::fabs(expected)) return true;
    return false;
}

inline bool FloatEquals(const FloatWithTol& f, float actual) {
    return FloatEquals(actual, f.value, f.abs_tol, f.rel_tol);
}

// ============================================================================
// Result Type
// ============================================================================

struct TestResult {
    bool passed = false;
    std::string message;        // failure details
    std::vector<std::string> errors;  // list of specific mismatches
    
    // Execution info
    uint64_t cycles_taken = 0;
    uint64_t instructions_taken = 0;
    uint32_t final_pc = 0;
};

// ============================================================================
// Golden Test Runner
// ============================================================================

class GoldenTestRunner {
public:
    explicit GoldenTestRunner(const TestCase& tc) : tc_(tc) {}
    
    /**
     * Execute the test case and return the result.
     * 
     * Steps:
     *  1. Set initial register state
     *  2. Set initial memory state
     *  3. Load program into interpreter
     *  4. Execute (Step() for single-step, or Run() for full)
     *  5. Compare state
     */
    TestResult Run() {
        TestResult result;
        
        Interpreter interp;
        
        // --- 1. Set initial register state ---
        for (const auto& [reg, val] : tc_.initial_regs) {
            interp.SetRegister(static_cast<uint8_t>(reg), val);
        }
        
        // --- 2. Set initial memory state ---
        for (const auto& [addr, val] : tc_.initial_mem) {
            interp.SetMemory(addr, val);
        }
        
        // --- 3. Build program vector ---
        std::vector<uint32_t> program_words;
        for (const auto& inst : tc_.program) {
            program_words.push_back(inst.raw);
        }
        
        // --- 3b. Load program into interpreter's program memory ---
        // The Interpreter uses m_program (vector<uint32_t>), set via LoadProgram or internal method
        // For the current Interpreter, we need to use a helper or friend class
        // We'll use ExecuteInstruction directly with a helper
        
        // --- 4. Execute program ---
        if (tc_.step_expectations.empty()) {
            // Full execution mode
            result = ExecuteFull(interp, program_words);
        } else {
            // Single-step mode with per-step verification
            result = ExecuteSingleStep(interp, program_words);
        }
        
        return result;
    }

private:
    const TestCase& tc_;
    
    // Helper: load program into interpreter
    // Note: We use a raw pointer approach since Interpreter doesn't expose m_program setter
    // We'll create the program using a workaround: Step() expects m_program to be set
    // For the current implementation, we'll use a simplified approach by calling
    // ExecuteInstruction directly in a loop, simulating the fetch/decode/execute cycle
    
    TestResult ExecuteFull(Interpreter& interp, const std::vector<uint32_t>& program) {
        TestResult result;
        
        // Simulate fetch+execute by directly calling ExecuteInstruction in a loop
        // This bypasses the m_program loading issue
        uint32_t pc = 0;
        int max_steps = 10000;
        int steps = 0;
        
        while (pc < program.size() * 4 && steps < max_steps) {
            uint32_t word_idx = pc / 4;
            if (word_idx >= program.size()) break;
            
            Instruction inst(program[word_idx]);
            Opcode op = inst.GetOpcode();
            
            if (op == Opcode::INVALID) {
                result.errors.push_back("Invalid opcode at PC=0x" + ToHex(pc));
                break;
            }
            
            interp.ExecuteInstruction(inst);
            pc = interp.GetPC();
            steps++;
            
            if (op == Opcode::VS_HALT || op == Opcode::RET) {
                if (op == Opcode::VS_HALT) {
                    result.final_pc = pc;
                    break;
                }
                // RET: need link register - the interpreter handles this
                // For full execution, continue until HALT or max steps
            }
        }
        
        if (steps >= max_steps) {
            result.errors.push_back("Exceeded max steps (" + std::to_string(max_steps) + ")");
        }
        
        result.cycles_taken = interp.GetStats().cycles;
        result.instructions_taken = interp.GetStats().instructions_executed;
        result.final_pc = pc;
        
        // --- 5. Compare state ---
        ValidateState(interp, result);
        
        return result;
    }
    
    TestResult ExecuteSingleStep(Interpreter& interp, const std::vector<uint32_t>& program) {
        TestResult result;
        
        uint32_t pc = 0;
        int step = 0;
        
        while (pc < program.size() * 4 && step < (int)tc_.step_expectations.size()) {
            uint32_t word_idx = pc / 4;
            if (word_idx >= program.size()) break;
            
            Instruction inst(program[word_idx]);
            Opcode op = inst.GetOpcode();
            
            if (op == Opcode::INVALID) {
                result.errors.push_back("Invalid opcode at PC=0x" + ToHex(pc));
                break;
            }
            
            interp.ExecuteInstruction(inst);
            pc = interp.GetPC();
            step++;
            
            // Check per-step expectation
            bool matched = true;
            for (const auto& expect : tc_.step_expectations) {
                if (expect.step == step) {
                    for (const auto& [reg, expected_val] : expect.regs) {
                        float actual = interp.GetRegister(static_cast<uint8_t>(reg));
                        if (!FloatEquals(expected_val, actual)) {
                            std::ostringstream oss;
                            oss << "Step " << step << ": R" << (int)reg 
                                << " expected=" << expected_val.value 
                                << " actual=" << actual;
                            result.errors.push_back(oss.str());
                            matched = false;
                        }
                    }
                    if (expect.expected_pc != 0xFFFFFFFF && expect.expected_pc != pc) {
                        std::ostringstream oss;
                        oss << "Step " << step << ": PC expected=0x" << ToHex(expect.expected_pc) 
                            << " actual=0x" << ToHex(pc);
                        result.errors.push_back(oss.str());
                        matched = false;
                    }
                }
            }
            
            if (op == Opcode::VS_HALT) break;
            if (op == Opcode::RET && pc == 0) break;  // returned to 0, done
        }
        
        result.cycles_taken = interp.GetStats().cycles;
        result.instructions_taken = interp.GetStats().instructions_executed;
        result.final_pc = pc;
        
        ValidateState(interp, result);
        
        return result;
    }
    
    void ValidateState(Interpreter& interp, TestResult& result) {
        // Check expected registers
        for (const auto& [reg, expected_val] : tc_.expected.regs) {
            float actual = interp.GetRegister(static_cast<uint8_t>(reg));
            if (!FloatEquals(expected_val, actual)) {
                std::ostringstream oss;
                oss << "R" << (int)reg << ": expected=" << expected_val.value 
                    << " (tol=" << expected_val.abs_tol << "), actual=" << actual;
                result.errors.push_back(oss.str());
            }
        }
        
        // Check expected memory
        for (const auto& [addr, expected_val] : tc_.expected.mem) {
            float actual = interp.GetMemory(addr);
            if (!FloatEquals(expected_val, actual)) {
                std::ostringstream oss;
                oss << "M[0x" << ToHex(addr) << "]: expected=" << expected_val.value 
                    << ", actual=" << actual;
                result.errors.push_back(oss.str());
            }
        }
        
        // Check expected cycles
        if (tc_.expected.expected_cycles >= 0) {
            if ((int)result.cycles_taken != tc_.expected.expected_cycles) {
                std::ostringstream oss;
                oss << "cycles: expected=" << tc_.expected.expected_cycles 
                    << ", actual=" << result.cycles_taken;
                result.errors.push_back(oss.str());
            }
        }
        
        // Check expected instruction count
        if (tc_.expected.expected_instructions >= 0) {
            if ((int)result.instructions_taken != tc_.expected.expected_instructions) {
                std::ostringstream oss;
                oss << "instructions: expected=" << tc_.expected.expected_instructions 
                    << ", actual=" << result.instructions_taken;
                result.errors.push_back(oss.str());
            }
        }
        
        // Overall pass/fail
        result.passed = result.errors.empty();
        
        if (!result.passed) {
            std::ostringstream oss;
            for (size_t i = 0; i < result.errors.size(); ++i) {
                if (i > 0) oss << "; ";
                oss << result.errors[i];
            }
            result.message = oss.str();
        } else {
            result.message = "OK";
        }
    }
    
    static std::string ToHex(uint32_t v) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << v << std::dec;
        return oss.str();
    }
};

// ============================================================================
// P0 Opcodes (ISA v2.5 - Phase 1)
// ============================================================================

// These are the 15 P0 instructions for Phase 1 golden testing
// Organized by functional group

// Control Flow (Format-D J-type): NOP, HALT
// Control Flow (Format-B double-word): BRA, JMP, CALL, RET
// Arithmetic (Format-A R-type): ADD, SUB, MUL, DIV, MAD, CMP, MIN, MAX
// Bitwise (Format-A R-type): AND, OR

constexpr uint8_t OP_NOP   = 0x00;
constexpr uint8_t OP_ADD   = 0x01;
constexpr uint8_t OP_SUB   = 0x02;
constexpr uint8_t OP_MUL   = 0x03;
constexpr uint8_t OP_DIV   = 0x04;
constexpr uint8_t OP_MAD   = 0x05;
constexpr uint8_t OP_CMP   = 0x0B;
constexpr uint8_t OP_MIN   = 0x0D;
constexpr uint8_t OP_MAX   = 0x0E;
constexpr uint8_t OP_AND   = 0x09;
constexpr uint8_t OP_OR    = 0x0A;
constexpr uint8_t OP_BRA   = 0x11;  // Format-B double-word
constexpr uint8_t OP_JMP   = 0x12;  // Format-B double-word  
constexpr uint8_t OP_CALL  = 0x13;  // Format-B double-word
constexpr uint8_t OP_RET   = 0x14;  // Format-D
constexpr uint8_t OP_HALT  = 0xFF;  // HALT - special

// Double-word instructions (Format-B)
// LD, ST, VLOAD, VSTORE, LDC, MOV_IMM
constexpr uint8_t OP_LD       = 0x0F;
constexpr uint8_t OP_ST       = 0x10;
constexpr uint8_t OP_VLOAD    = 0x29;  // VS_VLOAD in implementation
constexpr uint8_t OP_VSTORE   = 0x4A;  // VS_VSTORE
constexpr uint8_t OP_LDC     = 0x1B;
constexpr uint8_t OP_MOV_IMM = 0x64;  // VS_MOV_IMM

} // namespace golden
} // namespace isa
} // namespace softgpu
