#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

// ============================================================================
// Math Basic Test
// 测试 glm 库的基本功能
// ============================================================================

namespace {

using vec3 = glm::vec3;
using mat4 = glm::mat4;
using quat = glm::quat;

class MathTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ---------------------------------------------------------------------------
// 向量测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, Vec3BasicOperations)
{
    vec3 a(1.0f, 2.0f, 3.0f);
    vec3 b(4.0f, 5.0f, 6.0f);

    // 加法
    vec3 c = a + b;
    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);

    // 点积
    float dot = glm::dot(a, b);
    EXPECT_FLOAT_EQ(dot, 32.0f); // 1*4 + 2*5 + 3*6 = 32

    // 叉积
    vec3 cross = glm::cross(a, b);
    EXPECT_FLOAT_EQ(cross.x, -3.0f);  // 2*6 - 3*5 = -3
    EXPECT_FLOAT_EQ(cross.y, 6.0f);   // 3*4 - 1*6 = 6
    EXPECT_FLOAT_EQ(cross.z, -3.0f);  // 1*5 - 2*4 = -3
}

TEST_F(MathTest, Vec3Normalization)
{
    vec3 v(3.0f, 4.0f, 0.0f);
    vec3 n = glm::normalize(v);

    EXPECT_FLOAT_EQ(glm::length(n), 1.0f);
    EXPECT_FLOAT_EQ(n.x, 0.6f);
    EXPECT_FLOAT_EQ(n.y, 0.8f);
}

// ---------------------------------------------------------------------------
// 矩阵测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, Mat4Identity)
{
    mat4 identity = mat4(1.0f);

    // 对角线为 1，其他为 0
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            EXPECT_FLOAT_EQ(identity[i][j], expected);
        }
    }
}

TEST_F(MathTest, Mat4PerspectiveProjection)
{
    float aspect = 1280.0f / 720.0f;
    float fov = glm::radians(45.0f);
    float near = 0.1f;
    float far = 100.0f;

    mat4 proj = glm::perspective(fov, aspect, near, far);

    // 验证 proj[1][1] = 1/tan(fov/2) = 1/tan(22.5°) ≈ 2.414
    float expected_cot = 1.0f / glm::tan(fov * 0.5f);
    EXPECT_FLOAT_EQ(proj[1][1], expected_cot);
}

TEST_F(MathTest, Mat4Translation)
{
    vec3 translate(1.0f, 2.0f, 3.0f);
    mat4 T = glm::translate(mat4(1.0f), translate);

    // 验证平移矩阵
    EXPECT_FLOAT_EQ(T[3][0], 1.0f);
    EXPECT_FLOAT_EQ(T[3][1], 2.0f);
    EXPECT_FLOAT_EQ(T[3][2], 3.0f);
}

TEST_F(MathTest, Mat4TransformChain)
{
    // 平移 * 旋转 * 缩放
    vec3 translation(1.0f, 0.0f, 0.0f);
    vec3 scale(2.0f, 2.0f, 2.0f);

    mat4 T = glm::translate(mat4(1.0f), translation);
    mat4 S = glm::scale(mat4(1.0f), scale);

    mat4 M = S * T; // 先平移后缩放

    // 验证：点 (1, 0, 0) 经变换后为 (4, 0, 0)
    // T * (1,0,0,1) = (2,0,0,1)
    // S * (2,0,0,1) = (4,0,0,1)
    glm::vec4 p(1.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 result = M * p;

    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

// ---------------------------------------------------------------------------
// 四元数测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, QuatBasicRotation)
{
    // 绕 Z 轴旋转 90 度
    quat q = glm::angleAxis(glm::radians(90.0f), vec3(0.0f, 0.0f, 1.0f));

    vec3 v(1.0f, 0.0f, 0.0f);
    vec3 rotated = q * v;

    // 绕 Z 轴旋转 90° 后，(1,0,0) 应变为 (0,1,0)
    EXPECT_NEAR(rotated.x, 0.0f, 0.001f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.001f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.001f);
}

TEST_F(MathTest, QuatNormalization)
{
    quat q(1.0f, 2.0f, 3.0f, 4.0f);
    quat n = glm::normalize(q);

    EXPECT_NEAR(glm::length(n), 1.0f, 0.0001f);
}

TEST_F(MathTest, QuatToMat4)
{
    // 单位四元数应转换为单位矩阵
    quat q(1.0f, 0.0f, 0.0f, 0.0f); // w=1, 其他为 0
    mat4 m = glm::mat4_cast(q);

    mat4 identity = mat4(1.0f);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_FLOAT_EQ(m[i][j], identity[i][j]);
        }
    }
}

// ---------------------------------------------------------------------------
// 数学常量测试
// ---------------------------------------------------------------------------

TEST_F(MathTest, MathConstants)
{
    // PI 值验证
    EXPECT_NEAR(glm::pi<float>(), 3.14159265358979f, 0.0001f);

    // epsilon 值合理
    EXPECT_GT(glm::epsilon<float>(), 0.0f);
    EXPECT_LT(glm::epsilon<float>(), 1e-5f);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 主函数（由 GoogleTest 提供）
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
