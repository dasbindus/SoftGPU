# SoftGPU 架构与设计

一个软件实现的 Tile-Based GPU 模拟器，再现真实 GPU 渲染管线。

---

## 概述

SoftGPU 模拟现代 Tile-Based Deferred Rendering (TBDR) GPU 架构。提供实时性能分析、瓶颈检测和基准测试自动化。

**核心设计目标：**
- 忠实实现 GPU 渲染管线
- 精确的内存带宽模型
- 实时性能分析
- 可扩展架构

---

## 系统架构

### 8 级渲染管线

```
┌─────────────────────┐
│ CommandProcessor    │  解析 DrawCall，顶点缓冲
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ VertexShader        │  MVP 变换，顶点裁剪
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ PrimitiveAssembly    │  视锥剔除，三角形组装
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ TilingStage         │  三角形分箱（300 tiles, 20×15）
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ Rasterizer          │  Edge Function DDA，逐 tile 光栅化
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ FragmentShader      │  片元处理，着色
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ Framebuffer         │  Z-Buffer 深度测试，颜色写入
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│ TileWriteBack       │  Tile 数据写回 GMEM
└─────────────────────┘
```

---

## 内存架构

### GMEM（全局内存）

片外 DRAM，存储渲染后的 tile 数据。

- **容量：** 300 tiles × 20KB = 6MB
- **带宽模型：** 令牌桶算法
- **访问模式：** 顺序 tile 写回

### LMEM（本地内存）

片上 tile 内存（per tile）。

- **大小：** 32×32 像素 × 4 通道 × 4 字节 = 16KB per tile
- **用途：** Tile 渲染时快速读写

### L2 缓存

GMEM 与 shader 核心之间的模拟 L2 缓存。

- **行大小：** 64 字节
- **关联度：** 4 路组相联
- **命中率：** 每次访问跟踪

---

## Tile-Based 渲染流程

### 阶段一：几何处理

```
1. CommandProcessor 接收 DrawCall
2. VertexShader 变换顶点（MVP 矩阵）
3. PrimitiveAssembly 执行视锥剔除
4. TilingStage 将三角形分配到 tile 列表（300 tiles）
```

### 阶段二：逐 Tile 渲染

```
对每个 tile（按 tile 顺序）:
    1. 从 GMEM 加载 tile → LMEM
    2. 对 tile bin 中的每个三角形:
        a. 光栅化 → 产生片元
        b. FragmentShader 着色片元
        c. Framebuffer 深度测试 + 颜色写入
    3. 从 LMEM 存储 tile → GMEM
```

### 阶段三：帧输出

```
1. 从 GMEM 复制所有 tiles 到 framebuffer
2. 输出到显示（测试用 PPM 转储）
```

---

## 性能分析

### 采集指标

| 指标 | 描述 |
|------|------|
| Stage Time | 每级纳秒级精度计时 |
| Bandwidth | GMEM 读写字节数 |
| L2 Hit Rate | 缓存效率 |
| Fragment Count | 处理的总片元数 |

### 瓶颈检测

三维度分析：

```
Shader Bound:    FS 时间 > 70% && 核心利用率 < 50%
Memory Bound:   带宽 > 85% && 核心利用率 < 70%
Fill Rate:      光栅器输出 < 峰值 30%
```

---

## 核心算法

### Tiling（分箱）

每个三角形经过光栅化以确定它覆盖哪些 tile：

```cpp
for (每个三角形):
    计算边界框 (minX, maxX, minY, maxY)
    for (边界框内的每个 tile):
        if (三角形覆盖 tile 中心):
            添加三角形到 tile 的 bin
```

### Edge Function 光栅化

```
对每个 tile:
    for (tile bin 中的每个三角形):
        计算 tile 角点的边缘函数
        插值深度和颜色
        if (在三角形内) 写入片元
```

### Z-Buffer 深度测试

```cpp
depthTestAndWrite(x, y, z, color):
    if z < zBuffer[y][x]:
        zBuffer[y][x] = z
        colorBuffer[y][x] = color
        return true
    return false
```

---

## 内存带宽模型

### 令牌桶算法

```
令牌：表示可用带宽
速率：每时间单位补充量
容量：最大令牌数

tryConsume(bytes):
    if 令牌 >= bytes:
        令牌 -= bytes
        return true
    return false
```

### 带宽计算

```
read_bytes = tiles × tile_size × load_passes
write_bytes = tiles × tile_size × store_passes
total_bandwidth = read_bytes + write_bytes
```

---

## 测试策略

### 单元测试

- Framebuffer（清空、写入、深度测试）
- Rasterizer（边缘函数、插值）
- PrimitiveAssembly（剔除）
- VertexShader（矩阵变换）

### 集成测试

- GreenTriangle（单三角形渲染）
- ZBuffer（深度测试顺序）
- ColorInterpolation（Gouraud 着色）

### 基准测试

- FPS 测量
- 级时间精度
- 带宽计算验证

---

## 未来扩展

- SIMD 光栅化（SSE/AVX）
- 多线程 tile 处理
- Vulkan 后端
- 更多场景格式（OBJ、glTF）
