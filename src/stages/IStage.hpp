// ============================================================================
// SoftGPU - IStage.hpp
// Stage 抽象基类
// ============================================================================

#pragma once

#include "core/PipelineTypes.hpp"

namespace SoftGPU {

// ============================================================================
// IStage - 流水线阶段基类
// ============================================================================
class IStage {
public:
    virtual ~IStage() = default;

    // 阶段名称
    virtual const char* getName() const = 0;

    // 执行阶段
    virtual void execute() = 0;

    // 获取性能计数器
    virtual const PerformanceCounters& getCounters() const = 0;

    // 重置计数器
    virtual void resetCounters() = 0;
};

}  // namespace SoftGPU
