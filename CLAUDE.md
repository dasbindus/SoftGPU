# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build
mkdir -p build && cd build && cmake .. && make -j4

# Run main program (GUI mode - requires display)
./build/bin/SoftGPU

# Run main program (headless mode - no display required)
./build/bin/SoftGPU --headless
./build/bin/SoftGPU --headless --scene Triangle-1Tri --output .
```

## Test Commands

```bash
# Run all tests via ctest
cd build && ctest --output-on-failure

# Run test executables directly
./build/bin/test_test_scenarios    # TestScene unit tests (18 tests)
./build/bin/test_Integration       # Integration tests
./build/bin/test_e2e              # E2E tests with golden references (55 tests)
./build/bin/test_PipelineVerification
./build/bin/test_benchmark_runner  # Benchmark suite
```

## Architecture

SoftGPU is a software **Tile-Based Deferred Rendering (TBDR)** GPU simulator with an 8-stage pipeline and programmable fragment shaders via ISA interpreter.

### 8-Stage Rendering Pipeline

```
CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage
    → Rasterizer → FragmentShader → Framebuffer → TileWriteBack
```

- **CommandProcessor**: Parses draw calls and vertex buffers
- **VertexShader**: MVP transformation, clipping
- **PrimitiveAssembly**: View frustum culling, triangle assembly
- **TilingStage**: Bins triangles into 300 tiles (20×15 grid)
- **Rasterizer**: Edge function DDA per tile
- **FragmentShader**: ISA interpreter with 36 instructions, 4 shader types
- **Framebuffer**: Z-buffer depth test, color write
- **TileWriteBack**: GMEM ↔ LMEM synchronization

### ISA Fragment Shader

The fragment shader uses a custom ISA (Instruction Set Architecture) with 36 instructions:
- **ShaderCore**: Execution unit pool with multiple ExecutionUnits
- **Interpreter**: Decodes and executes ISA instructions
- **WarpScheduler**: Batch processing with 8-thread warps

4 programmable shader types: Flat Color, Barycentric Color, Depth Test, Multi-Triangle

### Memory Architecture

- **GMEM (Global Memory)**: Off-chip DRAM, 6MB total (300 tiles × 20KB)
- **LMEM (Local Memory)**: Per-tile on-chip memory, 16KB per tile
- **L2 Cache**: Simulated, 64B line, 256 sets × 8-way, 128KB total
- **Bandwidth Model**: Token bucket algorithm (default 100 GB/s)

### Key Source Directories

| Directory | Purpose |
|-----------|---------|
| `src/pipeline/` | RenderPipeline orchestrator, ShaderCore, WarpScheduler |
| `src/stages/` | Individual pipeline stage implementations |
| `src/isa/` | ISA interpreter, Decoder, ExecutionUnits, RegisterFile |
| `src/core/` | MemorySubsystem, RenderCommand, PipelineTypes |
| `src/profiler/` | FrameProfiler, BottleneckDetector |
| `src/renderer/` | ImGui visualization |
| `src/test/` | TestScene, TestSceneBuilder (scene definitions) |
| `tests/` | Unit tests, E2E tests, benchmarks |

### TBR Flow

1. **Geometry Phase**: CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage (bins triangles to tiles)
2. **Per-Tile Loop**: For each tile: load from GMEM → rasterize → fragment shader (ISA) → framebuffer → store to GMEM
3. **Frame Output**: Sync all tiles to framebuffer, dump to PPM

## Headless Mode Scenes

```bash
./build/bin/SoftGPU --headless --scene Triangle-1Tri
./build/bin/SoftGPU --headless --scene Triangle-Cube
./build/bin/SoftGPU --headless --scene Triangle-Cubes-100
./build/bin/SoftGPU --headless --scene Triangle-SponzaStyle
./build/bin/SoftGPU --headless --scene PBR-Material
```

## E2E Test Scenes (Golden Reference Tests)

The E2E tests compare rendering output against golden reference PPM files in `tests/e2e/golden/`:
- `scene001_green_triangle.ppm` - Single green triangle
- `scene002_rgb_interpolation.ppm` - RGB color interpolation
- `scene003_depth_test.ppm` - Depth testing with occlusion
- `scene004_mad_verification.ppm` - MAD operation verification
- `scene005_multi_triangle.ppm` - Multiple triangles
- `scene006_warp_scheduling.ppm` - Warp scheduling demonstration

## Verification Requirements

Before every commit, verify:
1. Main program compiles: `make -j4` shows `Built target SoftGPU`
2. Program runs headless: `./build/bin/SoftGPU --headless --scene Triangle-1Tri`
3. All tests pass: `ctest --output-on-failure`

## Vertex Format

TestScene vertices use format: `x, y, z, w, r, g, b, a` (8 floats per vertex)
- **w**: Homogeneous coordinate (typically 1.0)
- **r, g, b, a**: Color values (typically 0.0-1.0)

Note: VertexShader performs MVP transformation, so w component is critical for proper clipping.
