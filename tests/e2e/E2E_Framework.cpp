// ============================================================================
// E2E_Framework.cpp - E2E Test Framework Implementation
// SoftGPU v1.0 - End-to-End Scene Tests
//
// Author: 王刚（@wanggang）— Reviewer Agent & 测试专家
// ============================================================================

#include "E2E_Framework.hpp"

namespace {
// Global test config (defined once)
E2ETestConfig g_e2e_config_impl;
}  // anonymous namespace

E2ETestConfig g_e2e_config = g_e2e_config_impl;

// ============================================================================
// PPMVerifier Implementation
// ============================================================================

PPMVerifier::PPMVerifier(const std::string& filename) {
    load(filename);
}

bool PPMVerifier::load(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.good()) {
        std::cerr << "[PPMVerifier] ERROR: Cannot open file: " << filename << "\n";
        return false;
    }

    std::string line;
    if (!std::getline(f, line)) return false;
    if (line != "P6") {
        std::cerr << "[PPMVerifier] ERROR: Not a P6 file: " << line << "\n";
        return false;
    }

    if (!std::getline(f, line)) return false;
    std::istringstream dimSS(line);
    if (!(dimSS >> m_width >> m_height)) {
        std::cerr << "[PPMVerifier] ERROR: Invalid dimensions: " << line << "\n";
        return false;
    }

    if (!std::getline(f, line)) return false;  // max value

    size_t pixelCount = static_cast<size_t>(m_width) * m_height;
    m_pixels.resize(pixelCount * 3);
    f.read(reinterpret_cast<char*>(m_pixels.data()), m_pixels.size());
    if (!f) {
        std::cerr << "[PPMVerifier] ERROR: Failed to read pixel data\n";
        return false;
    }

    m_loaded = true;
    return true;
}

Pixel PPMVerifier::getPixel(int x, int y) const {
    if (!m_loaded || x < 0 || x >= static_cast<int>(m_width) ||
        y < 0 || y >= static_cast<int>(m_height)) {
        return Pixel();
    }
    int storedY = m_height - 1 - y;
    size_t idx = (static_cast<size_t>(storedY) * m_width + x) * 3;
    return Pixel(m_pixels[idx], m_pixels[idx + 1], m_pixels[idx + 2]);
}

Pixel PPMVerifier::getPixelTL(int x, int y) const {
    if (!m_loaded || x < 0 || x >= static_cast<int>(m_width) ||
        y < 0 || y >= static_cast<int>(m_height)) {
        return Pixel();
    }
    size_t idx = (static_cast<size_t>(y) * m_width + x) * 3;
    return Pixel(m_pixels[idx], m_pixels[idx + 1], m_pixels[idx + 2]);
}

template<typename Pred>
int PPMVerifier::countPixels(Pred pred) const {
    if (!m_loaded) return 0;
    int count = 0;
    for (int y = 0; y < static_cast<int>(m_height); ++y) {
        for (int x = 0; x < static_cast<int>(m_width); ++x) {
            if (pred(getPixel(x, y))) count++;
        }
    }
    return count;
}

int PPMVerifier::countPixelsInRegion(int x1, int y1, int x2, int y2, std::function<bool(const Pixel&)> pred) const {
    if (!m_loaded) return 0;
    int count = 0;
    int minX = std::min(x1, x2), maxX = std::max(x1, x2);
    int minY = std::min(y1, y2), maxY = std::max(y1, y2);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (x < 0 || x >= static_cast<int>(m_width) ||
                y < 0 || y >= static_cast<int>(m_height))
                continue;
            if (pred(getPixel(x, y))) count++;
        }
    }
    return count;
}

PixelStats PPMVerifier::analyzeRegion(int x1, int y1, int x2, int y2) const {
    PixelStats stats;
    if (!m_loaded) return stats;

    int minX = std::min(x1, x2), maxX = std::max(x1, x2);
    int minY = std::min(y1, y2), maxY = std::max(y1, y2);
    float sumR = 0, sumG = 0, sumB = 0;
    long pixelCount = 0;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (x < 0 || x >= static_cast<int>(m_width) ||
                y < 0 || y >= static_cast<int>(m_height))
                continue;
            Pixel p = getPixel(x, y);
            float fr = p.r / 255.0f, fg = p.g / 255.0f, fb = p.b / 255.0f;
            bool nonBlack = (fr > 0.01f || fg > 0.01f || fb > 0.01f);
            if (nonBlack) {
                stats.nonBlackPixelCount++;
                sumR += fr; sumG += fg; sumB += fb;
                pixelCount++;
                if (fg > 0.5f && fg > fr && fg > fb) stats.greenPixelCount++;
                if (fr > 0.5f && fr > fg && fr > fb) stats.redPixelCount++;
                if (fb > 0.5f && fb > fr && fb > fg) stats.bluePixelCount++;
            }
        }
    }

    stats.pixelCount = static_cast<int>((maxX - minX + 1) * (maxY - minY + 1));
    if (pixelCount > 0) {
        stats.avgR = sumR / pixelCount;
        stats.avgG = sumG / pixelCount;
        stats.avgB = sumB / pixelCount;
    }
    return stats;
}

bool PPMVerifier::assertPixelRGB(int x, int y, float er, float eg, float eb, float tolerance) const {
    Pixel p = getPixel(x, y);
    float dr = std::abs(static_cast<float>(p.r) / 255.0f - er);
    float dg = std::abs(static_cast<float>(p.g) / 255.0f - eg);
    float db = std::abs(static_cast<float>(p.b) / 255.0f - eb);
    if (dr > tolerance || dg > tolerance || db > tolerance) {
        std::cerr << "[PPMVerifier] Pixel mismatch at (" << x << "," << y << "): "
                  << "expected=(" << er << "," << eg << "," << eb << "), "
                  << "got=(" << static_cast<float>(p.r)/255.0f << ","
                  << static_cast<float>(p.g)/255.0f << ","
                  << static_cast<float>(p.b)/255.0f << ")\n";
        return false;
    }
    return true;
}

bool PPMVerifier::compareWithGolden(const std::string& goldenPath, float maxError) const {
    PPMVerifier golden(goldenPath);
    if (!golden.isLoaded()) {
        std::cerr << "[PPMVerifier] ERROR: Cannot load golden file: " << goldenPath << "\n";
        return false;
    }

    int diffCount = 0;
    int totalChecked = 0;
    for (int y = 0; y < static_cast<int>(m_height); ++y) {
        for (int x = 0; x < static_cast<int>(m_width); ++x) {
            Pixel p = getPixel(x, y);
            Pixel g = golden.getPixel(x, y);
            float dr = std::abs(static_cast<float>(p.r - g.r) / 255.0f);
            float dg = std::abs(static_cast<float>(p.g - g.g) / 255.0f);
            float db = std::abs(static_cast<float>(p.b - g.b) / 255.0f);
            if (dr > maxError || dg > maxError || db > maxError) diffCount++;
            totalChecked++;
        }
    }
    float errorRate = static_cast<float>(diffCount) / totalChecked;
    if (errorRate > 0.05f) {
        std::cerr << "[PPMVerifier] Golden comparison failed. "
                  << "Error rate: " << (errorRate * 100) << "%\n";
        return false;
    }
    return true;
}

template<typename Pred>
PixelBounds PPMVerifier::findBounds(Pred pred) const {
    PixelBounds bounds;
    if (!m_loaded) return bounds;

    bool first = true;
    for (int y = 0; y < static_cast<int>(m_height); ++y) {
        for (int x = 0; x < static_cast<int>(m_width); ++x) {
            if (pred(getPixel(x, y))) {
                bounds.count++;
                if (first) {
                    bounds.minX = bounds.maxX = x;
                    bounds.minY = bounds.maxY = y;
                    first = false;
                } else {
                    bounds.minX = std::min(bounds.minX, x);
                    bounds.maxX = std::max(bounds.maxX, x);
                    bounds.minY = std::min(bounds.minY, y);
                    bounds.maxY = std::max(bounds.maxY, y);
                }
            }
        }
    }
    bounds.valid = !first;
    return bounds;
}

int PPMVerifier::countGreenPixels(float threshold) const {
    return countPixels([threshold](Pixel p) {
        float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
        return g > threshold && g > r && g > b;
    });
}

int PPMVerifier::countRedPixels(float threshold) const {
    return countPixels([threshold](Pixel p) {
        float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
        return r > threshold && r > g && r > b;
    });
}

int PPMVerifier::countBluePixels(float threshold) const {
    return countPixels([threshold](Pixel p) {
        float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
        return b > threshold && b > r && b > g;
    });
}

int PPMVerifier::countNonBlackPixels(float threshold) const {
    return countPixels([threshold](Pixel p) {
        float r = p.r / 255.0f, g = p.g / 255.0f, b = p.b / 255.0f;
        return r > threshold || g > threshold || b > threshold;
    });
}

// ============================================================================
// E2ETest Implementation
// ============================================================================

void E2ETest::SetUp() {
    m_pipeline = std::make_unique<RenderPipeline>();
    const char* env_out = std::getenv("TEST_OUTPUT_DIR");
    m_output_dir = env_out ? env_out : "";
    if (!m_output_dir.empty()) {
        m_pipeline->setDumpOutputPath(m_output_dir);
    }
}

void E2ETest::TearDown() {
    m_pipeline.reset();
}

void E2ETest::renderTriangle(const float* vertices, size_t vertexCount) {
    RenderCommand cmd;
    cmd.vertexBufferData = vertices;
    cmd.vertexBufferSize = static_cast<size_t>(vertexCount) * VERTEX_STRIDE;
    cmd.drawParams.vertexCount = static_cast<uint32_t>(vertexCount);
    cmd.drawParams.indexed = false;
    cmd.modelMatrix = identityMatrix();
    cmd.viewMatrix = identityMatrix();
    cmd.projectionMatrix = identityMatrix();
    cmd.clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    m_pipeline->render(cmd);
}

std::string E2ETest::dumpPPM(const char* filename) {
    m_pipeline->dump(filename);
    // FrameDumper writes to: m_outputPath + filename (or just filename if m_outputPath empty)
    std::string path;
    if (m_output_dir.empty()) {
        // No output dir set: FrameDumper uses CWD, prepend "./"
        path = std::string("./") + filename;
    } else {
        path = m_output_dir + "/" + filename;
    }
    m_lastPPMFilename = path;
    return path;
}

const float* E2ETest::getColorBuffer() const {
    return m_pipeline->getFramebuffer()->getColorBuffer();
}

const float* E2ETest::getDepthBuffer() const {
    return m_pipeline->getFramebuffer()->getDepthBuffer();
}

int E2ETest::countGreenPixelsFromBuffer(float threshold) const {
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i * 4 + 0];
        float g = color[i * 4 + 1];
        float b = color[i * 4 + 2];
        if (g > threshold && g > r && g > b) count++;
    }
    return count;
}

int E2ETest::countRedPixelsFromBuffer(float threshold) const {
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i * 4 + 0];
        float g = color[i * 4 + 1];
        float b = color[i * 4 + 2];
        if (r > threshold && r > g && r > b) count++;
    }
    return count;
}

int E2ETest::countBluePixelsFromBuffer(float threshold) const {
    const float* color = getColorBuffer();
    int count = 0;
    for (size_t i = 0; i < FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT; i++) {
        float r = color[i * 4 + 0];
        float g = color[i * 4 + 1];
        float b = color[i * 4 + 2];
        if (b > threshold && b > r && b > g) count++;
    }
    return count;
}

bool E2ETest::isBufferPixelGreen(int x, int y, float threshold) const {
    if (x < 0 || x >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
        y < 0 || y >= static_cast<int>(FRAMEBUFFER_HEIGHT))
        return false;
    const float* color = getColorBuffer();
    size_t idx = (static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x) * 4;
    return color[idx + 1] > threshold &&
           color[idx + 1] > color[idx + 0] &&
           color[idx + 1] > color[idx + 2];
}

bool E2ETest::isBufferPixelRed(int x, int y, float threshold) const {
    if (x < 0 || x >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
        y < 0 || y >= static_cast<int>(FRAMEBUFFER_HEIGHT))
        return false;
    const float* color = getColorBuffer();
    size_t idx = (static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x) * 4;
    return color[idx + 0] > threshold &&
           color[idx + 0] > color[idx + 1] &&
           color[idx + 0] > color[idx + 2];
}

bool E2ETest::isBufferPixelBlue(int x, int y, float threshold) const {
    if (x < 0 || x >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
        y < 0 || y >= static_cast<int>(FRAMEBUFFER_HEIGHT))
        return false;
    const float* color = getColorBuffer();
    size_t idx = (static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x) * 4;
    return color[idx + 2] > threshold &&
           color[idx + 2] > color[idx + 0] &&
           color[idx + 2] > color[idx + 1];
}

void E2ETest::getBufferPixelColor(int x, int y, float& r, float& g, float& b) const {
    r = g = b = 0.0f;
    if (x < 0 || x >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
        y < 0 || y >= static_cast<int>(FRAMEBUFFER_HEIGHT))
        return;
    const float* color = getColorBuffer();
    size_t idx = (static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x) * 4;
    r = color[idx + 0];
    g = color[idx + 1];
    b = color[idx + 2];
}

float E2ETest::getBufferPixelDepth(int x, int y) const {
    if (x < 0 || x >= static_cast<int>(FRAMEBUFFER_WIDTH) ||
        y < 0 || y >= static_cast<int>(FRAMEBUFFER_HEIGHT))
        return CLEAR_DEPTH;
    const float* depth = getDepthBuffer();
    return depth[static_cast<size_t>(y) * FRAMEBUFFER_WIDTH + x];
}

PixelBounds E2ETest::getGreenBoundsFromBuffer() const {
    PixelBounds bounds;
    bool first = true;
    int count = 0;
    for (int y = 0; y < static_cast<int>(FRAMEBUFFER_HEIGHT); ++y) {
        for (int x = 0; x < static_cast<int>(FRAMEBUFFER_WIDTH); ++x) {
            if (isBufferPixelGreen(x, y)) {
                count++;
                if (first) {
                    bounds.minX = bounds.maxX = x;
                    bounds.minY = bounds.maxY = y;
                    first = false;
                } else {
                    bounds.minX = std::min(bounds.minX, x);
                    bounds.maxX = std::max(bounds.maxX, x);
                    bounds.minY = std::min(bounds.minY, y);
                    bounds.maxY = std::max(bounds.maxY, y);
                }
            }
        }
    }
    bounds.valid = !first;
    bounds.count = count;
    return bounds;
}

PixelBounds E2ETest::getRedBoundsFromBuffer() const {
    PixelBounds bounds;
    bool first = true;
    int count = 0;
    for (int y = 0; y < static_cast<int>(FRAMEBUFFER_HEIGHT); ++y) {
        for (int x = 0; x < static_cast<int>(FRAMEBUFFER_WIDTH); ++x) {
            if (isBufferPixelRed(x, y)) {
                count++;
                if (first) {
                    bounds.minX = bounds.maxX = x;
                    bounds.minY = bounds.maxY = y;
                    first = false;
                } else {
                    bounds.minX = std::min(bounds.minX, x);
                    bounds.maxX = std::max(bounds.maxX, x);
                    bounds.minY = std::min(bounds.minY, y);
                    bounds.maxY = std::max(bounds.maxY, y);
                }
            }
        }
    }
    bounds.valid = !first;
    bounds.count = count;
    return bounds;
}

std::array<float, 16> E2ETest::identityMatrix() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

// ============================================================================
// GoldenRef Implementation
// ============================================================================

bool GoldenRef::pointInTriangle(float px, float py,
                                 float v0x, float v0y,
                                 float v1x, float v1y,
                                 float v2x, float v2y) {
    float area = 0.5f * (-v1y * v2x + v0y * (-v1x + v2x) + v0x * (v1y - v2y) + v1x * v2y);
    float s = 1.0f / (2.0f * area) * (v0y * v2x - v0x * v2y + (v2y - v0y) * px + (v0x - v2x) * py);
    float t = 1.0f / (2.0f * area) * (v0x * v1y - v0y * v1x + (v0y - v1y) * px + (v1x - v0x) * py);
    return s >= 0 && t >= 0 && (1.0f - s - t) >= 0;
}

void GoldenRef::generateFlatTrianglePPM(
    const char* filename,
    uint32_t width, uint32_t height,
    float v0x, float v0y, float v1x, float v1y, float v2x, float v2y,
    float cr, float cg, float cb,
    float bgR, float bgG, float bgB
) {
    std::vector<uint8_t> pixels(width * height * 3);
    for (size_t i = 0; i < width * height; i++) {
        pixels[i * 3 + 0] = static_cast<uint8_t>(std::min(1.0f, bgR) * 255);
        pixels[i * 3 + 1] = static_cast<uint8_t>(std::min(1.0f, bgG) * 255);
        pixels[i * 3 + 2] = static_cast<uint8_t>(std::min(1.0f, bgB) * 255);
    }

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            float ndcX = (static_cast<float>(x) / width) * 2.0f - 1.0f;
            float ndcY = (1.0f - static_cast<float>(y) / height) * 2.0f - 1.0f;
            if (pointInTriangle(ndcX, ndcY, v0x, v0y, v1x, v1y, v2x, v2y)) {
                size_t idx = (y * width + x) * 3;
                pixels[idx + 0] = static_cast<uint8_t>(std::min(1.0f, cr) * 255);
                pixels[idx + 1] = static_cast<uint8_t>(std::min(1.0f, cg) * 255);
                pixels[idx + 2] = static_cast<uint8_t>(std::min(1.0f, cb) * 255);
            }
        }
    }

    FILE* f = fopen(filename, "wb");
    if (!f) {
        std::cerr << "[GoldenRef] ERROR: Cannot write: " << filename << "\n";
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
    printf("[GoldenRef] Generated: %s\n", filename);
}
