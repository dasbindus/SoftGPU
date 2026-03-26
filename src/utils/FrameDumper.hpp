// ============================================================================
// SoftGPU - FrameDumper.hpp
// 帧导出工具：将渲染结果 dump 为 PPM 文件
// ============================================================================

#pragma once

#include <string>
#include <cstdint>

namespace SoftGPU {

// ============================================================================
// FrameDumper - 帧导出工具
// 职责：将 RGBA float buffer 导出为图片文件（PPM 格式优先实现）
// ============================================================================
class FrameDumper {
public:
    FrameDumper() = default;

    // 设置输出目录（默认为当前目录）
    void setOutputPath(const std::string& path);

    // 获取当前输出目录
    const std::string& getOutputPath() const { return m_outputPath; }

    // dump RGBA float buffer 为 PPM 格式文件
    // filename: 例如 "frame_0000.ppm"
    // 如果 filename 不含路径，则输出到 m_outputPath
    void dumpPPM(const float* colorBuffer,
                 uint32_t width, uint32_t height,
                 const std::string& filename) const;

    // 便捷方法：生成带序号的帧文件名并 dump
    // frameIndex: 帧序号（例如 0 -> "frame_0000.ppm"）
    void dumpFrame(const float* colorBuffer,
                   uint32_t width, uint32_t height,
                   uint32_t frameIndex) const;

private:
    std::string m_outputPath = ".";  // 默认当前目录

    // 内部：生成 4 位序号文件名
    static std::string makeFrameName(uint32_t frameIndex);

    // 内部：确保目录存在（仅创建最后一级目录）
    static void ensureDirectory(const std::string& path);
};

}  // namespace SoftGPU
