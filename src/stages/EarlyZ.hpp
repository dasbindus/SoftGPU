// ============================================================================
// SoftGPU - EarlyZ.hpp
// Early Depth Test - Depth pre-testing before FragmentShader
// v1.4 Phase - P1-2
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>

namespace SoftGPU {

struct Fragment {
    float depth;
    uint32_t x, y;
    uint32_t shader_id;
    uint32_t pc;
};

class EarlyZ {
public:
    EarlyZ();

    // Single pixel depth pre-test
    // Returns true if fragment passes (is in front of current depth buffer value)
    // Returns false if fragment is occluded (behind current depth buffer value)
    bool testFragment(float fragDepth, float depthBufferValue);

    // Batch filter: filter out occluded fragments
    std::vector<Fragment> filterOccluded(const std::vector<Fragment>& fragments,
                                          const float* depthBuffer);

    const char* getName() const { return "EarlyZ"; }

    uint32_t getRejectedCount() const { return m_rejectedCount; }
    uint32_t getPassedCount() const { return m_passedCount; }
    void resetStats();

private:
    uint32_t m_rejectedCount = 0;
    uint32_t m_passedCount = 0;
};

} // namespace SoftGPU
