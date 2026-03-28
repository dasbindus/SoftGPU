#pragma once

// ============================================================================
// SoftGPU ISA - Instruction Set Architecture
// ============================================================================
// 
// v1.0 ISA - 28 Instructions
// 
// This is the main header for the ISA framework. Include this to get
// access to all ISA components.
// 
// Usage:
//   #include "isa/ISA.hpp"
//   using namespace softgpu::isa;
// 
// ============================================================================

#include "Opcode.hpp"
#include "Instruction.hpp"
#include "Decoder.hpp"
#include "RegisterFile.hpp"
#include "ExecutionUnits.hpp"
#include "Interpreter.hpp"

// ============================================================================
// ISA Version Info
// ============================================================================

#define ISA_VERSION_MAJOR 1
#define ISA_VERSION_MINOR 0
#define ISA_VERSION_PATCH 0

#define ISA_VERSION_STRING "1.0.0"
#define ISA_INSTRUCTION_COUNT 28

// ============================================================================
// Quick Reference
// ============================================================================
//
// Instruction Format: 32-bit fixed length
//   31    25 24    20 19    15 14    10 9              0
//  +--------+-------+-------+-------+------------------+
//  | Opcode |  Rd   |  Ra   |  Rb   |   Immediate      |
//  | 7 bit  | 5 bit | 5 bit | 5 bit |     10 bit       |
//  +--------+-------+-------+-------+------------------+
//
// Registers: R0-R63 (5-bit field)
//   R0 = Zero Register (hardwired to 0.0f)
//   R1-R7 = Argument registers (A0-A7)
//   R8-R15 = Temporary registers (T0-T7)
//   R16-R63 = General purpose
//
// Instruction Types:
//   R   - 3 register operands (ADD, SUB, MUL, etc.)
//   R4  - 4 register operands (MAD, SEL)
//   U   - 1 register + immediate (MOV, RCP, etc.)
//   I   - Memory: base + offset (LD, ST)
//   B   - Conditional branch (BRA)
//   J   - Jump (JMP, CALL, RET, NOP, BAR)
//
// Cycles by Category:
//   ALU:   1 cycle  (ADD, SUB, MUL, AND, OR, CMP, SEL, MIN, MAX, MOV)
//   SFU:   2-7 cycles (RCP=4, SQRT=4, RSQ=2, DIV=7, F2I=1, I2F=1, FRACT=1)
//   MEM:   4 cycles (LD, ST)
//   CTRL:  1-3 cycles (BRA=1, JMP=2, CALL=3, RET=2, NOP=1)
//   TEX:   4-8 cycles (TEX=8, SAMPLE=4, LDC=2, BAR=1)
//
// ============================================================================

namespace softgpu {
namespace isa {

// Static assertions to verify sizes at compile time
static_assert(sizeof(float) == 4, "ISA requires 32-bit float");
static_assert(sizeof(uint32_t) == 4, "ISA requires 32-bit word");

// Register count
constexpr size_t kNumRegisters = 64;
constexpr uint8_t kZeroRegister = 0;

// Instruction encoding
constexpr size_t kOpcodeBits = 7;
constexpr size_t kRegBits = 5;
constexpr size_t kImmBits = 10;

// Instruction word size
constexpr size_t kInstructionSize = 4; // bytes

} // namespace isa
} // namespace softgpu

// ============================================================================
// Example Usage
// ============================================================================
//
//   #include "isa/ISA.hpp"
//   
//   void example() {
//       using namespace softgpu::isa;
//       
//       // Create an instruction
//       Instruction add_inst = Instruction::MakeR(Opcode::ADD, 3, 1, 2);
//       
//       // Decode
//       Decoder decoder;
//       Instruction decoded = decoder.Decode(add_inst.raw);
//       
//       // Execute
//       Interpreter interp;
//       interp.SetRegister(1, 10.0f);
//       interp.SetRegister(2, 20.0f);
//       interp.ExecuteInstruction(decoded);
//       float result = interp.GetRegister(3); // Should be 30.0f
//   }
//
// ============================================================================
