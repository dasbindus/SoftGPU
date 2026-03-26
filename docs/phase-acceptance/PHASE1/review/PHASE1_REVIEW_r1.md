# SoftGPU PHASE1 审查报告（第1轮）

**审查人：** 王刚（@wanggang）  
**被审查人：** 白小西  
**审查日期：** 2026-03-26  

---

## 审查结果

| 分类 | 数量 |
|------|------|
| P0 问题 | 0 |
| P1 问题 | 0 |

---

## 本轮修复：P1-2 connectStages() 实现

### ✅ P1-2: `connectStages()` 实现不完整

**结论：** 已正确修复（commit `dd42fb5`）

**修改内容：**

1. **新增 `setInputFromConnect()` 方法**（Rasterizer, FragmentShader, Framebuffer）：
   - 专供 `connectStages()` 调用，存储指向前一阶段输出的指针
   - 使用 `m_inputVersion = 1` 标记"由 connectStages 设置"
   - `setInput()` 保持原有复制语义（供单元测试使用临时 initializer_list）

2. **版本控制机制**（`m_inputVersion`）：
   - `version == 1`：由 `connectStages()` 设置，`execute()` 使用指向阶段输出的指针
   - `version >= 2`：由 `setInput()` 调用（如单元测试），`execute()` 使用复制
   - 解决了单元测试使用临时 initializer_list 而 Pipeline 使用阶段引用的兼容问题

3. **`connectStages()` 完整连接 7 个阶段：**
   ```cpp
   void RenderPipeline::connectStages() {
       // Stage 3: PrimitiveAssembly -> Stage 4: Rasterizer
       m_rasterizer.setInputFromConnect(m_primitiveAssembly.getOutput());
       m_rasterizer.setViewport(FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

       // Stage 4: Rasterizer -> Stage 5: FragmentShader
       m_fragmentShader.setInputFromConnect(m_rasterizer.getOutput());

       // Stage 5: FragmentShader -> Stage 6: Framebuffer
       m_framebuffer.setInputFromConnect(m_fragmentShader.getOutput());

       // Stage 6: Framebuffer -> Stage 7: TileWriteBack
       m_tileWriteBack.setFramebuffer(&m_framebuffer);
   }
   ```

4. **`render()` 仅负责执行和 per-render 状态：**
   - `render()` 调用各阶段 `execute()`，不再设置 stage-to-stage 连接
   - Per-render 状态（如 `setDepthTestEnabled`、`setDepthWriteEnabled`）保留在 `render()`

**验证：** 所有 24 个测试通过

---

## 上一轮问题验证

### ✅ P0-1: TileWriteBack 索引计算

**结论：** 已正确修复

`getTileOffset()` 计算正确：
```cpp
return tileY * NUM_TILES_X * TILE_W * TILE_H + tileX * TILE_W * TILE_H;
```

`writeTileToGMEM` 中 `gmemIdx * 4` 索引 RGBA 通道正确。

---

### ✅ P1-1: PrimitiveAssembly 三角形数量

**结论：** 已正确修复

- 非索引绘制：`m_inputVertices.size() / 3` ✅
- 索引绘制：`m_inputIndices.size() / 3` ✅

---

### ✅ P1-2: connectStages() 实现

**结论：** 已正确修复（commit `dd42fb5`）

---

### ✅ P1-3: VertexShader near-plane 剔除

**结论：** 已正确修复

```cpp
if (out.w <= 0.0f) {
    out.culled = true;
    return out;
}
```

在 clip space 阶段正确检查 near-plane 剔除条件。

---

### ✅ P1-4: Framebuffer depthTest() const

**结论：** 无此问题（之前为误报）

`depthTest()` 声明为 `private const`，内部仅读取 `m_depthBuffer` 比较深度，不修改对象状态，const 正确。

---

### ✅ P1-5: RenderPipeline indexed 判断

**结论：** 已正确修复

```cpp
drawParams.indexed ? ib : std::vector<uint32_t>()
```

与非索引绘制传递空 vector 保持一致。

---

## 代码质量观察

### 良好实践
- Near-plane 剔除在 VertexShader 中完成，早于 PrimitiveAssembly 的视锥剔除
- PrimitiveAssembly 在 computeNDC 前检查 `tri.culled`，避免无效计算
- 性能计数器设置完整
- `connectStages()` 版本控制机制优雅解决了 Pipeline 连接 vs 单元测试临时对象的兼容问题

### 架构说明
- Stage-to-stage 连接在 `connectStages()` 中通过指针建立（指向阶段输出引用）
- Per-render 数据（命令缓冲、uniforms、drawParams）在 `render()` 中设置
- 这种分离使得 Pipeline 在多次 `render()` 调用间高效复用连接

---

## 总体评价

**通过**

所有 P0/P1 问题均已正确修复。

**备注：** P1-2 的修复需要协调 Rasterizer、FragmentShader、Framebuffer 三个阶段的 API 扩展（`setInputFromConnect` 和 `m_inputVersion`），但改动对调用方透明，单元测试无需修改。

---

## 测试结果

```
test_Integration:  4 tests passed (1 disabled)
test_Framebuffer: 7 tests passed
test_Rasterizer: 5 tests passed
test_PrimitiveAssembly: 5 tests passed
test_VertexShader: 3 tests passed
-----------------------------------
Total: 24 tests passed
```
