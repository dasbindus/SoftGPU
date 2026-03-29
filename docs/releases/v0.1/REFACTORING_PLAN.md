# SoftGPU v0.2 重构计划

**基于：** `CODE_SMELL_REPORT.md` (王刚分析)
**版本：** v0.2
**目标：** 修复代码坏味道，提升可维护性和运行时性能

---

## 重构原则

1. **不影响功能** — 所有重构必须保持现有测试通过
2. **小步快走** — 每个坏味道单独修复，commit 可追溯
3. **先难后易** — 优先处理🔴严重问题
4. **持续验证** — 每修复一个坏味道，运行测试确认

---

## 🔴 严重问题重构（Priority 0）

### R-P0-1：Benchmark 硬编码时间估算

**问题：** `BenchmarkRunner.cpp:203-210` 各阶段耗时硬编码估算（×0.6/0.15/0.1）

**现状：**
```cpp
result.fragmentShaderTimeMs = elapsedMs * 0.6;  // 假数据
result.tilingTimeMs = elapsedMs * 0.1;
result.rasterizerTimeMs = elapsedMs * 0.15;
```

**修复方案：**
- 在 `RenderPipeline` 的各 Stage 关键节点插入计时戳记录
- 通过 `FrameProfiler` 的 `ProfilerGuard` RAII 机制采集真实数据
- `BenchmarkRunner` 从 profiler 获取真实阶段时间

**影响文件：**
- `src/profiler/FrameProfiler.hpp/cpp`
- `src/pipeline/RenderPipeline.cpp`
- `src/benchmark/BenchmarkRunner.cpp`

**预估工时：** 2小时

---

### R-P0-2：TileBuffer/TileWriteBack 逐像素循环改为 memcpy

**问题：** 多处使用 raw loop 逐像素拷贝，而非 `std::memcpy`

**现状：**
```cpp
for (uint32_t i = 0; i < TILE_SIZE; ++i) {
    tile.color[i * 4 + 0] = gmemColor[i * 4 + 0];
    // ...
}
```

**修复方案：**
```cpp
std::memcpy(tile.color, gmemColor, TILE_SIZE * 4 * sizeof(float));
```

**影响文件：**
- `src/stages/TileBuffer.cpp`
- `src/stages/TileWriteBack.cpp`

**预估工时：** 30分钟

---

### R-P0-3：RenderPipeline::render() 180行拆解

**问题：** `RenderPipeline.cpp:50-230` 违反单一职责原则

**拆解方案：**
```
render()
├── initFrame()           // 初始化当前帧
├── processCommand()       // 解析 RenderCommand
├── executeTBR()          // TBR 模式主循环
│   ├── loadTileFromGMEM()
│   ├── rasterizeTile()
│   └── storeTileToGMEM()
├── executeImmediate()    // PHASE1 兼容模式
├── syncGMEMToFramebuffer()
└── updateMetrics()
```

**预估工时：** 3小时

---

### R-P0-4：FragmentShader 裸指针 null check

**问题：** `setTileBufferManager()` 接收裸指针无校验

**修复方案：**
```cpp
void FragmentShader::setTileBufferManager(TileBufferManager* manager) {
    if (manager == nullptr) {
        LOG_ERROR("FragmentShader: TileBufferManager cannot be null");
        return;
    }
    m_tileBuffer = manager;
}
```

**预估工时：** 15分钟

---

### R-P0-5：MemorySubsystem 带宽模型失效

**问题：** `tryConsume()` 返回值被忽略，带宽耗尽仍继续执行

**修复方案：**
```cpp
bool MemorySubsystem::readGMEM(void* dst, uint64_t offset, size_t bytes) {
    bool allowed = m_bucket.tryConsume(bytes);
    if (!allowed) {
        // 等待令牌补充或降低请求速率
        return false;
    }
    // ...
}
```

**预估工时：** 1小时

---

## 🟡 中等问题重构（Priority 1）

### R-P1-1：双输入源模式提取

**问题：** `FragmentShader`、`Framebuffer`、`Rasterizer` 中重复出现相同的双输入源判断逻辑

**现状：**
```cpp
const std::vector<Fragment>& input = (m_inputVersion == 1 && m_inputFragmentsPtr != nullptr)
    ? *m_inputFragmentsPtr
    : m_inputFragments;
```

**修复方案：** 在 Stage 基类中提取：
```cpp
protected:
    const T& getInput() const {
        return (m_inputVersion == 1 && m_inputPtr != nullptr) 
            ? *m_inputPtr 
            : m_input;
    }
```

**预估工时：** 2小时

---

### R-P1-2：魔法数字提取

**问题：** 多处硬编码数字常量无命名

**提取清单：**
| 原值 | 建议常量名 |
|------|-----------|
| `1e-8f` | `EPSILON_TRIANGLE_AREA` |
| `0.70` | `THRESHOLD_SHADER_BOUND_FS_RATIO` |
| `0.50` | `THRESHOLD_SHADER_BOUND_CORE_UTIL` |
| `0.85` | `THRESHOLD_MEMORY_BOUND_BW` |
| `0.70` | `THRESHOLD_MEMORY_BOUND_CORE_UTIL` |
| `0.30` | `THRESHOLD_FILL_RATE_EFFICIENCY` |

**预估工时：** 1小时

---

### R-P1-3：RenderPipeline 依赖过多

**问题：** `RenderPipeline` 依赖 12+ 个类，违反依赖倒置

**修复方案：** 引入接口抽象
```cpp
class IPipelineStage { ... };
class IMemoryAccess { ... };
class IProfiler { ... };  // 已有
```

**预估工时：** 4小时

---

### R-P1-4：错误处理规范化

**问题：** 多处静默失败或错误返回

**修复方案：**
- `depthTestAndWrite()` 返回 `std::optional<bool>` 或使用错误码
- `readGMEM()` 带宽不足时正确返回 false
- `BenchmarkRunner` 找不到场景时返回错误而非伪造数据

**预估工时：** 2小时

---

## 🟢 建议项重构（Priority 2）

| # | 问题 | 修复方案 | 工时 |
|---|------|---------|------|
| R-P2-1 | 命名不直观（`M()`/`m_inputVersion`）| 重命名 | 1h |
| R-P2-2 | 注释不足 | 补充关键算法注释 | 1h |
| R-P2-3 | `ProfilerUI::render()` 88行 | 拆分为多个 `render*()` 方法 | 1h |
| R-P2-4 | `TileBuffer::executeTile()` 向量拷贝开销 | 改为指针引用 | 1h |
| R-P2-5 | PI 常量重复定义 | 统一使用 `core/Math.hpp` | 30min |
| R-P2-6 | 死代码/API | 清理 | 30min |

---

## 重构里程碑

### Milestone 1：运行时性能（v0.2.1）
- [ ] R-P0-1：Benchmark 真实计时
- [ ] R-P0-2：memcpy 优化
- [ ] R-P0-5：带宽模型修正

### Milestone 2：代码质量（v0.2.2）
- [ ] R-P0-3：render() 拆解
- [ ] R-P0-4：null check
- [ ] R-P1-1：双输入源提取
- [ ] R-P1-2：魔法数字常量

### Milestone 3：架构优化（v0.2.3）
- [ ] R-P1-3：依赖倒置
- [ ] R-P1-4：错误处理规范化
- [ ] R-P2-*：其他建议项

---

## 预估总工时

| 优先级 | 问题数 | 预估工时 |
|--------|--------|---------|
| 🔴 P0 | 5 | ~7小时 |
| 🟡 P1 | 4 | ~9小时 |
| 🟢 P2 | 6 | ~5小时 |
| **合计** | 15 | **~21小时** |

---

## 验收标准

1. 所有现有测试仍然通过
2. Benchmark 数据基于真实计时
3. TileBuffer/TileWriteBack 使用 memcpy
4. render() 函数拆分为可读的子函数
5. 无裸指针 null check 遗漏
6. 魔法数字提取为命名常量

---

*计划制定：白小东*
*基于坏味道分析：王刚*
