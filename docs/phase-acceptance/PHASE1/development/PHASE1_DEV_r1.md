# PHASE1_DEV_r1.md - P0/P1 问题修复报告

**日期:** 2026-03-26
**修复者:** 白小西（@xiaoxi）
**版本:** PHASE1_DEV_r1

---

## 修复的问题清单

### P0 问题

| 问题编号 | 描述 | 状态 | 修改文件 |
|---------|------|------|----------|
| P0-1 | TileWriteBack readTileFromGMEM 索引计算错误 (`tileOffset * 4` 应为 `tileOffset`) | **已修复** | `src/stages/TileWriteBack.cpp` |

### P1 问题

| 问题编号 | 描述 | 状态 | 修改文件 |
|---------|------|------|----------|
| P1-1 | PrimitiveAssembly 索引绘制三角形数量计算不清晰 | **已修复** | `src/stages/PrimitiveAssembly.hpp`, `src/stages/PrimitiveAssembly.cpp`, `src/pipeline/RenderPipeline.cpp` |
| P1-2 | RenderPipeline connectStages() 未实现 | **已修复** | `src/pipeline/RenderPipeline.cpp` |
| P1-3 | VertexShader 缺少 near-plane 剔除 | **已修复** | `src/stages/VertexShader.cpp`, `src/core/PipelineTypes.hpp` |
| P1-4 | Framebuffer depthTest() 声明 const 但实现违反 | **已确认无需修复** | - |
| P1-5 | RenderPipeline indexed 判断逻辑不一致 | **已修复** (随 P1-1 修复) | - |

---

## 详细修复说明

### P0-1: TileWriteBack readTileFromGMEM 索引计算错误

**问题:** `readTileFromGMEM` 中使用 `tileOffset * 4 + i * 4 + channel` 计算索引，但 `tileOffset` 已经是像素偏移量，不应再乘以 4。

**修复:**
```cpp
// 修复前 (错误):
outColor[i * 4 + 0] = m_gmemColor[tileOffset * 4 + i * 4 + 0];

// 修复后 (正确):
outColor[i * 4 + 0] = m_gmemColor[tileOffset + i * 4 + 0];
```

### P1-1: PrimitiveAssembly 索引绘制三角形数量计算

**问题:** `execute()` 使用 `m_inputIndices.empty()` 判断是否索引绘制，这不安全。

**修复:** 
- 为 `PrimitiveAssembly` 添加 `m_indexed` 成员变量
- 修改 `setInput()` 接受第三个参数 `indexed`
- `execute()` 使用 `m_indexed` 而非 `m_inputIndices.empty()` 判断

### P1-2: RenderPipeline connectStages() 未实现

**问题:** `connectStages()` 函数声明但未实现，且未被调用。

**修复:**
- 实现 `connectStages()` 函数，将 framebuffer 连接逻辑移至其中
- 在构造函数中调用 `connectStages()`

### P1-3: VertexShader 缺少 near-plane 剔除

**问题:** 设计文档要求检查 `w > 0`，但代码未实现。

**修复:**
- 在 `Vertex` 结构体中添加 `culled` 字段
- 在 `VertexShader::transformVertex()` 中，当 `w <= 0` 时设置 `culled = true`
- 在 `PrimitiveAssembly::execute()` 中检查顶点 culled 状态

### P1-4: Framebuffer depthTest() const

**问题描述:** 声称 `depthTest()` 声明 const 但实现违反。

**实际情况:** 经检查，当前代码中 `depthTest()` 并未声明 const，无需修复。

### P1-5: RenderPipeline indexed 判断逻辑不一致

**问题:** `execute()` 检查 `empty()`，`render()` 检查 `indexed` flag。

**修复:** 此问题随 P1-1 修复一并解决，现在 `PrimitiveAssembly` 使用显式的 `m_indexed` 标志。

---

## 测试结果

### 编译状态

| 组件 | 状态 |
|------|------|
| stages 库 | ✅ 编译通过 |
| pipeline 库 | ✅ 编译通过 |
| test_VertexShader | ✅ 编译通过 |
| test_PrimitiveAssembly | ✅ 编译通过 |
| test_Framebuffer | ✅ 编译通过 |
| test_Integration | ✅ 编译通过 |

### 测试执行结果

```
[==========] Running 3 tests from VertexShaderTest
[  PASSED  ] 3 tests.

[==========] Running 5 tests from PrimitiveAssemblyTest
[  PASSED  ] 5 tests.

[==========] Running 7 tests from FramebufferTest
[  PASSED  ] 7 tests.

[==========] Running 4 tests from IntegrationTest
[  PASSED  ] 4 tests.
```

**所有测试均通过！**

---

## 修改的文件清单

### 源代码文件

1. `src/stages/TileWriteBack.cpp` - P0-1 修复
2. `src/stages/PrimitiveAssembly.hpp` - P1-1 修复
3. `src/stages/PrimitiveAssembly.cpp` - P1-1, P1-3 修复
4. `src/pipeline/RenderPipeline.cpp` - P1-1, P1-2 修复
5. `src/stages/VertexShader.cpp` - P1-3 修复
6. `src/core/PipelineTypes.hpp` - P1-3 修复 (Vertex.culled 字段)

### 测试文件

1. `tests/core/math_test.cpp` - 预存 bug 修复 (GLM 命名空间)
2. `tests/stages/test_PrimitiveAssembly.cpp` - API 变更适配

---

## Git Commit 记录

```
c5abfa6 fix(P0-1): TileWriteBack readTileFromGMEM 索引计算错误
ee48981 fix(P1-1): PrimitiveAssembly 索引绘制三角形数量计算
5b1c488 fix(P1-2): RenderPipeline connectStages 实现
31afe92 fix(P1-3): VertexShader 添加 near-plane 剔除
53edb34 fix(tests): 修复测试文件以匹配 API 变更
```

---

## 备注

1. **P1-4 确认无需修复:** 经检查，`Framebuffer::depthTest()` 在当前代码中并未声明 const，此问题可能是描述过时或已在早期版本修复。

2. **P1-5 随 P1-1 修复:** indexed 判断逻辑一致性已通过 P1-1 的修复得到解决。

3. **测试文件修复:** 发现并修复了 `tests/core/math_test.cpp` 中的预存 bug（使用 `vec4` 而非 `glm::vec4`）。

---

**报告生成时间:** 2026-03-26 04:08 GMT+8
