# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build
mkdir build && cd build && cmake .. && make -j4

# Run all tests
ctest --output-on-failure

# Run specific test executable
./build/tests/stages/test_Integration
./build/tests/benchmark/test_benchmark_runner

# Run main program
./build/bin/SoftGPU
./build/bin/SoftGPU --headless                    # No display required
./build/bin/SoftGPU --headless --output /tmp      # Custom output dir
```

## Architecture

SoftGPU is a software **Tile-Based Deferred Rendering (TBDR)** GPU simulator with an 8-stage pipeline.

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
- **FragmentShader**: Per-fragment shading
- **Framebuffer**: Z-buffer depth test, color write
- **TileWriteBack**: GMEM ↔ LMEM synchronization

### Memory Architecture

- **GMEM (Global Memory)**: Off-chip DRAM, 6MB total (300 tiles × 20KB)
- **LMEM (Local Memory)**: Per-tile on-chip memory, 16KB per tile
- **L2 Cache**: Simulated, 64B line, 256 sets × 8-way, 128KB total
- **Bandwidth Model**: Token bucket algorithm (default 100 GB/s)

### Key Source Directories

| Directory | Purpose |
|-----------|---------|
| `src/pipeline/` | RenderPipeline orchestrator (TBR main loop) |
| `src/stages/` | Individual pipeline stage implementations |
| `src/core/` | MemorySubsystem, RenderCommand, PipelineTypes |
| `src/profiler/` | FrameProfiler, BottleneckDetector |
| `src/renderer/` | ImGui visualization |
| `src/app/` | Application/Scene management |

### TBR Flow

1. **Geometry Phase**: CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage (bins triangles to tiles)
2. **Per-Tile Loop**: For each tile: load from GMEM → rasterize → fragment shader → framebuffer → store to GMEM
3. **Frame Output**: Sync all tiles to framebuffer, dump to PPM

## Verification Requirements (from TEAM_RULES.md)

Before every commit, you MUST verify:
1. Main program compiles: `make -j4` shows `Built target SoftGPU`
2. Tests compile: `make -j4` shows `Built target test_Integration`
3. Program runs in headless mode: `./build/bin/SoftGPU --headless`
4. All tests pass: `ctest --output-on-failure`

Do NOT report "build success" without verifying both the main program AND test programs compile.

## Test Scenes

| Scene | Triangles | Description |
|-------|-----------|-------------|
| Triangle-1Tri | 1 | Single triangle |
| Triangle-Cube | 12 | Basic cube |
| Triangle-Cubes-100 | 1200 | 100 cubes |
| Triangle-SponzaStyle | ~80 | Architecture test |
| PBR-Material | ~180 | PBR materials |
