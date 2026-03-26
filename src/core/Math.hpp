// ============================================================================
// SoftGPU - Math.hpp
// 数学类型别名（基于 glm）
// ============================================================================

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

// ============================================================================
// 基础类型别名
// ============================================================================
namespace SoftGPU {

// 向量类型
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;

// 矩阵类型
using mat2  = glm::mat2;
using mat3  = glm::mat3;
using mat4  = glm::mat4;
using mat2x2 = glm::mat2x2;
using mat2x3 = glm::mat2x3;
using mat2x4 = glm::mat2x4;
using mat3x2 = glm::mat3x2;
using mat3x3 = glm::mat3x3;
using mat3x4 = glm::mat3x4;
using mat4x2 = glm::mat4x2;
using mat4x3 = glm::mat4x3;
using mat4x4 = glm::mat4x4;

// 四元数
using quat = glm::quat;

// 变换
using transform = glm::mat4;

// ============================================================================
// 数学常量
// ============================================================================
constexpr float PI          = glm::pi<float>();
constexpr float TWO_PI     = glm::two_pi<float>();
constexpr float HALF_PI    = glm::half_pi<float>();
constexpr float INV_PI     = glm::one_over_pi<float>();
constexpr float DEG_TO_RAD = glm::pi<float>() / 180.0f;
constexpr float RAD_TO_DEG  = 180.0f / glm::pi<float>();
constexpr float EPSILON    = glm::epsilon<float>();
constexpr float INFINITY_F = glm::infty<float>();

// ============================================================================
// 辅助函数
// ============================================================================

// 角度转弧度
constexpr float radians(float degrees) {
    return degrees * DEG_TO_RAD;
}

// 弧度转角度
constexpr float degrees(float radians) {
    return radians * RAD_TO_DEG;
}

// 夹紧
template<typename T>
constexpr T clamp(T value, T minVal, T maxVal) {
    return glm::clamp(value, minVal, maxVal);
}

// 线性插值
template<typename T>
constexpr T lerp(const T& a, const T& b, float t) {
    return glm::mix(a, b, t);
}

// 平滑步进
template<typename T>
constexpr T smoothstep(const T& a, const T& b, float t) {
    return glm::smoothstep(a, b, t);
}

// 长度
template<typename T>
constexpr float length(const T& v) {
    return glm::length(v);
}

// 归一化
template<typename T>
constexpr T normalize(const T& v) {
    return glm::normalize(v);
}

// 点积
template<typename T>
constexpr float dot(const T& a, const T& b) {
    return glm::dot(a, b);
}

// 叉积（仅 vec3）
template<>
constexpr vec3 cross(const vec3& a, const vec3& b) {
    return glm::cross(a, b);
}

// 距离
template<typename T>
constexpr float distance(const T& a, const T& b) {
    return glm::distance(a, b);
}

// 反射
template<typename T>
constexpr T reflect(const T& I, const T& N) {
    return glm::reflect(I, N);
}

// 折射
template<typename T>
constexpr T refract(const T& I, const T& N, float eta) {
    return glm::refract(I, N, eta);
}

// ============================================================================
// 矩阵构建辅助函数
// ============================================================================

// 构建平移矩阵
constexpr mat4 translate(const vec3& t) {
    return glm::translate(mat4(1.0f), t);
}

// 构建缩放矩阵
constexpr mat4 scale(const vec3& s) {
    return glm::scale(mat4(1.0f), s);
}

// 构建旋转矩阵（弧度）
constexpr mat4 rotate(float radians, const vec3& axis) {
    return glm::rotate(mat4(1.0f), radians, axis);
}

// 构建透视投影矩阵
constexpr mat4 perspective(float fovY, float aspect, float near, float far) {
    return glm::perspective(fovY, aspect, near, far);
}

// 构建正交投影矩阵
constexpr mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
    return glm::ortho(left, right, bottom, top, near, far);
}

// LookAt 视图矩阵
constexpr mat4 lookAt(const vec3& eye, const vec3& center, const vec3& up) {
    return glm::lookAt(eye, center, up);
}

// 单位矩阵
constexpr mat4 identity() {
    return mat4(1.0f);
}

// 逆矩阵
constexpr mat4 inverse(const mat4& m) {
    return glm::inverse(m);
}

// 转置矩阵
constexpr mat4 transpose(const mat4& m) {
    return glm::transpose(m);
}

// ========================================================================
// 四元数辅助函数
// ========================================================================

// 从轴角创建四元数
constexpr quat angleAxis(float angle, const vec3& axis) {
    return glm::angleAxis(angle, axis);
}

// 四元数转矩阵
constexpr mat4 mat4_cast(const quat& q) {
    return glm::mat4_cast(q);
}

// 矩阵转四元数
constexpr quat quat_cast(const mat4& m) {
    return glm::quat_cast(m);
}

// 球面线性插值
constexpr quat slerp(const quat& a, const quat& b, float t) {
    return glm::slerp(a, b, t);
}

// 四元数乘法
constexpr quat mul(const quat& a, const quat& b) {
    return a * b;
}

// 旋转向量
constexpr vec3 rotate(const quat& q, const vec3& v) {
    return q * v;
}

// 共轭
constexpr quat conjugate(const quat& q) {
    return glm::conjugate(q);
}

// 模长
constexpr float length(const quat& q) {
    return glm::length(q);
}

// 归一化
constexpr quat normalize(const quat& q) {
    return glm::normalize(q);
}

} // namespace SoftGPU
