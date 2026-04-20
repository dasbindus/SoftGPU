// ============================================================================
// SoftGPU - OBJLoader.cpp
// OBJ 3D Model File Loader Implementation
// ============================================================================

#include "OBJLoader.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>
#include <array>

namespace SoftGPU {

bool OBJLoader::load(const std::string& filepath) {
    m_mesh.clear();
    m_error.clear();
    m_loaded = false;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        m_error = "Failed to open file: " + filepath;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Trim leading/trailing whitespace
        while (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#') {
            continue;  // Skip empty lines and comments
        }

        parseLine(line);
    }

    file.close();
    m_loaded = true;
    return true;
}

void OBJLoader::parseLine(const std::string& line) {
    if (line.length() < 2) return;

    std::istringstream iss(line);
    std::string keyword;
    iss >> keyword;

    if (keyword == "v") {
        // Vertex position
        float x, y, z;
        if (iss >> x >> y >> z) {
            m_mesh.positions.push_back(x);
            m_mesh.positions.push_back(y);
            m_mesh.positions.push_back(z);
        }
    }
    else if (keyword == "vt") {
        // Texture coordinate
        float u, v;
        if (iss >> u >> v) {
            m_mesh.uvs.push_back(u);
            m_mesh.uvs.push_back(v);
        }
    }
    else if (keyword == "vn") {
        // Vertex normal
        float nx, ny, nz;
        if (iss >> nx >> ny >> nz) {
            m_mesh.normals.push_back(nx);
            m_mesh.normals.push_back(ny);
            m_mesh.normals.push_back(nz);
        }
    }
    else if (keyword == "f") {
        // Face
        std::string faceStr = line.substr(1);  // Skip "f"
        while (faceStr[0] == ' ') faceStr.erase(faceStr.begin());
        processFace(faceStr);
    }
    // Skip other keywords (mtllib, usemtl, etc.)
}

void OBJLoader::processFace(const std::string& faceStr) {
    // Split by spaces to get face vertices
    auto parts = split(faceStr, ' ');

    // We expect at least 3 vertices for a triangle
    if (parts.size() < 3) return;

    std::vector<std::tuple<int, int, int>> faceVertices;
    for (const auto& part : parts) {
        if (part.empty()) continue;
        faceVertices.push_back(parseFaceVertex(part));
    }

    if (faceVertices.size() < 3) return;

    // Triangulate: fan triangulation from first vertex
    // For quad: v0, v1, v2, v3 -> (v0,v1,v2), (v0,v2,v3)
    for (size_t i = 1; i < faceVertices.size() - 1; i++) {
        auto [v0, vt0, vn0] = faceVertices[0];
        auto [v1, vt1, vn1] = faceVertices[i];
        auto [v2, vt2, vn2] = faceVertices[i + 1];

        // OBJ indices are 1-based, convert to 0-based
        // Negative indices are relative to end, handle them
        int v0_idx = (v0 < 0) ? static_cast<int>(m_mesh.positions.size() / 3) + v0 : v0 - 1;
        int v1_idx = (v1 < 0) ? static_cast<int>(m_mesh.positions.size() / 3) + v1 : v1 - 1;
        int v2_idx = (v2 < 0) ? static_cast<int>(m_mesh.positions.size() / 3) + v2 : v2 - 1;

        // Validate indices
        if (v0_idx < 0 || v1_idx < 0 || v2_idx < 0) continue;
        size_t max_idx = m_mesh.positions.size() / 3;
        if (static_cast<size_t>(v0_idx) >= max_idx ||
            static_cast<size_t>(v1_idx) >= max_idx ||
            static_cast<size_t>(v2_idx) >= max_idx) continue;

        m_mesh.indices.push_back(static_cast<uint32_t>(v0_idx));
        m_mesh.indices.push_back(static_cast<uint32_t>(v1_idx));
        m_mesh.indices.push_back(static_cast<uint32_t>(v2_idx));
    }
}

std::tuple<int, int, int> OBJLoader::parseFaceVertex(const std::string& str) {
    // Face vertex format: "v" or "v/vt" or "v/vt/vn"
    // All indices are 1-based in OBJ format

    auto parts = split(str, '/');

    int v = 0, vt = -1, vn = -1;

    if (!parts.empty() && !parts[0].empty()) {
        v = std::stoi(parts[0]);
    }
    if (parts.size() > 1 && !parts[1].empty()) {
        vt = std::stoi(parts[1]);
    }
    if (parts.size() > 2 && !parts[2].empty()) {
        vn = std::stoi(parts[2]);
    }

    return {v, vt, vn};
}

std::vector<std::string> OBJLoader::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream iss(s);
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<float> OBJMesh::toSimpleVertices() const {
    std::vector<float> vertices;

    // Generate per-face colors (simple flat shading)
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> colorDist(0.5f, 1.0f);

    size_t numTriangles = indices.size() / 3;
    vertices.reserve(numTriangles * 3 * 8);  // 3 verts * 8 floats each

    for (size_t t = 0; t < numTriangles; t++) {
        // Generate color for this face
        float r = colorDist(rng);
        float g = colorDist(rng);
        float b = colorDist(rng);

        // Each triangle has 3 vertices
        for (int i = 0; i < 3; i++) {
            uint32_t idx = indices[t * 3 + i];
            size_t pos = idx * 3;

            if (pos + 2 < positions.size()) {
                vertices.push_back(positions[pos]);     // x
                vertices.push_back(positions[pos + 1]); // y
                vertices.push_back(positions[pos + 2]); // z
                vertices.push_back(1.0f);              // w
                vertices.push_back(r);                  // r
                vertices.push_back(g);                  // g
                vertices.push_back(b);                  // b
                vertices.push_back(1.0f);              // a
            }
        }
    }

    return vertices;
}

std::vector<float> OBJMesh::toInterleavedVertices() const {
    std::vector<float> vertices;

    // This version would include UV and normals if available
    // For now, just call simple version

    // Check if we have UV coordinates
    bool hasUV = !uvs.empty();
    bool hasNormals = !normals.empty();

    size_t numTriangles = indices.size() / 3;
    size_t floatsPerVertex = hasUV ? 10 : 8;  // 8 basic + 2 UV if available

    vertices.reserve(numTriangles * 3 * floatsPerVertex);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> colorDist(0.5f, 1.0f);

    for (size_t t = 0; t < numTriangles; t++) {
        float faceColorR = colorDist(rng);
        float faceColorG = colorDist(rng);
        float faceColorB = colorDist(rng);

        for (int i = 0; i < 3; i++) {
            uint32_t idx = indices[t * 3 + i];
            size_t pos = idx * 3;

            if (pos + 2 < positions.size()) {
                vertices.push_back(positions[pos]);     // x
                vertices.push_back(positions[pos + 1]); // y
                vertices.push_back(positions[pos + 2]); // z
                vertices.push_back(1.0f);               // w
                vertices.push_back(faceColorR);          // r
                vertices.push_back(faceColorG);          // g
                vertices.push_back(faceColorB);          // b
                vertices.push_back(1.0f);               // a

                if (hasUV) {
                    size_t uvIdx = idx * 2;
                    if (uvIdx + 1 < uvs.size()) {
                        vertices.push_back(uvs[uvIdx]);     // u
                        vertices.push_back(uvs[uvIdx + 1]); // v
                    } else {
                        vertices.push_back(0.0f);
                        vertices.push_back(0.0f);
                    }
                }
            }
        }
    }

    return vertices;
}

} // namespace SoftGPU
