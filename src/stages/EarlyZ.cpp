// ============================================================================
// SoftGPU - EarlyZ.cpp
// Early Depth Test Implementation
// v1.4 Phase - P1-2
// ============================================================================

#include "EarlyZ.hpp"

namespace SoftGPU {

EarlyZ::EarlyZ() = default;

bool EarlyZ::testFragment(float fragDepth, float depthBufferValue) {
    // Fragment passes if it is closer to camera than current depth buffer value
    // i.e., fragDepth < depthBufferValue (smaller depth = closer in OpenGL)
    if (fragDepth < depthBufferValue) {
        m_passedCount++;
        return true;
    } else {
        m_rejectedCount++;
        return false;
    }
}

std::vector<Fragment> EarlyZ::filterOccluded(const std::vector<Fragment>& fragments,
                                              const float* depthBuffer) {
    std::vector<Fragment> passed;

    // Assumes width = 1920 for depth buffer indexing
    // TODO: make width configurable if needed
    constexpr uint32_t WIDTH = 1920;

    for (const auto& frag : fragments) {
        uint32_t idx = frag.y * WIDTH + frag.x;
        float depthBufferValue = depthBuffer[idx];

        if (testFragment(frag.depth, depthBufferValue)) {
            passed.push_back(frag);
        }
    }

    return passed;
}

void EarlyZ::resetStats() {
    m_rejectedCount = 0;
    m_passedCount = 0;
}

} // namespace SoftGPU
