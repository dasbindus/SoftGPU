// ============================================================================
// SoftGPU - OBJLoader.hpp
// OBJ 3D Model File Loader
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SoftGPU {

// ============================================================================
// OBJMesh - Parsed OBJ model data
// ============================================================================
struct OBJMesh {
    std::vector<float> positions;   // x,y,z triplets
    std::vector<float> uvs;         // u,v pairs (optional, may be empty)
    std::vector<float> normals;     // nx,ny,nz triplets (optional, may be empty)
    std::vector<uint32_t> indices; // Triangle vertex indices (0-based)

    // Generate per-vertex color based on face index (for flat shading)
    // Returns: vector of floats in format [x,y,z,r,g,b,a,u,v] per vertex
    // If UV not available, u=v=0. If normals not available, use face normal
    std::vector<float> toInterleavedVertices() const;

    // Simple version: returns [x,y,z,r,g,b,a] with random colors per face
    std::vector<float> toSimpleVertices() const;

    // Get triangle count
    uint32_t getTriangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }

    // Clear all data
    void clear() {
        positions.clear();
        uvs.clear();
        normals.clear();
        indices.clear();
    }
};

// ============================================================================
// OBJLoader - Wavefront OBJ file parser
// ============================================================================
class OBJLoader {
public:
    OBJLoader() = default;

    // Load OBJ from file
    // Returns true on success, false on error
    bool load(const std::string& filepath);

    // Get parsed mesh data
    const OBJMesh& getMesh() const { return m_mesh; }

    // Get error message if load failed
    const std::string& getError() const { return m_error; }

    // Check if loaded successfully
    bool isLoaded() const { return m_loaded; }

private:
    // Parse a single line from OBJ file
    void parseLine(const std::string& line);

    // Process a face definition
    void processFace(const std::string& faceStr);

    // Split string by delimiter
    static std::vector<std::string> split(const std::string& s, char delimiter);

    // Parse a face vertex string like "v/vt/vn" or "v/vt" or "v"
    // Returns tuple of (vertex_index, uv_index, normal_index), -1 if not present
    static std::tuple<int, int, int> parseFaceVertex(const std::string& str);

    OBJMesh m_mesh;
    std::string m_error;
    bool m_loaded = false;
};

} // namespace SoftGPU
