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
├── BottleneckDetector  #瓶颈判定
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
./build/bin/SoftGPU --benchmark --scenes Triangle-Cube --runs 10
```

---

## Test Scenes

| Scene | Triangles | Description |
|-------|-----------|-------------|
| Triangle-1Tri | 1 | Single triangle |
| Triangle-Cube | 12 | Basic cube |
| Triangle-Cubes-100 | 1200 | 100 cubes |
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

## Releases

- **v0.2** - Refactoring complete (5 P0 issues fixed)
- **v0.1** - Initial release

---

## Documentation

- [Release Notes](docs/releases/)
- [Phase Acceptance Reports](docs/phase-acceptance/)
- [Code Smell Report](docs/CODE_SMELL_REPORT.md)
- [Project Retrospective](docs/PROJECT_RETROSPECTIVE.md)

---

## License

MIT License
