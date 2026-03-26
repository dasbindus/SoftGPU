// ============================================================================
// SoftGPU - FrameDumper.cpp
// 帧导出工具实现
// ============================================================================

#include "FrameDumper.hpp"
#include <cstdio>
#include <cmath>
#include <sys/stat.h>  // mkdir
#include <cstring>    // strlen

namespace SoftGPU {

void FrameDumper::setOutputPath(const std::string& path) {
    m_outputPath = path;
    if (!m_outputPath.empty() && m_outputPath.back() != '/' && m_outputPath.back() != '\\') {
        m_outputPath += '/';
    }
}

void FrameDumper::dumpPPM(const float* colorBuffer,
                          uint32_t width, uint32_t height,
                          const std::string& filename) const {
    // 拼接完整路径
    std::string fullPath = m_outputPath.empty() ? filename : m_outputPath + filename;

    FILE* f = fopen(fullPath.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "[FrameDumper] ERROR: Cannot open file for writing: %s\n", fullPath.c_str());
        return;
    }

    // PPM header: P6 = binary RGB, 640 480, 255
    fprintf(f, "P6\n%d %d\n255\n", width, height);

    // 写入 binary pixel data: R G B R G B ...
    // colorBuffer format: [R, G, B, A, R, G, B, A, ...] (float [0,1])
    for (uint32_t i = 0; i < width * height; i++) {
        uint8_t r = static_cast<uint8_t>(
            std::min(1.0f, std::max(0.0f, colorBuffer[i * 4 + 0])) * 255.0f);
        uint8_t g = static_cast<uint8_t>(
            std::min(1.0f, std::max(0.0f, colorBuffer[i * 4 + 1])) * 255.0f);
        uint8_t b = static_cast<uint8_t>(
            std::min(1.0f, std::max(0.0f, colorBuffer[i * 4 + 2])) * 255.0f);

        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }

    fclose(f);
    printf("[FrameDumper] Dumped frame to: %s (%dx%d)\n", fullPath.c_str(), width, height);
}

void FrameDumper::dumpFrame(const float* colorBuffer,
                             uint32_t width, uint32_t height,
                             uint32_t frameIndex) const {
    std::string filename = makeFrameName(frameIndex);
    dumpPPM(colorBuffer, width, height, filename);
}

std::string FrameDumper::makeFrameName(uint32_t frameIndex) {
    char buf[16];
    snprintf(buf, sizeof(buf), "frame_%04u.ppm", frameIndex);
    return std::string(buf);
}

void FrameDumper::ensureDirectory(const std::string& path) {
    if (path.empty() || path == "." || path == "./") {
        return;
    }
    // 简单实现：尝试创建目录（不递归）
    // Phase1 仅用于确保输出目录存在
#ifdef _WIN32
    mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

}  // namespace SoftGPU
