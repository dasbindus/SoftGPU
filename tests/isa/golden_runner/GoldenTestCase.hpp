/**
 * GoldenTestCase.hpp - Golden Test Case Format for SoftGPU ISA v2.5
 * 
 * Test Case Format (embedded YAML-like C++ DSL):
 * 
 * Each test case describes:
 *   - name / description
 *   - category (arithmetic, control_flow, memory, etc.)
 *   - initial register state (map reg# → float value)
 *   - initial memory state (map addr → float value)
 *   - program: list of raw 32-bit instruction words
 *   - expected final state: registers and memory
 *   - optional per-step expectations for single-step verification
 * 
 * The GoldenTestRunner executes the program against the Interpreter,
 * compares actual vs expected state, and reports coverage.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <optional>

namespace softgpu {
namespace isa {
namespace golden {

// ============================================================================
// Types
// ============================================================================

// Float-with-tolerance comparison for floating point results
struct FloatWithTol {
    float value;
    float abs_tol = 1e-6f;  // default tolerance
    float rel_tol = 1e-6f;
    
    FloatWithTol() : value(0.0f) {}
    explicit FloatWithTol(float v, float tol = 1e-6f) : value(v), abs_tol(tol) {}
};

struct RegState {
    std::unordered_map<uint8_t, float> values;
};

struct MemState {
    std::unordered_map<uint32_t, float> values;
};

// Expected final state after program execution
struct ExpectedState {
    std::unordered_map<uint8_t, FloatWithTol> regs;   // reg# → expected value
    std::unordered_map<uint32_t, FloatWithTol> mem;  // addr → expected value
    int expected_cycles = -1;                         // -1 = don't check
    int expected_instructions = -1;                   // -1 = don't check
    bool expect_halt = false;
};

// A single instruction in the test program
struct ProgramInstruction {
    uint32_t raw;           // raw 32-bit encoding
    std::string comment;     // e.g. "ADD R1, R2, R3"
};

// Per-step expectation (for single-step verification)
struct StepExpectation {
    int step;                // step number (0 = after first instruction)
    std::unordered_map<uint8_t, FloatWithTol> regs;
    uint32_t expected_pc = 0xFFFFFFFF;
};

// Test case category
enum class Category {
    CONTROL_FLOW,
    ARITHMETIC,
    MEMORY,
    BITWISE,
    SFU,
    VS,
    SCOREBOARD,
    UNKNOWN
};

inline Category CategoryFromString(const std::string& s) {
    if (s == "control_flow") return Category::CONTROL_FLOW;
    if (s == "arithmetic") return Category::ARITHMETIC;
    if (s == "memory") return Category::MEMORY;
    if (s == "bitwise") return Category::BITWISE;
    if (s == "sfu") return Category::SFU;
    if (s == "vs") return Category::VS;
    if (s == "scoreboard") return Category::SCOREBOARD;
    return Category::UNKNOWN;
}

inline const char* CategoryToString(Category c) {
    switch (c) {
        case Category::CONTROL_FLOW: return "control_flow";
        case Category::ARITHMETIC:   return "arithmetic";
        case Category::MEMORY:        return "memory";
        case Category::BITWISE:       return "bitwise";
        case Category::SFU:           return "sfu";
        case Category::VS:            return "vs";
        case Category::SCOREBOARD:    return "scoreboard";
        default:                      return "unknown";
    }
}

// ============================================================================
// Test Case Definition
// ============================================================================

struct TestCase {
    std::string name;           // unique test name, e.g. "add_positive"
    std::string description;    // human-readable description
    Category category = Category::UNKNOWN;
    
    // Initial state
    std::unordered_map<uint8_t, float> initial_regs;
    std::unordered_map<uint32_t, float> initial_mem;
    
    // Program
    std::vector<ProgramInstruction> program;
    
    // Expected final state
    ExpectedState expected;
    
    // Optional per-step expectations
    std::vector<StepExpectation> step_expectations;
    
    // P0/P1 classification
    bool is_p0 = false;
    
    // Tags for filtering
    std::vector<std::string> tags;
};

// ============================================================================
// Coverage Tracker
// ============================================================================

struct CoverageStats {
    std::unordered_map<uint8_t, int> opcode_coverage_count;   // opcode → # of tests covering it
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;
    
    void RecordTest(uint8_t opcode, bool passed) {
        total_tests++;
        opcode_coverage_count[opcode]++;
        if (passed) passed_tests++;
        else failed_tests++;
    }
};

// Get opcode coverage percentage for a given category of opcodes
double GetCoverageRate(const CoverageStats& stats, 
                       const std::vector<uint8_t>& p0_opcodes);

} // namespace golden
} // namespace isa
} // namespace softgpu
