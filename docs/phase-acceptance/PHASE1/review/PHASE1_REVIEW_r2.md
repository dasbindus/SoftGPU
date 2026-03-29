# SoftGPU PHASE1 审查报告（第2轮）

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

## 遗留问题验证

### ✅ P1-2: `connectStages()` 实现不完整

**结论：** 已正确修复（第1轮已确认，第2轮再次验证无回归）

**实现分析：**

`connectStages()` 当前实现完整覆盖 Stage 3→4→5→6→7 的数据流连接：

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

**7 个 Stage 连接方式说明：**

| Stage | 连接方式 | 说明 |
|-------|---------|------|
| Stage 1: CommandProcessor | N/A | 无输出到其他 stage，setCommand 由 render() 调用 |
| Stage 2: VertexShader | Per-render | 输入依赖命令数据（vb/ib/uniforms），render() 中 setInput |
| Stage 3: PrimitiveAssembly | Per-render | 输入依赖命令数据（index buffer / indexed flag），render() 中 setInput |
| Stage 4: Rasterizer | connectStages() | setInputFromConnect(PrimitiveAssembly.getOutput()) ✅ |
| Stage 5: FragmentShader | connectStages() | setInputFromConnect(Rasterizer.getOutput()) ✅ |
| Stage 6: Framebuffer | connectStages() | setInputFromConnect(FragmentShader.getOutput()) ✅ |
| Stage 7: TileWriteBack | connectStages() | setFramebuffer(&m_framebuffer) ✅ |

**关键设计合理性：**
- Stage 2 (VertexShader) 和 Stage 3 (PrimitiveAssembly) 的输入依赖命令数据（vertex buffer、index buffer、uniforms），必须在 `render()` 中根据每帧命令设置，而非在 `connectStages()` 中建立静态连接
- Stage 4-7 的输出类型（Triangle/Fragment）不依赖命令数据，可在构造函数中通过 `connectStages()` 预连接
- 版本控制机制（`m_inputVersion`）确保 Pipeline 连接（指针引用）与单元测试（临时 initializer_list 复制）兼容

---

## 新问题检查

### 未引入新问题

- 所有 Stage 的 `setInputFromConnect()` / `getOutput()` / `setFramebuffer()` 方法签名正确
- Pipeline 构造函数正确调用 `connectStages()`
- `render()` 中各 Stage 的执行顺序与数据流一致
- 所有 7 个 Stage 的性能计数器在 `printPerformanceReport()` 中正确打印

---

## 测试验证

所有测试通过（24 tests, 1 disabled）：

```
test_Integration:        4 tests passed (1 disabled)
test_Framebuffer:        7 tests passed
test_Rasterizer:         5 tests passed
test_PrimitiveAssembly:  5 tests passed
test_VertexShader:       3 tests passed
-----------------------------------
Total:                   24 tests passed (1 expected disabled)
```

**集成测试关键指标验证：**
- Triangle (3 vertices) → VertexShader (inv=3) → PrimitiveAssembly (inv=1) → Rasterizer (fragments=38400) → FragmentShader (inv=38400) → Framebuffer (writes=38400) → TileWriteBack (tiles=300)
- 完整数据流通过，性能计数器数值合理

---

## 代码质量观察

- 连接逻辑与关注点分离清晰：静态连接在 `connectStages()`，动态 per-render 状态在 `render()`
- 注释清晰标注了每个 Stage 的连接方式和数据来源
- `setViewport` 在 `connectStages()` 中调用，确保 Rasterizer viewport 在整个 Pipeline 生命周期内保持一致

---

## 总体评价

**通过**

所有 P0/P1 问题已修复，无新问题引入，测试全部通过。Pipeline 架构设计合理。

---

## 备注

- Platform 层（Window.cpp）存在编译错误（GLFW API 版本不匹配），与 Pipeline 实现无关，不影响 PHASE1 验收
- 如后续需修复 Platform 编译问题，建议单独处理
