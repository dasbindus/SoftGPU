// ============================================================================
// SoftGPU - ShaderRegs.hpp
// Shared shader register definitions for v2.5 ISA
// ============================================================================

#pragma once

#include <cstdint>

namespace ShaderRegs {
    // Input registers (R1-R9)
    constexpr uint8_t FRAG_X = 1;
    constexpr uint8_t FRAG_Y = 2;
    constexpr uint8_t FRAG_Z = 3;
    constexpr uint8_t COLOR_R = 4;
    constexpr uint8_t COLOR_G = 5;
    constexpr uint8_t COLOR_B = 6;
    constexpr uint8_t COLOR_A = 7;
    constexpr uint8_t TEX_U = 8;
    constexpr uint8_t TEX_V = 9;

    // Output registers (R10-R15)
    constexpr uint8_t OUT_R = 10;
    constexpr uint8_t OUT_G = 11;
    constexpr uint8_t OUT_B = 12;
    constexpr uint8_t OUT_A = 13;
    constexpr uint8_t OUT_Z = 14;
    constexpr uint8_t KILLED = 15;

    // Temporary registers (R16-R31)
    constexpr uint8_t TMP0 = 16;
    constexpr uint8_t TMP1 = 17;
    constexpr uint8_t TMP2 = 18;

    // Shader argument base
    constexpr uint8_t ARG_BASE = 1;
}
