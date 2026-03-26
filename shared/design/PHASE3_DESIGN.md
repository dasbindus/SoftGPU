# PHASE3 Design Document — SoftGPU TBR 收尾与 GMEM 联通

**项目名称：** SoftGPU Pixel Workshop
**阶段：** PHASE3 — TBR 完整性修复
**架构师：** 陈二虎（@erhu）
**日期：** 2026-03-26
**版本：** v1.0
**依赖：** PHASE2（TBR 管线骨架 + TilingStage）

---

## 1. 阶段目标

### 1.1 核心目标

修复 PHASE2 遗留的 3 个 Critical 问题，使 TBR 管线在 GMEM 层面真正联通：

1. **Critical 1**：`executeTile()` 中 `loadTileFromGMEM()` / `storeTileToGMEM()` 是空调用
2. **Critical 2**：`storeAllTilesFromBuffer()` 不调用 `memory->addAccess()`，带宽计数器永远为 0
3. **Critical 3**：`MemorySubsystem::readGMEM/writeGMEM` 只有 L2 cache 调用，没有实际 memcpy

### 1.2 与 PHASE2 的关键差异

| 维度 | PHASE2 | PHASE3 |
|------|--------|--------|
| executeTile GMEM load | 空调用 `(void)` 占位 | 真实 memcpy GMEM → TileBuffer |
| executeTile GMEM store | 注释掉的代码 | 真实 memcpy TileBuffer → GMEM |
| storeAllTilesFromBuffer 带宽 | 无 addAccess，计数器为 0 | 每次 tile 写回记录带宽 |
| readGMEM/writeGMEM | 只有 cache 模拟，无 memcpy | 实现真实 GMEM 内存读写 |
| GMEM 数据来源 | 初始化为 0 | TileWriteBack 持有真实 GMEM 缓冲区 |

---

## 2. PHASE2 Critical 问题修复方案

### 2.1 Critical 1：executeTile() GMEM load/store 未连线

**文件：** `src/pipeline/RenderPipeline.cpp`

**现状分析：**
```cpp
void RenderPipeline::executeTile(uint32_t tileIndex, ...) {
    TileBuffer& tileMem = m_tileBuffer.getTileBuffer(tileIndex);

    // Step 1: 空调用，只有 (void) 占位
    (void)m_tileWriteBack;
    (void)tileMem;

    // Step 4: 完全注释掉了
    // m_tileWriteBack.storeTileToGMEM(tileIndex, &m_memory, tileMem);
}
```

**修复方案：**

```cpp
void RenderPipeline::executeTile(uint32_t tileIndex, ...) {
    // ... triangle list 构建 ...

    TileBuffer& tileMem = m_tileBuffer.getTileBuffer(tileIndex);

    // ===== Step 1: Load tile from GMEM =====
    // 如果 tile 有历史数据（多 frame 累积），从 GMEM 加载
    // 如果 tile 无历史（首次或被 clear），跳过 load（TileBuffer 已 clear）
    if (hasHistoryLoad) {
        m_tileWriteBack.loadTileFromGMEM(tileIndex, &m_memory, tileMem);
    }

    // ===== Step 2-3: Rasterizer + FragmentShader (不变) =====

    // ===== Step 4: Store tile to GMEM (per-tile 写回) =====
    // PHASE3 改为每 tile 完成后立即写回（而非批量写回）
    // 正确性：支持多 frame 累积时 load/store 配对
    m_tileWriteBack.storeTileToGMEM(tileIndex, &m_memory, tileMem);
}
```

**loadTileFromGMEM 调用的数据流：**
```
GMEM (m_gmemColor/m_gmemDepth)
    ↓ memcpy (TILE_SIZE * 4 * sizeof(float) + TILE_SIZE * sizeof(float))
TileBuffer.color / TileBuffer.depth
```

**storeTileToGMEM 调用的数据流：**
```
TileBuffer.color / TileBuffer.depth
    ↓ memcpy (same size)
GMEM (m_gmemColor/m_gmemDepth)
```

### 2.2 Critical 2：storeAllTilesFromBuffer() 带宽计数器为 0

**文件：** `src/stages/TileWriteBack.cpp`

**现状分析：**
`storeAllTilesFromBuffer()` 只做 memcpy，没有调用 `memory->addAccess()`：
```cpp
void TileWriteBack::storeAllTilesFromBuffer(const TileBufferManager& manager) {
    for (uint32_t tileIndex = 0; tileIndex < NUM_TILES; ++tileIndex) {
        const auto& tile = manager.getTileBuffer(tileIndex);
        // ... memcpy color + depth to m_gmemColor/m_gmemDepth ...
        // BUG: 没有 memory->addAccess() 调用！
    }
}
```

**修复方案：**

由于 `storeAllTilesFromBuffer` 在 PHASE3 中被 per-tile store 替代（Critical 1 修复后不再调用），带宽统计已在 `storeTileToGMEM()` 中实现。但为保持 `storeAllTilesFromBuffer` 的可调用性（PHASE1 兼容路径），修复如下：

```cpp
void TileWriteBack::storeAllTilesFromBuffer(const TileBufferManager& manager,
                                           MemorySubsystem* memory) {
    for (uint32_t tileIndex = 0; tileIndex < NUM_TILES; ++tileIndex) {
        const auto& tile = manager.getTileBuffer(tileIndex);

        // ===== 带宽统计 =====
        size_t colorBytes = TILE_SIZE * 4 * sizeof(float);  // 16KB
        size_t depthBytes = TILE_SIZE * sizeof(float);       //  4KB
        size_t totalBytes = colorBytes + depthBytes;          // 20KB

        if (memory) {
            memory->addAccess(colorBytes, MemoryAccessType::StoreTile);
            memory->recordWrite(colorBytes);
            memory->addAccess(depthBytes, MemoryAccessType::StoreTile);
            memory->recordWrite(depthBytes);
        }

        // ===== 实际 memcpy =====
        size_t colorOffset = getTileColorOffset(tileIndex);
        size_t depthOffset = getTileDepthOffset(tileIndex);

        std::memcpy(&m_gmemColor[colorOffset], tile.color.data(), colorBytes);
        std::memcpy(&m_gmemDepth[depthOffset], tile.depth.data(), depthBytes);
    }
}
```

**注意：** `RenderPipeline::render()` 中对 `storeAllTilesFromBuffer` 的调用需要传入 `&m_memory`：
```cpp
m_tileWriteBack.storeAllTilesFromBuffer(m_tileBuffer, &m_memory);
```

### 2.3 Critical 3：MemorySubsystem GMEM 是空壳

**文件：** `src/core/MemorySubsystem.cpp`

**现状分析：**
```cpp
void MemorySubsystem::readGMEM(void* dst, uint64_t offset, size_t bytes) {
    // 只有 L2 cache 模拟，没有实际 memcpy
    // ...
    // 调用者负责实际的内存拷贝（但没人调用 memcpy！）
    recordRead(bytes);  // 只记录了字节数，没有真实数据移动
}
```

`readGMEM` / `writeGMEM` 目前只做 cache 模拟，不做实际数据搬运。但实际上，GMEM 数据在 `TileWriteBack` 的 `m_gmemColor` / `m_gmemDepth` 中，真正的 memcpy 应该在 `TileWriteBack` 层面做。

**修复方案（分层职责明确）：**

| 层级 | 职责 |
|------|------|
| `TileWriteBack` | 持有 GMEM 缓冲区（`m_gmemColor/m_gmemDepth`），做实际 memcpy |
| `MemorySubsystem` | 令牌桶带宽扣减 + L2 cache 模拟 + 带宽统计 |

因此 `readGMEM` / `writeGMEM` 的职责调整为：

```cpp
void MemorySubsystem::readGMEM(void* dst, uint64_t offset, size_t bytes,
                                const float* gmemBase) {
    // 1. L2 cache 模拟
    uint64_t addr = offset;
    size_t lineAlign = addr & ~(CACHE_LINE_SIZE - 1);
    size_t endAddr = addr + bytes;
    while (lineAlign < endAddr) {
        m_l2Cache.access(lineAlign, false);
        lineAlign += CACHE_LINE_SIZE;
    }

    // 2. 实际 memcpy（从 GMEM 缓冲区到 dst）
    //    gmemBase 指向 TileWriteBack::m_gmemColor 或 m_gmemDepth
    if (gmemBase && dst) {
        std::memcpy(dst, gmemBase + offset, bytes);
    }

    // 3. 带宽记录
    recordRead(bytes);
    addAccess(bytes, MemoryAccessType::LoadTile);
}

void MemorySubsystem::writeGMEM(uint64_t offset, const void* src, size_t bytes,
                                 float* gmemBase) {
    // 1. L2 cache 模拟
    uint64_t addr = offset;
    size_t lineAlign = addr & ~(CACHE_LINE_SIZE - 1);
    size_t endAddr = addr + bytes;
    while (lineAlign < endAddr) {
        m_l2Cache.access(lineAlign, true);
        lineAlign += CACHE_LINE_SIZE;
    }

    // 2. 实际 memcpy（从 src 到 GMEM 缓冲区）
    if (gmemBase && src) {
        std::memcpy(gmemBase + offset, src, bytes);
    }

    // 3. 带宽记录
    recordWrite(bytes);
    addAccess(bytes, MemoryAccessType::StoreTile);
}
```

**新增接口：**
- `readGMEM` 增加 `const float* gmemBase` 参数
- `writeGMEM` 增加 `float* gmemBase` 参数
- `TileWriteBack::loadTileFromGMEM` 调用 `memory->readGMEM(..., &m_gmemColor[0], ...)`
- `TileWriteBack::storeTileToGMEM` 调用 `memory->writeGMEM(..., &m_gmemColor[0], ...)`

---

## 3. GMEM 内存模型

### 3.1 物理规格

| 参数 | 值 |
|------|-----|
| Framebuffer 分辨率 | 640 × 480 |
| Tile Size | 32 × 32 |
| Tiles 总数 | 20 × 15 = 300 |
| 每个 Tile 像素数 | 32 × 32 = 1024 |
| Color 格式 | RGBA float32 = 4 × 4 bytes = 16 bytes/pixel |
| Depth 格式 | float32 = 4 bytes/pixel |

### 3.2 GMEM 大小计算

```
GMEM Color = 300 tiles × 1024 pixels/tile × 16 bytes/pixel
           = 300 × 1024 × 16
           = 4,915,200 bytes  ≈ 4.69 MB

GMEM Depth = 300 tiles × 1024 pixels/tile × 4 bytes/pixel
           = 300 × 1024 × 4
           = 1,228,800 bytes  ≈ 1.17 MB

GMEM Total ≈ 5.86 MB
```

### 3.3 内存布局

GMEM Color 布局：行优先（tile 网格内按 tile 行展开）

```
Tile Grid (20×15):
Row 0:  Tile 0 .. Tile 19        (each 32×32 pixels × 16B = 16KB)
Row 1:  Tile 20 .. Tile 39
...
Row 14: Tile 280 .. Tile 299

Linear offset for tileIndex:
  tileOffset = tileIndex × TILE_SIZE (in pixels)

Byte offset for color:
  colorByteOffset = tileOffset × 16

Byte offset for depth:
  depthByteOffset = tileOffset × 4
```

### 3.4 读写延迟模型（简化）

```
Latency Model (简化版，无真实延迟注入):
  GMEM Read:  ~50 ns （模拟读取延迟，可配置）
  GMEM Write: ~50 ns

  每 tile load:  TILE_SIZE × 16B (color) + TILE_SIZE × 4B (depth)
               = 16KB + 4KB = 20KB
               ≈ 0.16 μs @ 100 GB/s

  每 tile store: 同样 20KB
```

**延迟模拟策略（PHASE3 简化版）：**
暂不注入真实等待时间（PHASE4 多线程时再引入）。PHASE3 重点是数据通路打通 + 带宽计数正确。

---

## 4. Tile 同步详细设计

### 4.1 loadTileFromGMEM 完整实现

```cpp
void TileWriteBack::loadTileFromGMEM(uint32_t tileIndex,
                                     MemorySubsystem* memory,
                                     TileBuffer& outTileBuffer) {
    // Step 1: 计算 GMEM 偏移
    size_t colorOffset = getTileColorOffset(tileIndex);  // in floats
    size_t depthOffset = getTileDepthOffset(tileIndex);  // in floats

    size_t colorBytes = TILE_SIZE * 4 * sizeof(float);  // 16KB
    size_t depthBytes = TILE_SIZE * sizeof(float);       //  4KB

    // Step 2: 记录带宽（通过 MemorySubsystem）
    if (memory) {
        // Color load: L2 cache sim + bandwidth consume
        memory->readGMEM(
            outTileBuffer.color.data(),     // dst
            colorOffset * sizeof(float),    // offset in bytes
            colorBytes,                     // bytes
            m_gmemColor.data()              // GMEM base pointer
        );

        // Depth load
        memory->readGMEM(
            outTileBuffer.depth.data(),
            depthOffset * sizeof(float),
            depthBytes,
            m_gmemDepth.data()
        );
    } else {
        // Fallback：无 memory 系统时直接 memcpy
        std::memcpy(outTileBuffer.color.data(),
                    &m_gmemColor[colorOffset], colorBytes);
        std::memcpy(outTileBuffer.depth.data(),
                    &m_gmemDepth[depthOffset], depthBytes);
    }
}
```

### 4.2 storeTileToGMEM 完整实现

```cpp
void TileWriteBack::storeTileToGMEM(uint32_t tileIndex,
                                    MemorySubsystem* memory,
                                    const TileBuffer& tileBuffer) {
    size_t colorOffset = getTileColorOffset(tileIndex);
    size_t depthOffset = getTileDepthOffset(tileIndex);

    size_t colorBytes = TILE_SIZE * 4 * sizeof(float);
    size_t depthBytes = TILE_SIZE * sizeof(float);

    if (memory) {
        // Color store
        memory->writeGMEM(
            colorOffset * sizeof(float),    // offset in bytes
            tileBuffer.color.data(),         // src
            colorBytes,
            m_gmemColor.data()               // GMEM base
        );

        // Depth store
        memory->writeGMEM(
            depthOffset * sizeof(float),
            tileBuffer.depth.data(),
            depthBytes,
            m_gmemDepth.data()
        );
    } else {
        std::memcpy(&m_gmemColor[colorOffset],
                    tileBuffer.color.data(), colorBytes);
        std::memcpy(&m_gmemDepth[depthOffset],
                    tileBuffer.depth.data(), depthBytes);
    }
}
```

### 4.3 executeTile 完整修复后的流程

```cpp
void RenderPipeline::executeTile(uint32_t tileIndex, uint32_t tileX, uint32_t tileY) {
    const auto& bin = m_tilingStage.getTileBin(tileIndex);
    const std::vector<Triangle>& allTriangles = m_tilingStage.getInputTriangles();

    // Build per-tile triangle list
    std::vector<Triangle> tileTriangles;
    tileTriangles.reserve(bin.triangleIndices.size());
    for (uint32_t idx : bin.triangleIndices) {
        tileTriangles.push_back(allTriangles[idx]);
    }

    TileBuffer& tileMem = m_tileBuffer.getTileBuffer(tileIndex);

    // ===== PHASE3 FIX: GMEM Load =====
    // Clear tile buffer first (reset to clear values)
    tileMem.clear();
    // Then load existing GMEM content (if any)
    m_tileWriteBack.loadTileFromGMEM(tileIndex, &m_memory, tileMem);

    // ===== Rasterizer per-tile =====
    m_rasterizer.setInputPerTile(tileTriangles, tileX, tileY);
    m_rasterizer.executePerTile();
    const auto& fragments = m_rasterizer.getOutput();

    // ===== FragmentShader =====
    m_fragmentShader.setTileBufferManager(&m_tileBuffer);
    m_fragmentShader.setTileIndex(tileIndex);
    m_fragmentShader.setInputAndExecuteTile(fragments, tileX, tileY);

    // ===== PHASE3 FIX: GMEM Store =====
    m_tileWriteBack.storeTileToGMEM(tileIndex, &m_memory, tileMem);
}
```

---

## 5. 带宽统计实现

### 5.1 带宽统计点汇总

| 操作 | 统计方法 | 字节数 |
|------|----------|--------|
| Tile load (color) | `memory->readGMEM(..., colorBytes)` | TILE_SIZE × 4 × 4 = 16KB |
| Tile load (depth) | `memory->readGMEM(..., depthBytes)` | TILE_SIZE × 4 = 4KB |
| Tile store (color) | `memory->writeGMEM(..., colorBytes)` | 16KB |
| Tile store (depth) | `memory->writeGMEM(..., depthBytes)` | 4KB |

**每 tile load+store 合计：** 40KB（color+depth load + color+depth store）

### 5.2 MemorySubsystem 统计接口

```cpp
// 当前已支持：
uint64_t getReadBytes()   const { return m_totalReadBytes; }
uint64_t getWriteBytes()  const { return m_totalWriteBytes; }
uint64_t getAccessCount() const { return m_accessCount; }
double   getBandwidthUtilization() const;

// 新增 helper：
uint64_t getTotalBytes() const { return m_totalReadBytes + m_totalWriteBytes; }
```

### 5.3 预期带宽数字（300 tiles 全量 load+store）

```
理论值（无 cache，全量写回）:
  300 tiles × 40KB/tile = 12,000 KB = 11.72 MB

  @ 100 GB/s ≈ 0.117 ms 理论时间
  @ 实际利用率 80% ≈ 0.147 ms
```

### 5.4 L2 Cache 效果

L2 Cache（128KB，8-way，256 sets）模拟 tile 级别的缓存：

- **首次 load tile**：MISS，填充 cache
- **多 triangle 覆盖同一 tile**：L2 cache hit，避免重复 GMEM 读取
- **同一 tile 多次写回**：最后一版直接写 cover（write-through 简化）

命中率统计通过 `getL2Cache().getHitRate()` 获取。

---

## 6. 验收标准

### 6.1 Critical 1 验收

- [ ] `executeTile()` 中 `loadTileFromGMEM()` 被调用（打断点或加日志）
- [ ] `executeTile()` 中 `storeTileToGMEM()` 被调用
- [ ] GMEM 数据在 `storeTileToGMEM` 后被正确写入 `m_gmemColor` / `m_gmemDepth`

### 6.2 Critical 2 验收

- [ ] `storeAllTilesFromBuffer()` 新增 `MemorySubsystem* memory` 参数
- [ ] `storeAllTilesFromBuffer()` 内部调用 `memory->addAccess()` 和 `recordWrite()`
- [ ] `RenderPipeline::render()` 中 `storeAllTilesFromBuffer(m_tileBuffer, &m_memory)` 传入 memory
- [ ] 运行一帧后，`m_memory.getReadBytes() > 0` 且 `m_memory.getWriteBytes() > 0`

### 6.3 Critical 3 验收

- [ ] `readGMEM()` / `writeGMEM()` 签名增加 `gmemBase` 参数
- [ ] `readGMEM()` 内部执行 `std::memcpy(dst, gmemBase + offset, bytes)`
- [ ] `writeGMEM()` 内部执行 `std::memcpy(gmemBase + offset, src, bytes)`
- [ ] `TileWriteBack::loadTileFromGMEM` 调用时传入 `m_gmemColor.data()` / `m_gmemDepth.data()`

### 6.4 整体验收

- [ ] TBR 管线编译通过，无 link 错误
- [ ] 运行 triangle 渲染测试，输出正确的 PPM dump
- [ ] `printPerformanceReport()` 中 `read_bytes` 和 `write_bytes` 均大于 0
- [ ] `bandwidth_util` 有合理数值（非 NaN，非 0）
- [ ] L2 cache hit rate 可被查询（不为 NaN）

### 6.5 性能目标

| 指标 | 目标 |
|------|------|
| 每帧 GMEM 读/写 | ~11.72 MB（全 tile load+store） |
| 带宽利用率 | < 100%（不超过硬件上限） |
| L2 hit rate | > 0%（多 triangle 覆盖同一 tile 时应有命中） |

---

## 附录：修改文件清单

| 文件 | 修改类型 |
|------|----------|
| `src/core/MemorySubsystem.hpp` | 接口修改：readGMEM/writeGMEM 增加 gmemBase 参数 |
| `src/core/MemorySubsystem.cpp` | 实现修改：readGMEM/writeGMEM 增加实际 memcpy |
| `src/stages/TileWriteBack.hpp` | 接口修改：loadTileFromGMEM/storeTileToGMEM 签名确认；storeAllTilesFromBuffer 增加 memory 参数 |
| `src/stages/TileWriteBack.cpp` | 实现修改：带宽统计 + memcpy 实现 |
| `src/pipeline/RenderPipeline.cpp` | 实现修改：executeTile() 连接 GMEM load/store |
