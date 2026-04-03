# SoftGPU

一款软件实现的 Tile-Based GPU 模拟器，支持性能分析与可视化。

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-orange.svg)
![Build](https://github.com/dasbindus/SoftGPU/actions/workflows/build.yml/badge.svg)

---

## 特性

- **Tile-Based Rendering (TBR)** - 真正的 TBR 架构，支持 binning
- **8 级渲染管线** - CommandProcessor → VertexShader → PrimitiveAssembly → TilingStage → Rasterizer → FragmentShader → Framebuffer → TileWriteBack
- **ISA 解释器** - 36 条指令，支持可编程片元着色器
- **4 种 ISA 着色器类型** - Flat Color、Barycentric Color、Depth Test、Multi-Triangle
- **内存子系统** - Token bucket 带宽模型 + L2 缓存模拟（256KB）
- **Warp 调度器** - 批处理，8 线程 warps
- **性能分析器** - 实时各级阶段耗时与瓶颈检测
- **55 个 E2E 测试** - Golden Reference 对比测试
- **ImGui 可视化** - 架构图与利用率着色

---

## 架构

```
RenderPipeline (8 级管线)
├── CommandProcessor    # DrawCall 解析
├── VertexShader        # MVP 变换
├── PrimitiveAssembly   # 视锥体裁剪
├── TilingStage        # 三角形 binning（300 tiles）
├── Rasterizer         # 边缘函数 DDA
├── FragmentShader      # ISA 解释器，36 条指令
├── Framebuffer        # Z-buffer 深度测试
└── TileWriteBack       # GMEM 回写

支持模块:
├── ShaderCore          # ISA 执行单元
├── Interpreter         # 36 指令 ISA 解释器
├── MemorySubsystem     # 带宽模型 + 256KB L2 缓存
├── FrameProfiler       # 性能数据采集
├── BottleneckDetector  # 瓶颈检测
└── ProfilerUI         # ImGui 可视化
```

---

## 构建

### 依赖

**Ubuntu:**
```bash
sudo apt-get install libgl1-mesa-dev libxkbcommon-dev libglfw3-dev libgtest-dev libgmock-dev
```

**macOS:**
```bash
brew install glfw3 googletest
```

**通用依赖:**
- OpenGL
- CMake 3.16+
- C++17 编译器

```bash
# 克隆
git clone https://github.com/dasbindus/SoftGPU.git
cd SoftGPU

# 构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

---

## 测试

```bash
# 通过 ctest 运行所有测试
ctest --output-on-failure

# 运行单个测试
./bin/test_e2e --gtest_filter=*TriangleCubes100*

# 直接运行各测试可执行文件
./bin/test_e2e              # 77 个 E2E 测试，含 golden reference
./bin/test_Integration       # 6 个集成测试
./bin/test_test_scenarios   # 18 个 TestScene 单元测试

# 基准测试
./bin/SoftGPU --headless --scene Triangle-Cube
```

---

## ISA 着色器类型 (v1.3)

片元着色器支持 4 种可编程着色器类型：

1. **Flat Color** - 简单直通，带颜色钳制
2. **Barycentric Color** - 顶点颜色插值
3. **Depth Test** - 每像素深度测试
4. **Multi-Triangle** - 多三角形复杂渲染

---

## 性能分析

FrameProfiler + BottleneckDetector 提供完整的性能分析能力：

### 阶段级分析
- **阶段耗时** - 纳秒级精度，60 帧滚动平均
- **各阶段调用次数** - 每帧各级 invocations 统计

### 瓶颈判定
| 瓶颈类型 | 判定依据 |
|----------|----------|
| ShaderBound | FragmentShader 耗时占比高 |
| MemoryBound | GMEM 带宽饱和度高 |
| FillRateBound | 光栅化输出受限 |
| ComputeBound | VertexShader 计算瓶颈 |

### 内存系统统计
- **GMEM 读写字节数** - `getReadBytes() / getWriteBytes()`
- **L2 Cache 命中率** - `getHitRate()` (256KB, 256 sets × 8-way)
- **带宽利用率** - Token Bucket 模型计算
- **访问次数统计** - `getAccessCount()`

### 微架构指标
- **Warp 调度统计** - fragments_executed, instructions_executed, cycles_spent
- **ShaderCore IPC** - `getIPC()` 每周期指令数
- **Rasterizer 效率** - `getRasterizerEfficiency()`

---

## 运行

### GUI 模式（需要显示器）

```bash
./build/bin/SoftGPU
```

### 无头模式（无需显示器）

```bash
# 输出到当前目录
./build/bin/SoftGPU --headless

# 输出到指定目录
./build/bin/SoftGPU --headless --output /tmp

# 自定义文件名
./build/bin/SoftGPU --headless --output-filename my_render.ppm

# 选择场景
./build/bin/SoftGPU --headless --scene Triangle-Cube
```

### 可用场景

| 场景 | 三角形数 | 描述 |
|------|----------|------|
| Triangle-1Tri | 1 | 单个三角形 |
| Triangle-Cube | 12 | 立方体，6 面 |
| Triangle-Cubes-100 | 1200 | 100 个立方体，压力测试 |
| Triangle-SponzaStyle | 变化 | Sponza 风格建筑 |
| PBR-Material | 变化 | PBR 材质球 |

### 示例输出

渲染的 PPM 文件可用任意图像编辑器查看：

```bash
# 渲染并查看输出
./build/bin/SoftGPU --headless --scene Triangle-Cube
# 输出: frame_0000.ppm (640x480)
```

### 渲染效果

| Triangle-1Tri | Triangle-Cube | Triangle-Cubes-100 |
|:---:|:---:|:---:|
| ![1Tri](images/triangle_1tri.png) | ![Cube](images/triangle_cube.png) | ![Cubes](images/triangle_cubes100.png) |

---

## 路线图 (v1.3+)

**当前版本: v1.3** - 片元着色器 ISA 执行，4 种可编程着色器

### 管线微架构改造状态

```
8 级渲染管线微架构实现进度

CommandProcessor     ░░░░░░░░░░░ 0%  [待改造]
       ↓
VertexShader        ░░░░░░░░░░░ 0%  [待改造]
       ↓
PrimitiveAssembly   ░░░░░░░░░░░ 0%  [待改造]
       ↓
TilingStage        ▓▓▓▓░░░░░░ 25%  [部分实现]
       ↓
Rasterizer         ▓▓▓▓░░░░░░ 25%  [部分实现]
       ↓
FragmentShader      ██████████ 100%  [已完成]
       ↓
Framebuffer        ▓▓▓▓░░░░░░ 25%  [部分实现]
       ↓
TileWriteBack      ▓▓▓▓░░░░░░ 25%  [部分实现]

支持模块:
├── ShaderCore       ██████████ 100%  [已完成]
├── Interpreter      ██████████ 100%  [已完成]
├── WarpScheduler   ██████████ 100%  [已完成]
├── MemorySubsystem ██████████ 100%  [已完成]
└── L2Cache        ██████████ 100%  [已完成]
```

### 各阶段详情

| 阶段 | 状态 | 已实现 | 待实现 |
|------|------|--------|--------|
| **CommandProcessor** | ❌ | 命令解析 | 预取队列、并行解码 |
| **VertexShader** | ❌ | MVP 变换 | SIMD 矢量单元、流水线化 |
| **PrimitiveAssembly** | ❌ | 裁剪/装配 | 视图剔除并行化 |
| **TilingStage** | ⚠️ | 三角形 binning | 原子操作、输出缓冲 |
| **Rasterizer** | ⚠️ | DDA 光栅化 | 多样品采样、流水线化 |
| **FragmentShader** | ✅ | ISA 解释器、Warp 调度、36 指令 | 纹理采样单元 |
| **Framebuffer** | ⚠️ | Z-buffer、颜色缓冲 | Early-Z、层级缓冲 |
| **TileWriteBack** | ⚠️ | GMEM ↔ LMEM 同步 | Write Buffer、压缩回写 |

### 版本计划

| 版本 | 主题 | 目标 |
|------|------|------|
| **v1.4** | FragmentShader 增强 + Rasterizer 优化 | 纹理采样单元、MSAA、Write Buffer、Early-Z |
| **v1.5** | 前端管线并行化 | CommandProcessor 预取/解码、VertexShader SIMD/流水线 |
| **v1.6** | 几何处理优化 | PrimitiveAssembly 并行剔除、TilingStage 原子化 |
| **v1.7** | 内存与带宽优化 | L2 Cache 优化、TileWriteBack 压缩、带宽分配器 |
| **v1.8** | 微架构级性能分析 | Warp 分析、IPC/CPI、Cache Miss 分析、瓶颈自动判定 |
| **v2.0** | 多核并行化 | Job System、原子操作、锁-free 管线 |

### v1.8 性能分析特性预览

- **Warp 调度分析** - 占用率、调度延迟、lane 利用率、Divergence 检测
- **ISA 指令级分析** - 每条指令周期分布、IPC/CPI 细分
- **内存系统分析** - L2 Cache miss rate per tile、GMEM 带宽瓶颈
- **自动瓶颈判定** - Shader-bound / Memory-bound / Compute-bound 自动识别

---

## 发布历史

- **v1.3.2** - GUI 模式改进、Framebuffer depth test 修复 (2026-04-03)
- **v1.3.1** - CI 改进、中文 README、微架构路线图 (2026-03-30)
- **v1.3** - Fragment Shader ISA 执行，4 种 ISA 着色器，55 个 E2E 测试 (2026-03-30)
- **v1.2** - 仅文档更新 (2026-03-30)
- **v1.1** - Project Triangulum 补丁: GMEM 接线、DIV Newton-Raphson、TokenBucket、6 个 golden references (2026-03-29)
- **v1.0** - ISA 解释器 + ShaderCore + WarpScheduler，E2E 测试框架 22 个测试 (2026-03-28)
- **v0.5** - 文档发布 (2026-03-27)
- **v0.2** - 初始发布 (2026-03-26)

---

## 许可

MIT License
