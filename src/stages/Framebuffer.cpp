// ============================================================================
// SoftGPU - Framebuffer.cpp
// ============================================================================

#include "Framebuffer.hpp"

#include <chrono>
#include <algorithm>

namespace SoftGPU {

Framebuffer::Framebuffer() {
    m_colorBuffer.resize(PIXEL_COUNT * 4, 0.0f);
    m_depthBuffer.resize(PIXEL_COUNT, CLEAR_DEPTH);
    resetCounters();
}

Framebuffer::~Framebuffer() {
}

void Framebuffer::setInput(const std::vector<Fragment>& fragments) {
    // For unit tests: always copy
    m_inputFragments = fragments;
    m_inputVersion = 2;
}

void Framebuffer::setInputFromConnect(const std::vector<Fragment>& fragments) {
    m_inputFragmentsPtr = &fragments;
    m_inputFragments = fragments;
    m_inputVersion = 1;
}

void Framebuffer::clear(const float* clearColor) {
    float cc[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (clearColor) {
        cc[0] = clearColor[0];
        cc[1] = clearColor[1];
        cc[2] = clearColor[2];
        cc[3] = clearColor[3];
    }

    for (size_t i = 0; i < PIXEL_COUNT; ++i) {
        m_colorBuffer[i * 4 + 0] = cc[0];
        m_colorBuffer[i * 4 + 1] = cc[1];
        m_colorBuffer[i * 4 + 2] = cc[2];
        m_colorBuffer[i * 4 + 3] = cc[3];
        m_depthBuffer[i] = CLEAR_DEPTH;
    }
}

void Framebuffer::execute() {
    auto start = std::chrono::high_resolution_clock::now();

    // Use pointer from connectStages only if setInput has not been called after connect
    const std::vector<Fragment>& input = (m_inputVersion == 1 && m_inputFragmentsPtr != nullptr)
        ? *m_inputFragmentsPtr
        : m_inputFragments;

    uint64_t depthTests = 0;
    uint64_t depthRejects = 0;
    uint64_t writes = 0;

    for (const auto& frag : input) {
        uint32_t x = frag.x;
        uint32_t y = frag.y;

        if (x >= WIDTH || y >= HEIGHT) continue;

        depthTests++;
        bool pass = depthTest(x, y, frag.z);

        if (pass) {
            float color[4] = {frag.r, frag.g, frag.b, frag.a};
            writePixel(x, y, frag.z, color);
            writes++;
        } else {
            depthRejects++;
        }
    }

    m_counters.invocation_count = static_cast<uint64_t>(input.size());
    m_counters.extra_count0 = writes;       // writes
    m_counters.extra_count1 = depthTests;  // depth_tests
    m_counters.cycle_count  = depthRejects; // depth_rejects

    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms =
        std::chrono::duration<double, std::milli>(end - start).count();
}

void Framebuffer::resetCounters() {
    m_counters = PerformanceCounters{};
}

bool Framebuffer::depthTest(uint32_t x, uint32_t y, float z) {
    if (!m_depthTestEnabled) return true;

    size_t idx = y * WIDTH + x;
    float oldZ = m_depthBuffer[idx];
    return z > oldZ;  // Larger Z = closer to camera (fix: green z=-0.3 should cover red z=0.3)
}

void Framebuffer::writePixel(uint32_t x, uint32_t y, float z, const float color[4]) {
    size_t idx = y * WIDTH + x;

    // Write color
    m_colorBuffer[idx * 4 + 0] = color[0];
    m_colorBuffer[idx * 4 + 1] = color[1];
    m_colorBuffer[idx * 4 + 2] = color[2];
    m_colorBuffer[idx * 4 + 3] = color[3];

    // Write depth
    if (m_depthWriteEnabled) {
        m_depthBuffer[idx] = z;
    }
}

}  // namespace SoftGPU
