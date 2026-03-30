// ============================================================================
// SoftGPU - Common.hpp
// 通用类型定义、宏、工具函数
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

// ============================================================================
// 基本类型别名
// ============================================================================
using int8  = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;



// ============================================================================
// 浮点类型别名
// ============================================================================
using float32 = float;
using float64 = double;

// ============================================================================
// 编译期断言
// ============================================================================
static_assert(sizeof(int32) == 4, "int32 must be 4 bytes");
static_assert(sizeof(float32) == 4, "float32 must be 4 bytes");
static_assert(sizeof(float64) == 8, "float64 must be 8 bytes");

// ============================================================================
// 宏定义
// ============================================================================

// 禁止拷贝和移动宏
#define SOFTGPU_NONCOPYABLE(TypeName) \
    TypeName(const TypeName&) = delete; \
    TypeName& operator=(const TypeName&) = delete;

#define SOFTGPU_NONMOVEABLE(TypeName) \
    TypeName(TypeName&&) = delete; \
    TypeName& operator=(TypeName&&) = delete;

// 禁用拷贝和移动
#define SOFTGPU_NON_COPYABLE_AND_MOVABLE(TypeName) \
    SOFTGPU_NONCOPYABLE(TypeName) \
    SOFTGPU_NONMOVEABLE(TypeName)

// 清理资源 RAII 包装器
#define SOFTGPU_DELETE_RESOURCE(ResourceType, DeleterFunc) \
    template<typename T> \
    struct ResourceDeleter { \
        void operator()(T* ptr) const { \
            if (ptr) DeleterFunc(ptr); \
        } \
    }

// 便捷日志宏
#define SOFTGPU_LOG_INFO(msg)    fprintf(stdout, "[INFO] %s\n", msg)
#define SOFTGPU_LOG_WARNING(msg) fprintf(stdout, "[WARN] %s\n", msg)
#define SOFTGPU_LOG_ERROR(msg)   fprintf(stderr, "[ERROR] %s\n", msg)

// ============================================================================
// 常量
// ============================================================================
namespace SoftGPU {

constexpr float32 PI        = 3.14159265358979f;
constexpr float32 TWO_PI    = 6.28318530717958f;
constexpr float32 HALF_PI   = 1.57079632679489f;
constexpr float32 INV_PI    = 0.31830988618379f;

constexpr float32 EPSILON   = 1.19209289550781e-7f;
// Undefine INFINITY macro if defined (conflicts with cmath)
#ifdef INFINITY
#undef INFINITY
#endif
constexpr float32 INFINITY_F = 3.402823466e+38f;

} // namespace SoftGPU
