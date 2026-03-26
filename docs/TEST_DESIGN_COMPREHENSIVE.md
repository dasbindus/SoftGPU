# SoftGPU 综合测试设计

## 1. 测试原则

### 1.1 测试用例正确性原则

| 原则 | 说明 |
|------|------|
| **自验证性** | 测试用例必须能自我验证正确性，不能依赖被测代码 |
| **确定性** | 相同输入必须产生相同结果 |
| **独立性** | 测试之间不能有依赖关系 |
| **可重复性** | 测试可以多次运行且结果一致 |

### 1.2 覆盖原则

| 层级 | 覆盖内容 |
|------|----------|
| **单元测试** | 每个 stage 的输入、输出、内部逻辑 |
| **集成测试** | 多个 stage 串联的正确性 |
| **E2E 测试** | 完整管线的 PPM 像素级验证 |

### 1.3 不破坏原则

- 新测试必须是**新增**的，不能修改现有测试
- 新测试不能改变被测代码的行为
- 所有现有测试必须继续通过

---

## 2. 管线阶段与测试点

```
RenderPipeline
├── CommandProcessor ────→ 验证 vertex buffer 解析
├── VertexShader ────────→ 验证 MVP 变换、NDC 输出
├── PrimitiveAssembly ───→ 验证图元组装、背面剔除
├── TilingStage ─────────→ 验证 tile binning 正确性
├── Rasterizer ──────────→ 验证 fragment 生成、属性插值
├── FragmentShader ───────→ 验证 fragment 着色
├── Framebuffer ─────────→ 验证 Z-test、颜色写入
└── TileWriteBack ───────→ 验证 GMEM ↔ LMEM 同步
```

---

## 3. 各阶段测试设计

### 3.1 CommandProcessor

**输入验证**：
- 已知数据的 vertex buffer
- 验证解析后的数据完整性

**测试用例**：
```
CommandProcessor_ParseVertexBuffer
  输入: vertices = {0,0,0,1, 1,0,0,1,  0,1,0,1, 0,1,0,1, ...}
  验证: getVertexBuffer() 返回的数据与输入一致
```

### 3.2 VertexShader

**输入验证**：
- 已知位置的顶点
- 验证 MVP 变换后的 NDC 坐标

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| w component | w > 0 时保留，w ≤ 0 时 culled | 缺少 w → culled |
| NDC X | (x/w) 正确 | w=0 → NDC 错误 |
| NDC Y | (y/w) 正确 | Y 轴反转 |
| Perspective divide | clip → NDC | 未执行除法 |

**测试用例**：
```
VertexShader_MVPTransformation
  输入: vertex at (1, 0, 0) with w=1
  预期输出: ndcX ≈ 1.0, ndcY ≈ 0.0
  
VertexShader_NearPlaneCull
  输入: vertex at (0, 0, -5) with w=-5
  预期: vertex.culled = true

VertexShader_PreserveW
  输入: vertex with w=2.0
  预期: ndcX = clipX / 2.0
```

### 3.3 PrimitiveAssembly

**输入验证**：
- 已知 NDC 坐标的顶点
- 验证组装后的三角形

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| 顶点顺序 | v0, v1, v2 顺序保留 | 顺序翻转 |
| NDC 计算 | (clipX/w, clipY/w, clipZ/w) | w 未参与 |
| 背面剔除 | 逆时针 vs 顺时针 | 错误剔除 |
| near-plane | w ≤ 0 三角形剔除 | 未剔除 |

**测试用例**：
```
PrimitiveAssembly_TriangleAssembly
  输入: 3 个顶点，cw 顺序
  预期: 三角形保留，culled=false

PrimitiveAssembly_BackFaceCulling
  输入: 逆时针排列的正面三角形
  预期: 保留
  输入: 顺时针排列的背面三角形
  预期: culled=true

PrimitiveAssembly_NearPlaneCull
  输入: 顶点 w 全部 ≤ 0 的三角形
  预期: triangle.culled = true
```

### 3.4 TilingStage

**输入验证**：
- 已知 NDC 坐标的三角形
- 验证分配到的 tile

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| Tile 计算 | NDC → tile index 正确 | tile 计算错误 |
| 包围盒 | 覆盖的 tile 范围正确 | min/max 交换 |
| Y 轴 | 正确 (1-ndcY)*0.5*H | Y 轴反转 |
| 空三角形 | 跳过 | 不应该分配 |

**测试用例**：
```
TilingStage_TriangleBinning
  输入: 三角形覆盖 NDC (-0.5, 0.5) 到 (0.5, -0.5)
  预期: 分配到 tile X=[5,15], Y=[3,11] 范围内
  
TilingStage_YAxisConversion
  输入: 顶点 NDC Y=0.5 (top)
  预期: screenY 较小 (≈120)
  输入: 顶点 NDC Y=-0.5 (bottom)
  预期: screenY 较大 (≈360)

TilingStage_EmptyTriangle
  输入: 被完全裁剪的三角形
  预期: 不分配到任何 tile
```

### 3.5 Rasterizer

**输入验证**：
- 已知 tile 坐标和三角形
- 验证生成的 fragment

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| Fragment 位置 | 在三角形内部 | 边缘计算错误 |
| 深度插值 | 正确透视校正 | 线性插值错误 |
| 属性插值 | 颜色正确插值 | 插值公式错误 |
| Tile 裁剪 | 只生成 tile 内 fragment | 越界生成 |

**测试用例**：
```
Rasterizer_FragmentPosition
  输入: 三角形顶点 (0,0), (1,0), (0,1)
  验证: 生成的 fragment 都在三角形内

Rasterizer_DepthInterpolation
  输入: 三角形三个顶点深度 0.1, 0.5, 1.0
  验证: 中心 fragment 深度 ≈ 0.53

Rasterizer_TileClipping
  输入: 跨越多个 tile 的三角形
  验证: 每个 tile 只生成该 tile 内的 fragment
```

### 3.6 FragmentShader

**输入验证**：
- 已知 fragment (位置、深度、属性)
- 验证着色结果

**测试用例**：
```
FragmentShader_ColorPassthrough
  输入: fragment color (1, 0, 0, 1)
  预期: 输出 color 不变
```

### 3.7 Framebuffer

**输入验证**：
- 已知 fragment
- 验证 color buffer 和 depth buffer

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| Z-test 通过 | 更浅的 fragment 写入 | Z-test 逻辑错误 |
| Z-test 失败 | 更深的 fragment 丢弃 | 未丢弃 |
| 颜色写入 | Z-test 通过时写入 | 错误颜色 |
| 深度写入 | Z-test 通过时写入 | 错误深度 |

**测试用例**：
```
Framebuffer_ZTestPass
  输入: 先写入 z=0.5 fragment，再写入 z=0.3 fragment
  预期: 最终 buffer 中 z=0.3 (更浅的通过)

Framebuffer_ZTestFail
  输入: 先写入 z=0.3 fragment，再写入 z=0.5 fragment
  预期: 最终 buffer 中 z=0.3 (更深的被拒绝)

Framebuffer_ColorWrite
  输入: fragment color (1, 0, 0, 1), z=0.5
  预期: color buffer 中 (1, 0, 0, 1), depth buffer 中 0.5
```

### 3.8 TileWriteBack

**输入验证**：
- 已知 TileBuffer 数据
- 验证 GMEM 中的数据

**关键验证点**：
| 检查项 | 预期 | 错误检测 |
|--------|------|----------|
| Load | GMEM → LMEM 数据一致 | 数据错误 |
| Store | LMEM → GMEM 数据一致 | 数据丢失 |
| Tile index | 正确的 tile 偏移 | 偏移计算错误 |

**测试用例**：
```
TileWriteBack_LoadStoreConsistency
  输入: TileBuffer with known color
  操作: storeToGMEM → loadFromGMEM
  预期: 加载的数据与原始数据一致

TileWriteBack_TileOffset
  输入: tileIndex = 50
  预期: GMEM offset = 50 * TILE_SIZE * 4
```

---

## 4. E2E 像素级验证测试

### 4.1 单三角形精确验证

**输入**：标准绿色三角形
**验证**：
1. 绿色像素总数 ≈ 38400
2. 每行宽度符合线性递变
3. 三角形方向正确（顶窄底宽）
4. Y 轴未反转

### 4.2 多场景回归测试

| 场景 | 验证点 |
|------|--------|
| Triangle-1Tri | 绿色三角形形状、位置 |
| Triangle-Cube | 多种颜色、可见面 |
| Triangle-Cubes-100 | 渲染时间、覆盖率 |
| Triangle-SponzaStyle | 结构完整性 |
| PBR-Material | 球体形状 |

---

## 5. 测试工具库

### 5.1 Vertex/Geometry 验证

```cpp
// 验证 VertexShader 输出
bool verifyNDC(Vertex& v, float expectedX, float expectedY, float tolerance) {
    return abs(v.ndcX - expectedX) < tolerance
        && abs(v.ndcY - expectedY) < tolerance;
}

// 验证三角形包含点
bool pointInTriangle(float px, float py, Triangle& tri) {
    // 使用边缘函数验证
}
```

### 5.2 PPM 分析工具

```cpp
struct PPMAnalyzer {
    // 加载 PPM
    static std::vector<uint8_t> load(const std::string& path, int& w, int& h);
    
    // 逐行扫描
    static RowScanResult scanRow(const std::vector<uint8_t>& pixels, int y, int width);
    
    // 验证三角形形状
    static ShapeVerification verifyTriangleShape(
        const std::vector<RowScanResult>& rows,
        int expectedMinY, int expectedMaxY);
    
    // 验证 Y 轴方向
    static bool isYAxisInverted(const std::vector<RowScanResult>& rows);
};
```

### 5.3 Stage 验证辅助

```cpp
// 获取 stage 输出进行验证
template<typename Stage>
void verifyStageOutput(Stage& stage, const Expected& expected) {
    const auto& output = stage.getOutput();
    // 验证输出数据
}
```

---

## 6. 测试实施计划

### Phase 1: 单元测试增强

| 优先级 | 测试 | 说明 |
|--------|------|------|
| P0 | VertexShader w component | 防止 w=0 导致 culled |
| P0 | TilingStage Y 轴转换 | 防止 Y 轴反转 |
| P0 | Framebuffer Z-test | 防止深度测试错误 |
| P1 | Rasterizer fragment 位置 | 防止光栅化错误 |

### Phase 2: 集成测试

| 优先级 | 测试 | 说明 |
|--------|------|------|
| P0 | CommandProcessor → VertexShader | 验证数据流 |
| P0 | TilingStage → Rasterizer | 验证 tile binning |
| P0 | Framebuffer → TileWriteBack | 验证 GMEM 同步 |

### Phase 3: E2E 像素级验证

| 优先级 | 测试 | 说明 |
|--------|------|------|
| P0 | PPM 三角形形状验证 | 每行扫描验证 |
| P0 | PPM Y 轴方向验证 | 最窄/最宽行位置 |
| P1 | 多场景回归测试 | 覆盖所有 TestScene |

---

## 7. 已知 Bug 与对应测试

| Bug | 根因 | 防范测试 |
|-----|------|----------|
| Y 轴反转 | `(ndcY+1)*0.5*H` | TilingStage_YAxisConversion |
| 缺少 w | vertex format 7 floats | VertexShader_PreserveW |
| dump() 未 sync | 未调用 syncGMEMToFramebuffer | Integration_GMEMSync |
| min/max 交换 | Y 轴反转后未交换 | TilingStage_TileBounds |

---

## 8. 测试不变量

以下不变量在所有测试中必须保持：

1. **VertexShader**: w > 0 的顶点不能被 culled
2. **TilingStage**: tile index 必须在 [0, 299] 范围内
3. **Rasterizer**: 生成的 fragment 必须在三角形内
4. **Framebuffer**: Z-test 通过时必须写入
5. **TileWriteBack**: GMEM load/store 数据一致
