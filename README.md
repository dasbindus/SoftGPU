# SoftGPU

A software Tile-Based GPU simulator with performance analysis and visualization.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-orange.svg)
![Build](https://img.shields.io/badge/build-passing-green.svg)

---

## Features

- **Tile-Based Rendering (TBR)** - True TBR architecture with binning
- **8-Stage Pipeline** - CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage → Rasterizer → FragmentShader → Framebuffer → TileWriteBack
- **Memory Subsystem** - Token bucket bandwidth model with L2 cache simulation
- **Performance Profiler** - Real-time stage timing and bottleneck detection
- **Benchmark Suite** - Automated testing with 5 scene types
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
├── FragmentShader     # Per-fragment processing
├── Framebuffer        # Z-buffer depth test
└── TileWriteBack      # GMEM write-back

Support Modules:
├── MemorySubsystem     # Bandwidth model + L2 Cache
├── FrameProfiler       # Performance data collection
├── BottleneckDetector  # 瓶颈判定
└── ProfilerUI          # ImGui visualization
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
./build/tests/stages/test_Integration
./build/tests/benchmark/test_benchmark_runner

# Benchmark
./build/bin/SoftGPU --headless --scene Triangle-Cube
```

---

## Test Scenes

| Scene | Triangles | Description |
|-------|-----------|-------------|
| Triangle-1Tri | 1 | Single green triangle |
| Triangle-Cube | 12 | Basic cube with multiple colors |
| Triangle-Cubes-100 | 1200 | 100 cubes grid |
| Triangle-SponzaStyle | ~80 | Sponza-style architecture |
| PBR-Material | ~180 | PBR material test |

---

## Performance Analysis

The profiler provides:

- **Stage Timing** - Nanosecond precision per stage
- **Bottleneck Detection** - Shader/Memory/FillRate bound analysis
- **Bandwidth Utilization** - GMEM read/write statistics
- **L2 Cache Hit Rate** - Cache efficiency metrics

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

## v1.0 Roadmap

**Next milestone: v1.0 (Microarchitecture)**

- ISA (Instruction Set Architecture) design
- Shader Core microarchitecture
- Warp scheduler simulation

---

## Releases

- **v0.2** - Refactoring complete (2026-03-26)
- **v0.1** - Initial release (2026-03-26)

---

## License

MIT License
