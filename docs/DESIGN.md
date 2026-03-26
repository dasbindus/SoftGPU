# SoftGPU Architecture and Design

A software Tile-Based GPU simulator implementing a real GPU rendering pipeline in software.

---

## Overview

SoftGPU simulates a modern Tile-Based Deferred Rendering (TBDR) GPU architecture. It provides real-time performance profiling, bottleneck detection, and benchmark automation.

**Key Design Goals:**
- Faithful implementation of GPU rendering pipeline
- Accurate memory bandwidth modeling
- Real-time performance analysis
- Extensible architecture

---

## System Architecture

### 8-Stage Rendering Pipeline

```
┌─────────────────┐
│ CommandProcessor │  Parse draw calls, vertex buffers
└────────┬────────┘
         ▼
┌─────────────────┐
│  VertexShader   │  MVP transformation, clipping
└────────┬────────┘
         ▼
┌─────────────────┐
│PrimitiveAssembly │  View frustum culling, triangle assembly
└────────┬────────┘
         ▼
┌─────────────────┐
│   TilingStage   │  Triangle binning to 300 tiles (20×15)
└────────┬────────┘
         ▼
┌─────────────────┐
│   Rasterizer    │  Edge function DDA, per-tile rasterization
└────────┬────────┘
         ▼
┌─────────────────┐
│ FragmentShader  │  Per-fragment processing, shading
└────────┬────────┘
         ▼
┌─────────────────┐
│   Framebuffer   │  Z-buffer depth test, color write
└────────┬────────┘
         ▼
┌─────────────────┐
│  TileWriteBack  │  Tile data write-back to GMEM
└─────────────────┘
```

---

## Memory Architecture

### GMEM (Global Memory)

Off-chip DRAM storing rendered tile data.

- **Capacity:** 300 tiles × 20KB = 6MB
- **Bandwidth Model:** Token bucket algorithm
- **Access Pattern:** Sequential tile write-back

### LMEM (Local Memory)

On-chip tile memory (per tile).

- **Size:** 32×32 pixels × 4 channels × 4 bytes = 16KB per tile
- **Purpose:** Fast read/write during tile rendering

### L2 Cache

Simulated L2 cache between GMEM and shader cores.

- **Line Size:** 64 bytes
- **Associativity:** 4-way set associative
- **Hit Rate:** Tracked per access

---

## Tile-Based Rendering Flow

### Phase 1: Geometry Processing

```
1. CommandProcessor receives draw call
2. VertexShader transforms vertices (MVP matrices)
3. PrimitiveAssembly performs frustum culling
4. TilingStage bins triangles into tile lists (300 tiles)
```

### Phase 2: Per-Tile Rendering

```
For each tile (in tile order):
    1. Load tile from GMEM → LMEM
    2. For each triangle in tile bin:
        a. Rasterize → generate fragments
        b. FragmentShader shade fragments
        c. Framebuffer depth test + color write
    3. Store tile from LMEM → GMEM
```

### Phase 3: Frame Output

```
1. Copy all tiles from GMEM to framebuffer
2. Output to display (PPM dump for testing)
```

---

## Performance Profiling

### Metrics Collected

| Metric | Description |
|--------|-------------|
| Stage Time | Nanosecond-precision per stage |
| Bandwidth | GMEM read/write bytes |
| L2 Hit Rate | Cache efficiency |
| Fragment Count | Total fragments processed |

### Bottleneck Detection

Three-dimensional analysis:

```
Shader Bound:    FS time > 70% && Core utilization < 50%
Memory Bound:    Bandwidth > 85% && Core utilization < 70%
Fill Rate:      Rasterizer output < 30% of peak
```

---

## Data Structures

### Vertex

```cpp
struct Vertex {
    glm::vec4 position;  // Clip-space position
    glm::vec4 color;     // RGBA color
};
```

### Triangle

```cpp
struct Triangle {
    Vertex vertices[3];
    uint32_t indices[3];
};
```

### Fragment

```cpp
struct Fragment {
    glm::ivec2 position;  // Screen coordinates
    float depth;          // Z value for depth test
    glm::vec4 color;      // Final color
};
```

### TileBin

```cpp
struct TileBin {
    uint32_t tileIndex;
    std::vector<uint32_t> triangleIndices;
};
```

---

## Key Algorithms

### Tiling (Binning)

Each triangle is rasterized to determine which tiles it covers:

```cpp
for (each triangle):
    compute bounding box (minX, maxX, minY, maxY)
    for each tile in bounding box:
        if triangle covers tile center:
            add triangle to tile's bin
```

### Edge Function Rasterization

```
For each tile:
    for each triangle in tile's bin:
        compute edge functions at tile corners
        interpolate depth and color
        write fragment if inside triangle
```

### Z-Buffer Depth Test

```cpp
depthTestAndWrite(x, y, z, color):
    if z < zBuffer[y][x]:
        zBuffer[y][x] = z
        colorBuffer[y][x] = color
        return true
    return false
```

---

## Memory Bandwidth Model

### Token Bucket Algorithm

```
Tokens: Represents available bandwidth
Rate: Refill rate per time unit
Capacity: Maximum tokens

tryConsume(bytes):
    if tokens >= bytes:
        tokens -= bytes
        return true
    return false
```

### Bandwidth Calculation

```
read_bytes = tiles × tile_size × load_passes
write_bytes = tiles × tile_size × store_passes
total_bandwidth = read_bytes + write_bytes
```

---

## Testing Strategy

### Unit Tests

- Framebuffer (clear, write, depth test)
- Rasterizer (edge function, interpolation)
- PrimitiveAssembly (culling)
- VertexShader (matrix transformation)

### Integration Tests

- GreenTriangle (single triangle rendering)
- ZBuffer (depth test ordering)
- ColorInterpolation (Gouraud shading)

### Benchmark Tests

- FPS measurement
- Stage timing accuracy
- Bandwidth calculation verification

---

## Build System

### Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| CMake | ≥3.16 | Build system |
| GLFW | 3.3+ | Window management |
| GLM | 0.9.9+ | Math library |
| GoogleTest | 1.12+ | Unit testing |
| ImGui | latest | Debug UI |

### Build Commands

```bash
mkdir build && cd build
cmake ..
make -j4
ctest --output-on-failure
```

---

## Future Extensions

- SIMD rasterization (SSE/AVX)
- Multi-threaded tile processing
- Vulkan backend
- More scene formats (OBJ, glTF)
