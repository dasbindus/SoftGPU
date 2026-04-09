# SoftGPU VS ISA Integration Design

**版本：** 1.0  
**作者：** 陈二虎（Architect Agent）  
**日期：** 2026-04-09  
**状态：** 正式版

---

## 1. 背景与目标

### 1.1 当前状态

VS ISA（32条指令）已在 `Interpreter` 里完整实现并通过 68 tests PASS。当前架构：

```
VS_ISA_Program → Interpreter → m_voutput_buf (float[]) 
                                      ↓
                              缺转换层
                                      ↓
                    PrimitiveAssembly::setInput() ← 期望 Vertex{pos+color}
```

- `VS_VOUTPUT` 仅输出 clip position (x,y,z,w) 到 `m_voutput_buf`
- `PrimitiveAssembly::setInput()` 期望 `std::vector<Vertex>` (含 pos + color)
- VS_ATTR stub 返回 (0,0,0,1)，无实际功能

### 1.2 设计目标

1. **VOUTPUT buffer → Vertex 转换层**：将 ISA 输出转换为 PA 可消费格式
2. **双路径执行**：`VertexShader::execute()` 支持 C++ 路径和 ISA 路径切换
3. **ATTR 正式化**：定义 ATTR 行为，实现 VBO 属性读取
4. **Uniforms 注入**：MVP 矩阵到寄存器映射
5. **与 FS ISA 共存一致性**：复用统一解码框架

---

## 2. 架构设计

### 2.1 数据流全景

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ISA Vertex Shader 路径                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  DrawCommand                                                             │
│    ├─ vertex_buffer[] (float[], layout=VertexLayout)                   │
│    ├─ index_buffer[] (uint32_t[])                                        │
│    ├─ uniforms (model/view/projection matrices)                         │
│    └─ vs_program (ISA bytecode)                                          │
│                                                                          │
│  VertexShader::execute() [ISA mode]                                      │
│    │                                                                     │
│    ├─ [A] Load uniforms into registers:                                 │
│    │     M_MAT → R8..R23 (16 regs, column-major 4×4)                   │
│    │     V_MAT → R24..R39 (16 regs)                                    │
│    │     P_MAT → R40..R55 (16 regs)                                    │
│    │     viewport → R56, R57 (width, height)                           │
│    │                                                                     │
│    ├─ [B] For each vertex i:                                            │
│    │     │                                                               │
│    │     ├─ Reset interpreter (reg_file_.Reset, pc_=0)                 │
│    │     ├─ SetVBO(vertex_buffer)                                       │
│    │     ├─ Run VS program:                                             │
│    │     │   VLOAD R0, #0        ; load pos from VBO                   │
│    │     │   ...                                                           │
│    │     │   MAT_MUL ...         ; MVP transform                         │
│    │     │   ATTR  R4, #16       ; load color attr (VS_ATTR)            │
│    │     │   VOUTPUT R_c, #0    ; output clip pos                       │
│    │     │   (color implicitly follows in attr buffer)                 │
│    │     └─ Collect: m_voutput_buf + m_vattr_buf                        │
│    │                                                                     │
│    └─ [C] VSOutputAssembler: flat buffers → vector<Vertex>             │
│          ├─ For each vertex i:                                          │
│          │   Vertex v;                                                  │
│          │   v.x = m_voutput_buf[i*4 + 0]; v.y = ...; v.z = ...; v.w = ... │
│          │   v.r = m_vattr_buf[i*4 + 0]; v.g = ...; v.b = ...; v.a = ...│
│          │   v.ndcX = v.ndcY = v.ndcZ = 0.0f; v.culled = false;         │
│          │   output.push_back(v);                                       │
│          └─ → std::vector<Vertex> → PrimitiveAssembly::setInput()       │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                        C++ Vertex Shader 路径（调试用）                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  VertexShader::execute() [C++ mode]                                      │
│    ├─ for each vertex:                                                  │
│    │   Vertex v = transformVertex(rawVertex);  // C++ MVP              │
│    │   output.push_back(v);                                             │
│    └─ → std::vector<Vertex> → PrimitiveAssembly::setInput()           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 转换层：VSOutputAssembler

```cpp
// 新文件：src/stages/VSOutputAssembler.hpp
class VSOutputAssembler {
public:
    // 输入：VOUTPUT buffer (clip position, 4 floats per vertex)
    // 输入：VSATTR buffer (vertex color/attrs, 4 floats per vertex)
    // 输入：vertex count
    // 输出：std::vector<Vertex>
    
    std::vector<Vertex> assemble(
        const float* voutput_buf,   // clip position: [vx0,vy0,vz0,vw0, vx1,...]
        const float* vattr_buf,     // attributes:    [ar0,ag0,ab0,aa0, ar1,...]
        size_t vertex_count
    );
    
    // 设置顶点属性布局（VBO stride, color offset, etc.）
    void setVertexLayout(const VertexLayout& layout);
    
private:
    VertexLayout m_layout;
};
```

**行为**：
- 从 `voutput_buf` 读取 (x,y,z,w) 作为 clip position
- 从 `vattr_buf` 读取 (r,g,b,a) 作为 color
- NDC 坐标在 PrimitiveAssembly 透视除法后计算（不在此阶段）
- culled flag 初始化为 false（near-plane culling 在 VS 执行时检查）

### 2.3 VertexLayout 定义

```cpp
struct VertexLayout {
    size_t stride_bytes = 32;          // VBO stride: x,y,z,w,r,g,b,a = 8 floats = 32 bytes
    size_t position_offset = 0;        // clip pos offset in VBO (bytes)
    size_t color_offset = 16;          // color offset in VBO (bytes)
    size_t normal_offset = 0;          // future: normal offset
    size_t uv_offset = 0;              // future: uv offset
};
```

---

## 3. VS_ATTR 正式规格

### 3.1 指令定义

| 字段 | 值 |
|------|-----|
| Opcode | 0x48 |
| 类型 | I-type |
| 操作数 | `VS_ATTR Rd, #attr_id` |
| 执行周期 | 2（EU_MEM） |
| 执行单元 | EU_MEM |

**操作数约束**：
- Rd 必须是 4-aligned（Rd % 4 == 0）
- attr_id 是属性标识符（0=position, 1=normal, 2=color, 3=uv, 4+=custom）

**功能**：从当前顶点的 VBO 属性槽中加载指定属性到 Rd..Rd+3。

```
VS_ATTR(Rd, attr_id):
    byte_offset = ATTR_TABLE[attr_id]   // 查表获取属性在 VBO 内的字节偏移
    for i in 0..3:
        addr = VBO_base + vertex_byte_offset + byte_offset + i * 4
        value = memory_.Load32(addr)
        reg_file_.Write(rd + i, value)
    stats_.loads++
    pc_.addr += 4
```

> **attr_id 查表**：attr_id 作为索引查询硬编码的 ATTR_TABLE，将 attr_id 转换为字节偏移量。此设计允许 VBO 任意布局而无需修改指令格式。

**ATTR_TABLE 布局（可配置）**：

| attr_id | 含义 | 字节偏移（默认） |
|---------|------|----------------|
| 0 | Position (x,y,z,w) | 0 |
| 1 | Normal (nx,ny,nz,nw) | 16 |
| 2 | Color (r,g,b,a) | 16 |
| 3 | UV (u,v) | 32 |

> **实现说明**：ATTR 使用 EU_MEM 执行单元，与 VLOAD 共用内存端口。VS_ATTR 在 vertex program 中的典型位置是 MVP 变换后（或同时）加载颜色属性，等价于 FS 中的 varying 插值请求语义。

### 3.2 ATTR 与 VLOAD 的分工

| 指令 | 使用场景 | 数据源 | 语义 |
|------|---------|--------|------|
| VLOAD | 加载顶点坐标（position）| VBO | 必需，在 VS 开始时调用 |
| ATTR | 加载顶点属性（color/normal/uv）| VBO | 可选，用于插值属性传递到 PA |

> **设计理由**：VS_ATTR 独立于 VLOAD，因为两者读取 VBO 的不同 offset。VLOAD 读取 position（VS 坐标变换输入），ATTR 读取 color/UV（透传到 PA/Rasterizer 的插值属性）。VOUTPUT 仅输出 clip position，不携带属性；属性通过 ATTR 独立通道传递给 PA。

### 3.3 ATTR 在 shader 程序中的典型用法

```
; VS program: MVP transform + color passthrough
.entry:
    VLOAD   R0, #0              ; R0..R3 = position (x,y,z,w) from VBO offset 0
    MAT_MUL R4, M_MAT, R0        ; R4 = M * pos
    MAT_MUL R5, V_MAT, R4        ; R5 = V * M * pos
    MAT_MUL R6, P_MAT, R5        ; R6 = P * V * M * pos (clip)
    ATTR    R8, #2               ; R8..R11 = color (r,g,b,a) from VBO offset 16
    VOUTPUT R6, #0               ; output clip position
    HALT
```

**执行结果**：
- clip position → `m_voutput_buf[i*4..i*4+3]`
- color → `m_vattr_buf[i*4..i*4+3]`（由 ATTR 写入专用属性 buffer）
- VSOutputAssembler 组合两者生成 `Vertex{x,y,z,w,r,g,b,a}`

### 3.4 ATTR buffer 实现

Interpreter 新增成员：

```cpp
// VS: Vertex attribute buffer (ATTR outputs) - 256 bytes = 64 floats = 16 vertices × 4 components
static constexpr size_t VATTR_BUF_SIZE = MAX_VERTICES * 4;  // 64 floats
std::vector<float> m_vattr_buf;

// 新增方法：
const float* GetVAttrBufData() const { return m_vattr_buf.data(); }
float GetVAttrFloat(int vertex_idx, int attr_offset) const;
```

---

## 4. Uniforms 注入方案

### 4.1 寄存器分配（64 个 R0-R63）

| 寄存器区间 | 用途 | 数量 | 描述 |
|-----------|------|------|------|
| R0 | Zero Register | 1 | 硬件级硬连线为 0.0f |
| R1–R7 | 临时寄存器 | 7 | VS 程序通用临时 |
| R8–R23 | Model Matrix | 16 (4×4) | M_MAT，column-major |
| R24–R39 | View Matrix | 16 (4×4) | V_MAT，column-major |
| R40–R55 | Projection Matrix | 16 (4×4) | P_MAT，column-major |
| R56–R57 | Viewport | 2 | width, height |
| R58–R63 | 通用临时 | 6 | VS 程序临时计算 |

**约束**：
- Model/View/Projection 矩阵固定占用 R8–R57（编译器不得使用）
- 超出此范围的矩阵需求（如 bone matrices）需通过 VLOAD 从 constant buffer 加载

### 4.2 Uniforms 注入 API

```cpp
// VertexShader.hpp 新增
void setUniforms(const Uniforms& uniforms);

// 内部实现：加载矩阵到寄存器
void loadUniformsToRegisters(Interpreter& interp, const Uniforms& uniforms) {
    // M_MAT → R8..R23
    for (int i = 0; i < 16; ++i) {
        interp.SetRegister(8 + i, uniforms.modelMatrix[i]);
    }
    // V_MAT → R24..R39
    for (int i = 0; i < 16; ++i) {
        interp.SetRegister(24 + i, uniforms.viewMatrix[i]);
    }
    // P_MAT → R40..R55
    for (int i = 0; i < 16; ++i) {
        interp.SetRegister(40 + i, uniforms.projectionMatrix[i]);
    }
    // Viewport → R56, R57
    interp.SetRegister(56, uniforms.viewportWidth);
    interp.SetRegister(57, uniforms.viewportHeight);
}
```

---

## 5. VertexShader::execute() 双路径设计

### 5.1 执行模式枚举

```cpp
enum class VSExecutionMode {
    Auto,   // 优先 ISA，无 ISA program 时 fallback C++
    ISA,    // 强制 ISA 路径
    CPP     // 强制 C++ 路径（调试用）
};
```

### 5.2 VertexShader 新增接口

```cpp
class VertexShader {
public:
    // 设置 VS 程序（ISA bytecode）
    void setProgram(const std::vector<uint32_t>& isa_code);
    bool hasProgram() const { return !m_vsProgram.empty(); }
    
    // 执行模式
    void setExecutionMode(VSExecutionMode mode);
    VSExecutionMode getExecutionMode() const { return m_execMode; }
    
    // ATTR 布局配置
    void setAttrLayout(const std::vector<size_t>& attr_byte_offsets);
    
    // 执行（双路径）
    void execute() override;

private:
    // ISA 路径
    void executeISA();
    void executeCPPRef();
    
    VSExecutionMode m_execMode = VSExecutionMode::Auto;
    std::vector<uint32_t> m_vsProgram;  // ISA bytecode
    
    // ATTR table（attr_id → VBO byte offset）
    std::vector<size_t> m_attrTable;    // 大小=ATTR_COUNT, 默认 {0,16,16,32}
    
    // VS output assembler
    VSOutputAssembler m_assembler;
    
    // Interpreter for ISA execution
    Interpreter m_interpreter;
};
```

### 5.3 execute() 路由逻辑

```cpp
void VertexShader::execute() {
    switch (m_execMode) {
    case VSExecutionMode::ISA:
        executeISA();
        break;
    case VSExecutionMode::CPP:
        executeCPPRef();
        break;
    case VSExecutionMode::Auto:
        if (hasProgram()) {
            executeISA();
        } else {
            executeCPPRef();
        }
        break;
    }
}
```

### 5.4 executeISA() 详细实现

```cpp
void VertexShader::executeISA() {
    auto start = std::chrono::high_resolution_clock::now();
    m_counters.invocation_count += m_vertexCount;
    
    m_outputVertices.clear();
    m_outputVertices.reserve(m_vertexCount);
    
    // Load uniforms to interpreter registers
    loadUniformsToRegisters(m_interpreter, m_uniforms);
    
    for (size_t i = 0; i < m_vertexCount; ++i) {
        // Reset VS interpreter state (per-vertex)
        m_interpreter.Reset();
        m_interpreter.ResetVS();  // clear VOUTPUT buf, VATTR buf, vertex count
        
        // Inject VBO (per-vertex: set base pointer)
        m_interpreter.SetVBO(m_vertexBuffer.data() + i * VERTEX_STRIDE, 
                              m_vertexBuffer.size() - i * VERTEX_STRIDE);
        
        // Load VS program
        m_interpreter.LoadProgram(m_vsProgram.data(), m_vsProgram.size());
        
        // Execute until HALT
        m_interpreter.Run(100000);
        
        // Collect output
        Vertex v;
        v.x = m_interpreter.GetVOutputFloat(i, 0);
        v.y = m_interpreter.GetVOutputFloat(i, 1);
        v.z = m_interpreter.GetVOutputFloat(i, 2);
        v.w = m_interpreter.GetVOutputFloat(i, 3);
        v.r = m_interpreter.GetVAttrFloat(i, 0);
        v.g = m_interpreter.GetVAttrFloat(i, 1);
        v.b = m_interpreter.GetVAttrFloat(i, 2);
        v.a = m_interpreter.GetVAttrFloat(i, 3);
        v.ndcX = v.ndcY = v.ndcZ = 0.0f;
        v.culled = (v.w <= 0.0f);  // near-plane culling check
        
        m_outputVertices.push_back(v);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    m_counters.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
}
```

### 5.5 executeCPPRef() 保持现有实现

现有 `transformVertex()` 实现保持不变，供 C++ 路径使用：

```cpp
void VertexShader::executeCPPRef() {
    m_outputVertices.clear();
    m_outputVertices.reserve(m_vertexCount);
    for (size_t i = 0; i < m_vertexCount; ++i) {
        const float* raw = m_vertexBuffer.data() + i * VERTEX_STRIDE;
        Vertex transformed = transformVertex(raw);  // 现有 C++ MVP
        m_outputVertices.push_back(transformed);
    }
}
```

---

## 6. ATTR 实现（Interpreter.cpp）

在 `ExecuteInstruction()` 的 VS I-type 分支添加：

```cpp
case Opcode::VS_ATTR: {
    // VS_ATTR Rd, #attr_id: Load vertex attribute into Rd..Rd+3
    uint16_t attr_id = inst.GetImm();
    uint8_t rd = inst.GetRd();
    
    // Get byte offset from attr table (indexed by attr_id)
    size_t byte_offset = (attr_id < m_attrTable.size()) 
        ? m_attrTable[attr_id] 
        : 0;
    
    // Calculate actual VBO address: VBO_base + vertex_byte_offset + byte_offset
    // vertex_byte_offset = current_vertex_index * VERTEX_STRIDE * sizeof(float)
    // NOTE: VERTEX_STRIDE is in bytes; addr here is float-index into m_vbo_data[float_idx]
    size_t vertex_byte_offset = m_vertex_count * VERTEX_STRIDE * sizeof(float);
    size_t addr = vertex_byte_offset + byte_offset;
    
    // Load 4 floats
    for (int i = 0; i < 4; ++i) {
        float val = 0.0f;
        size_t float_idx = addr / 4 + i;
        if (float_idx < m_vbo_count) {
            val = m_vbo_data[float_idx];
        }
        reg_file_.Write(rd + i, val);
    }
    
    // Write to VATTR buffer (for later assembly)
    size_t attr_base = m_vertex_count * 4;
    if (attr_base + 3 < m_vattr_buf.size()) {
        for (int i = 0; i < 4; ++i) {
            m_vattr_buf[attr_base + i] = reg_file_.Read(rd + i);
        }
    }
    
    stats_.loads++;
    pc_.addr += 4;
    break;
}
```

**Interpreter.hpp 新增成员**：

```cpp
// ATTR table (attr_id → VBO byte offset)
std::vector<size_t> m_attrTable;

// VS: Vertex attribute buffer
std::vector<float> m_vattr_buf;

// Methods
void SetAttrTable(const std::vector<size_t>& table);
float GetVAttrFloat(int vertex_idx, int attr_offset) const;
```

---

## 7. 数据流汇总

### 7.1 ISA 路径完整数据流

```
DrawCommand
  │
  ├─ uniforms (MVP matrices)
  │       │
  │       └─→ VertexShader::loadUniformsToRegisters()
  │                    │
  │                    ├─ M_MAT → R8..R23
  │                    ├─ V_MAT → R24..R39
  │                    └─ P_MAT → R40..R55
  │
  ├─ vertex_buffer (float[], 8 floats/vertex)
  │       │
  │       └─→ Interpreter::SetVBO() [per vertex]
  │
  ├─ index_buffer
  │
  └─ vs_program (ISA bytecode)
            │
            └─→ Interpreter::LoadProgram()
                           │
                           ├─ VLOAD R0, #0          ; read from VBO
                           ├─ MAT_MUL / ADD / ...    ; compute
                           ├─ ATTR R4, #2            ; read color from VBO
                           ├─ VOUTPUT R6, #0         ; write clip pos
                           └─ HALT                   ; end
                                    │
                                    ├─→ m_voutput_buf (clip pos)
                                    └─→ m_vattr_buf (color)
                                              │
                                              └─→ VSOutputAssembler::assemble()
                                                           │
                                                           └─→ std::vector<Vertex>
                                                                       │
                                                                       └─→ PrimitiveAssembly::setInput()
```

### 7.2 与 PrimitiveAssembly 的接口

```cpp
// VertexShader 输出接口不变
const std::vector<Vertex>& VertexShader::getOutput() const { 
    return m_outputVertices; 
}

// PA 消费方式不变
m_primitiveAssembly.setInput(
    m_vertexShader.getOutput(),   // std::vector<Vertex>
    drawParams.indexed ? ib : std::vector<uint32_t>(),
    drawParams.indexed
);
```

---

## 8. 与 FS ISA 共存设计

### 8.1 共存原则

- VS 和 FS 共享同一 `Interpreter` 类（不同的 `Interpreter` 实例）
- VS 的 `Interpreter` 处理顶点程序，FS 的处理片元程序
- 两者的寄存器文件独立（各自 64 个 R0-R63）
- 数据通过 `VOUTPUTBUF` 和 `Framebuffer` 传递，不经寄存器

### 8.2 ATTR 与 FS TEX 的对称性

| VS 指令 | 语义 | FS 对应指令 | 语义 |
|--------|------|-----------|------|
| VLOAD | 从 VBO 加载顶点数据 | LD | 从任意地址加载 |
| ATTR | 从 VBO 加载插值属性 | TEX/SAMPLE | 从纹理采样插值 |
| VOUTPUT | 输出到光栅化器 | FRAMEBUF_WRITE | 输出到帧缓冲 |

**设计一致性**：
- ATTR 使用 EU_MEM（与 VLOAD/TEX 共用内存端口）
- ATTR 的 attr_id 查表机制与 TEX 的 texture_id 选择器形成对称设计

---

## 9. 测试验证计划

### 9.1 VSOutputAssembler 测试

```
test_VSOutputAssembler:
  - 输入：voutput_buf=[1,2,3,4, 5,6,7,8], vattr_buf=[0.1,0.2,0.3,0.4, 0.5,0.6,0.7,0.8], count=2
  - 预期输出：2个Vertex，pos/attr 正确映射
```

### 9.2 ATTR 功能测试

```
test_VS_ATTR:
  - VBO: [x=1,y=2,z=3,w=4, r=0.1,g=0.2,b=0.3,a=1.0, ...]
  - VS program: VLOAD R0,#0; ATTR R4,#2; VOUTPUT R0,#0; HALT
  - 预期：m_vattr_buf[0..3] = {0.1, 0.2, 0.3, 1.0}
```

### 9.3 ISA vs C++ 一致性测试

```
test_VS_consistency:
  - 相同 MVP 矩阵 + 相同 vertex buffer
  - 分别用 ISA 路径和 C++ 路径执行
  - 预期：输出 Vertex 的 x,y,z,w,r,g,b,a 完全一致（误差 < 1e-6）
```

### 9.4 端到端渲染测试

```
test_VS_ISA_render:
  - 使用 VS ISA program 渲染场景
  - 对比 C++ 路径的参考渲染结果
  - 预期：PPM 输出像素级一致
```

---

## 10. 文件变更清单

| 文件 | 操作 | 描述 |
|------|------|------|
| `src/stages/VertexShader.hpp` | 修改 | 新增 m_vsProgram, m_execMode, m_attrTable, m_interpreter, m_assembler |
| `src/stages/VertexShader.cpp` | 修改 | 实现 executeISA()（ISA 路径）、executeCPPRef()（C++ fallback）、loadUniformsToRegisters()（uniform 注入）、SetAttrTable()（ATTR 布局配置） |
| `src/isa/Interpreter.hpp` | 修改 | 新增 m_vattr_buf, m_attrTable, GetVAttrFloat(), SetAttrTable() |
| `src/isa/Interpreter.cpp` | 修改 | 实现 VS_ATTR 执行逻辑 |
| `src/stages/VSOutputAssembler.hpp` | 新增 | 转换层类定义 |
| `src/stages/VSOutputAssembler.cpp` | 新增 | 转换层实现 |
| `src/core/PipelineTypes.hpp` | 修改 | VertexLayout struct |
| `tests/stages/test_VertexShader.cpp` | 修改 | 新增 ISA 路径测试用例 |

---

## 11. 实现优先级

**Phase 1（必须）**：
1. Interpreter 新增 `m_vattr_buf` 和 `VS_ATTR` 实现
2. `VSOutputAssembler` 实现
3. `VertexShader::executeISA()` 基本框架
4. ATTR 布局表配置

**Phase 2（完善）**：
1. 双路径切换机制（Auto/ISA/CPP 模式）
2. ISA vs C++ 一致性验证测试
3. E2E 渲染测试

---

*本文档为正式架构设计，所有设计决策已明确，无"待定"项。*
