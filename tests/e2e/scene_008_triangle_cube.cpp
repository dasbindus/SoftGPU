// ============================================================================
// scene_008_triangle_cube.cpp - E2E Scene 008: Triangle-Cube
//
// Test: E2E-SCENE-008
// Target: Golden reference test for Triangle-Cube scene
//
// This test renders the Triangle-Cube scene (single multi-colored cube)
// and compares against a mathematically-generated golden reference.
//
// Author: Claude Code
// ============================================================================

#include "E2E_Framework.hpp"
#include "test/TestScene.hpp"
#include <cmath>

// ============================================================================
// Golden Reference Generator for 3D Scenes
// ============================================================================
namespace GoldenRef3D {

constexpr uint32_t FRAME_WIDTH = 640;
constexpr uint32_t FRAME_HEIGHT = 480;

struct Vec4 { float x, y, z, w; };
struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

// Matrix operations (column-major)
struct Mat4 {
    float m[16];
    static Mat4 identity() {
        Mat4 m = {};
        m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
        return m;
    }
    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 result = {};
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                float sum = 0;
                for (int k = 0; k < 4; k++) {
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                }
                result.m[col * 4 + row] = sum;
            }
        }
        return result;
    }
    Vec4 transform(const Vec4& v) const {
        Vec4 result = {};
        for (int row = 0; row < 4; row++) {
            result.x += m[row] * v.x;
            result.y += m[4 + row] * v.y;
            result.z += m[8 + row] * v.z;
            result.w += m[12 + row] * v.w;
        }
        return result;
    }
};

// Perspective projection matrix (column-major)
Mat4 perspective(float fovY, float aspect, float near, float far) {
    Mat4 result = {};
    float f = 1.0f / tan(fovY * 0.5f);
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (far + near) / (near - far);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * far * near) / (near - far);
    return result;
}

// Look-at view matrix (column-major)
Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float flen = sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
    f.x /= flen; f.y /= flen; f.z /= flen;

    Vec3 s = { up.y*f.z - up.z*f.y, up.z*f.x - up.x*f.z, up.x*f.y - up.y*f.x };
    float slen = sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
    s.x /= slen; s.y /= slen; s.z /= slen;

    Vec3 u = { f.y*s.z - f.z*s.y, f.z*s.x - f.x*s.z, f.x*s.y - f.y*s.x };

    Mat4 result = Mat4::identity();
    result.m[0] = s.x; result.m[4] = s.y; result.m[8] = s.z;
    result.m[1] = u.x; result.m[5] = u.y; result.m[9] = u.z;
    result.m[2] = -f.x; result.m[6] = -f.y; result.m[10] = -f.z;
    result.m[12] = -s.x*eye.x - s.y*eye.y - s.z*eye.z;
    result.m[13] = -u.x*eye.x - u.y*eye.y - u.z*eye.z;
    result.m[14] = f.x*eye.x + f.y*eye.y + f.z*eye.z;
    return result;
}

// Edge function for rasterization
inline float edgeFunction(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// Clamp value to [0, 1]
inline float saturate(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// Backface culling - returns true if triangle is front-facing
bool isFrontFacing(const Vec2& v0, const Vec2& v1, const Vec2& v2) {
    return edgeFunction(v0, v1, v2) > 0;
}

// Project and rasterize a single triangle
void rasterizeTriangle(
    std::vector<uint8_t>& pixels,
    std::vector<float>& zbuffer,
    uint32_t width, uint32_t height,
    const Vec4 v0, const Vec4 v1, const Vec4 v2,
    const Vec3& c0, const Vec3& c1, const Vec3& c2
) {
    // Perspective divide
    float iw0 = 1.0f / v0.w, iw1 = 1.0f / v1.w, iw2 = 1.0f / v2.w;

    // Viewport transform
    Vec2 sv0 = { (v0.x * iw0 + 1.0f) * 0.5f * width, (1.0f - v0.y * iw0) * 0.5f * height };
    Vec2 sv1 = { (v1.x * iw1 + 1.0f) * 0.5f * width, (1.0f - v1.y * iw1) * 0.5f * height };
    Vec2 sv2 = { (v2.x * iw2 + 1.0f) * 0.5f * width, (1.0f - v2.y * iw2) * 0.5f * height };

    float ndcZ0 = v0.z * iw0, ndcZ1 = v1.z * iw1, ndcZ2 = v2.z * iw2;

    // Backface culling
    if (!isFrontFacing(sv0, sv1, sv2)) return;

    // Bounding box
    int minX = std::max(0, (int)std::floor(std::min({sv0.x, sv1.x, sv2.x})));
    int maxX = std::min((int)width - 1, (int)std::ceil(std::max({sv0.x, sv1.x, sv2.x})));
    int minY = std::max(0, (int)std::floor(std::min({sv0.y, sv1.y, sv2.y})));
    int maxY = std::min((int)height - 1, (int)std::ceil(std::max({sv0.y, sv1.y, sv2.y})));

    float area = edgeFunction(sv0, sv1, sv2);

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            Vec2 p = { (float)x + 0.5f, (float)y + 0.5f };

            float w0 = edgeFunction(sv1, sv2, p);
            float w1 = edgeFunction(sv2, sv0, p);
            float w2 = edgeFunction(sv0, sv1, p);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float bary0 = w0 / area;
                float bary1 = w1 / area;
                float bary2 = w2 / area;

                // Z interpolation (using 1/w for perspective-correct)
                float z = bary0 * ndcZ0 + bary1 * ndcZ1 + bary2 * ndcZ2;
                size_t idx = y * width + x;

                // Depth test - smaller z is closer (OpenGL convention)
                if (z < zbuffer[idx]) {
                    zbuffer[idx] = z;

                    // Color interpolation
                    float r = bary0 * c0.x + bary1 * c1.x + bary2 * c2.x;
                    float g = bary0 * c0.y + bary1 * c1.y + bary2 * c2.y;
                    float b = bary0 * c0.z + bary1 * c1.z + bary2 * c2.z;

                    size_t pidx = idx * 3;
                    pixels[pidx + 0] = static_cast<uint8_t>(saturate(r) * 255);
                    pixels[pidx + 1] = static_cast<uint8_t>(saturate(g) * 255);
                    pixels[pidx + 2] = static_cast<uint8_t>(saturate(b) * 255);
                }
            }
        }
    }
}

// Generate golden reference for Triangle-Cube
void generateCubePPM(const char* filename, uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels(width * height * 3, 25); // Dark background
    std::vector<float> zbuffer(width * height, 1.0f);   // Far plane

    // Camera setup (matches TestScene.cpp)
    Vec3 eye = { 2.0f, 2.0f, 2.0f };
    Vec3 center = { 0.0f, 0.0f, 0.0f };
    Vec3 up = { 0.0f, 1.0f, 0.0f };
    float aspect = 640.0f / 480.0f;
    float fovY = 60.0f * 3.14159f / 180.0f;

    Mat4 proj = perspective(fovY, aspect, 0.1f, 100.0f);
    Mat4 view = lookAt(eye, center, up);
    Mat4 vp = Mat4::multiply(proj, view); // View-Projection

    // Cube vertices (h = 0.5, unit cube centered at origin)
    // Format: position (x,y,z), color (r,g,b)
    struct CubeVert { Vec3 pos; Vec3 color; };
    std::vector<CubeVert> cubeVerts = {
        // Front face (z=+0.5) - Green
        { {-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        { { 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        { { 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        { {-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        { { 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        { {-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f} },
        // Back face (z=-0.5) - Blue
        { { 0.5f,-0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        { {-0.5f,-0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        { {-0.5f, 0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f,-0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        { {-0.5f, 0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        { { 0.5f, 0.5f,-0.5f}, {0.0f, 0.0f, 1.0f} },
        // Left face (x=-0.5) - Red
        { {-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f} },
        { {-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f, 0.0f} },
        // Right face (x=+0.5) - Yellow
        { { 0.5f,-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f} },
        { { 0.5f,-0.5f,-0.5f}, {1.0f, 1.0f, 0.0f} },
        { { 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f, 0.0f} },
        { { 0.5f,-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f} },
        { { 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f, 0.0f} },
        { { 0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f} },
        // Top face (y=+0.5) - Magenta
        { {-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f} },
        { { 0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f} },
        { { 0.5f, 0.5f,-0.5f}, {1.0f, 0.0f, 1.0f} },
        { {-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f} },
        { { 0.5f, 0.5f,-0.5f}, {1.0f, 0.0f, 1.0f} },
        { {-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f, 1.0f} },
        // Bottom face (y=-0.5) - Cyan
        { {-0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 1.0f} },
        { { 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 1.0f} },
        { { 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 1.0f} },
        { {-0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 1.0f} },
        { { 0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 1.0f} },
        { {-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 1.0f} },
    };

    // Transform and rasterize all triangles
    for (size_t i = 0; i < cubeVerts.size(); i += 3) {
        const auto& a = cubeVerts[i];
        const auto& b = cubeVerts[i + 1];
        const auto& c = cubeVerts[i + 2];

        Vec4 va = vp.transform({ a.pos.x, a.pos.y, a.pos.z, 1.0f });
        Vec4 vb = vp.transform({ b.pos.x, b.pos.y, b.pos.z, 1.0f });
        Vec4 vc = vp.transform({ c.pos.x, c.pos.y, c.pos.z, 1.0f });

        rasterizeTriangle(pixels, zbuffer, width, height,
                        va, vb, vc, a.color, b.color, c.color);
    }

    FILE* f = fopen(filename, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", width, height);
        fwrite(pixels.data(), 1, pixels.size(), f);
        fclose(f);
        printf("[GoldenRef3D] Generated: %s\n", filename);
    }
}

} // namespace GoldenRef3D

// ============================================================================
// Tests
// ============================================================================
TEST_F(E2ETest, Scene008_TriangleCube_GoldenReference) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cube");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    std::string ppmPath = dumpPPM("scene008_triangle_cube.ppm");
    PPMVerifier verifier(ppmPath);
    ASSERT_TRUE(verifier.isLoaded());

    // Compare with golden reference
    const char* goldenFile = "tests/e2e/golden/scene008_triangle_cube.ppm";
    bool match = verifier.compareWithGolden(goldenFile, 0.05f);
    EXPECT_TRUE(match) << "Scene008: Triangle-Cube should match golden reference";
}

TEST_F(E2ETest, Scene008_TriangleCube_NonBlackPixelCount) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cube");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        if (color[i*4] > 0.1f || color[i*4+1] > 0.1f || color[i*4+2] > 0.1f) {
            count++;
        }
    }
    EXPECT_GT(count, 5000) << "Should have > 5000 non-black pixels (cube faces visible)";
}

TEST_F(E2ETest, Scene008_TriangleCube_HasMultipleColors) {
    TestSceneRegistry::instance().registerBuiltinScenes();
    auto scene = TestSceneRegistry::instance().getScene("Triangle-Cube");
    ASSERT_NE(scene, nullptr);

    RenderCommand cmd;
    scene->buildRenderCommand(cmd);
    m_pipeline->render(cmd);

    // Count non-black pixels with various colors (the cube has different face colors)
    const float* color = getColorBuffer();
    int colorfulCount = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i*4], g = color[i*4+1], b = color[i*4+2];
        // Check if pixel has substantial color variation (not grayscale)
        float maxC = std::max({r, g, b});
        float minC = std::min({r, g, b});
        if (maxC > 0.1f && (maxC - minC) > 0.1f) {
            colorfulCount++;
        }
    }
    EXPECT_GT(colorfulCount, 1000) << "Should have colorful pixels from different cube faces";
}