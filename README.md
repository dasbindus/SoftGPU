# SoftGPU

A software Tile-Based GPU simulator with performance analysis and visualization.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-orange.svg)
![Build](https://img.shields.io/badge/build-passing-green.svg)

---

## Features

- **Tile-Based Rendering (TBR)** - True TBR architecture with binning
- **8-Stage Pipeline** - CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage → Rasterizer → FragmentShader → Framebuffer → TileWriteBack
- **ISA Interpreter** - 36 instructions with programmable fragment shader execution
- **4 ISA Shader Types** - Flat Color, Barycentric Color, Depth Test, Multi-Triangle
- **Memory Subsystem** - Token bucket bandwidth model with L2 cache simulation (256KB)
- **Warp Scheduler** - Batch processing with 8-thread warps
- **Performance Profiler** - Real-time stage timing and bottleneck detection
- **Benchmark Suite** - 55 E2E tests with Golden Reference comparison
- **ImGui Visualization** - Architecture diagram with utilization coloring

---

## Architecture

```
RenderPipeline (8 Stage)
├── CommandProcessor    # DrawCall parsing
├── VertexShader        # MVP transformation
├── PrimitiveAssembly   # View frustum culling
├── TilingStage        # Triangle binning (300 tiles)
├── Rasterizer         # Edge function DDA
├── FragmentShader      # ISA interpreter with 36 instructions
├── Framebuffer        # Z-buffer depth test
└── TileWriteBack       # GMEM write-back

Support Modules:
├── ShaderCore          # ISA execution unit
├── Interpreter         # 36-instruction ISA interpreter
├── MemorySubsystem     # Bandwidth model + 256KB L2 Cache
├── FrameProfiler       # Performance data collection
├── BottleneckDetector  # 瓶颈判定
└── ProfilerUI         # ImGui visualization
```

---

## Building

```bash
# Clone
git clone https://github.com/dasbindus/SoftGPU.git
cd SoftGPU

# Build
mkdir build && cd build
cmake ..
make -j4

# Run tests
ctest --output-on-failure
```

---

## Testing

```bash
# Run all tests
./bin/test_e2e
./bin/test_Integration
./bin/test_PipelineVerification

# Benchmark
./bin/SoftGPU --headless --scene Triangle-Cube
```

---

## Test Scenes

| Scene | Triangles | Description |
|-------|-----------|-------------|
| Scene001 GreenTriangle | 1 | Single green triangle |
| Scene002 RGBInterpolation | 1 | RGB color interpolation |
| Scene003 DepthTest | 2 | Depth testing with occlusion |
| Scene004 MAD | 1 | MAD operation verification |
| Scene005 MultiTriangle | 3 | Multiple triangles with depth |
| Scene006 WarpScheduling | 1 | Warp scheduling demonstration |

---

## ISA Shader Types (v1.3)

The fragment shader now supports 4 programmable shader types via ISA execution:

1. **Flat Color** - Simple passthrough with clamping
2. **Barycentric Color** - Vertex color interpolation
3. **Depth Test** - Per-fragment depth testing
4. **Multi-Triangle** - Complex multi-primitive rendering

---

## Performance Analysis

The profiler provides:

- **Stage Timing** - Nanosecond precision per stage
- **Bottleneck Detection** - Shader/Memory/FillRate bound analysis
- **Bandwidth Utilization** - GMEM read/write statistics
- **L2 Cache Hit Rate** - Cache efficiency metrics (256KB, tile-aware)

---

## Running

### GUI Mode (requires display)

```bash
./build/bin/SoftGPU
```

### Headless Mode (no display required)

```bash
# Output to current directory
./build/bin/SoftGPU --headless

# Output to specific directory
./build/bin/SoftGPU --headless --output /tmp

# Custom filename
./build/bin/SoftGPU --headless --output-filename my_render.ppm

# Select scene
./build/bin/SoftGPU --headless --scene Triangle-Cube
```

---

## Roadmap (v1.3+)

**Current version: v1.3** - Fragment Shader ISA execution with 4 programmable shaders

| Version | Target | Status |
|---------|--------|--------|
| v1.3 | Fragment Shader ISA execution | ✅ Complete |
| v1.4 | Performance optimization, texture sampling | Planned |
| v2.0 | Multi-core parallelization | Planned |

---

## Releases

- **v1.3** - Fragment Shader ISA execution (2026-03-30)
- **v1.2** - Warp scheduler batch processing, ISA 36 instructions (2026-03-29)
- **v1.1** - GMEM wiring, DIV Newton-Raphson, TokenBucket, 55 E2E tests (2026-03-29)
- **v1.0** - ISA 28 instructions, 5-stage pipeline (2026-03-27)
- **v0.5** - Refactoring complete (2026-03-26)
- **v0.2** - Initial release (2026-03-26)

---

## License

MIT License
