# SoftGPU Vertex Shader ISA Design Specification

**版本：** 1.0  
**作者：** 陈二虎（SoftGPU Architect Agent）  
**日期：** 2026-04-09  
**状态：** 正式版 v1.5  

---

## 第1章：概述与设计目标

### 1.1 设计背景

Vertex Shader ISA 是 SoftGPU 可编程渲染管线的第一阶段指令集，负责将顶点数据从对象空间（Object Space）经由模型空间、世界空间变换至裁剪空间（Clip Space），是 GPU 可编程管线的起点。

SoftGPU 采用统一解码框架，VS（Vertex Shader）指令与 FS（Fragment Shader）指令共享同一套 32-bit 固定长度指令格式，通过 **Opcode[5] = 1** 区分：VS 指令 opcode 在 0x30–0x5F（bit5=1），FS 指令 opcode 在 0x00–0x2F（bit5=0）。

### 1.2 设计目标

- **统一解码**：VS/FS 共用同一解码器，opcode bit5 决定目标执行单元
- **高吞吐矩阵运算**：MAT_MUL 专用 4×4 矩阵乘法流水线，4-cycle latency
- **顺序顶点加载**：VLOAD 专用于从 Vertex Buffer 顺序加载顶点属性
- **确定性周期计费**：每条指令周期数固定，适合 CPU 仿真解释执行
- **可扩展**：Phase 1 实现 12 条核心指令，Phase 2 扩展至 32 条

### 1.3 VS/FS 统一执行框架

```
Opcode[5] = 0 → FS 指令 → EU_ALU / EU_SFU / EU_MEM / EU_TEX
Opcode[5] = 1 → VS 指令 → EU_VTX / EU_MEM
```

所有指令均为 32-bit 固定长度，共用以下位域布局：

```
 31    25 24    20 19    15 14    10 9              0
+--------+-------+-------+-------+------------------+
| Opcode |  Rd   |  Ra   |  Rb   |   Immediate     |
| 7 bit  | 5 bit | 5 bit | 5 bit |     10 bit      |
+--------+-------+-------+-------+------------------+
```

VS 指令实际 opcode = 内部编码值（0x30 起始），硬件/解释器通过 `opcode[5]` 自动识别目标管線。

**两级解码逻辑**：解码器采用两级分流策略：
1. **精确匹配优先**：opcode 为 0x26（VOUTPUT）、0x28（MAT_MUL）、0x29（VLOAD）、0x2A（HALT）时，**无论 bit5 值如何**，均路由至 VS Interpreter（这四个是 VS 特殊扩展指令，在 FS opcode 空间无对应物）
2. **bit5 路由**：其余指令按 opcode[5] 分流：bit5=1 → VS Interpreter，bit5=0 → FS Interpreter

> **路由矛盾说明**：0x26/0x28/0x29/0x2A 的 bit5 均为 0，按简单 bit5 路由会错误落入 FS 分支，故需精确匹配优先于 bit5 路由。

### 1.4 与 Fragment Shader ISA 的分工

| 维度 | Vertex Shader ISA | Fragment Shader ISA |
|------|-------------------|---------------------|
| 指令 opcode 区间 | 0x30–0x5F（bit5=1）| 0x00–0x2F（bit5=0）|
| 执行单元 | EU_VTX, EU_MEM | EU_ALU, EU_SFU, EU_MEM, EU_TEX |
| 数据源 | Vertex Buffer（VBO）| Rasterizer（插值输出）|
| 输出目标 | Rasterizer（VOUTPUTBUF）| Framebuffer（FB_COLOR/FB_DEPTH）|
| 终结指令 | VOUTPUT（必须）或 HALT | 指令流结束自然终止 |
| 典型计算密度 | 低（矩阵运算为主）| 高（逐像素插值+光照）|
| 纹理采样 | 可选（顶点纹理提取）| 主要操作 |
| 条件分支 | 较少 | 频繁 |

---

## 第2章：指令格式

Vertex Shader ISA 与 Fragment Shader ISA 使用完全相同的 32-bit 指令格式，通过 opcode 的 bit 5 区分目标执行管線（bit5=1 为 VS，bit5=0 为 FS）。指令格式类型如下：

### 2.1 R-type — 三寄存器指令

```
Rd, Ra, Rb
```

格式：`[Opcode(7)] [Rd(5)] [Ra(5)] [Rb(5)] [0000000000(10)]`

适用指令：ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX, DOT3, DOT4, SHL, SHR 等。

### 2.2 R4-type — 四寄存器指令（含隐含第三操作数）

```
Rd, Ra, Rb, Rc
```

Ra/Rb/Rc 均为寄存器编号，Rc 编码在 immediate field 的高5位 `[9:5]`。

适用指令：MAD, MAT_MUL, SMOOTHSTEP 等。

### 2.3 U-type — 单寄存器 + 立即数指令

```
Rd, Ra [, #imm]
```

适用指令：MOV, MOV_IMM, SQRT, RSQ, ABS, NEG, FLOOR, CEIL, CVT_* 等。

### 2.4 I-type — 两寄存器 + 偏移立即数（内存访问）

```
Rd, [Ra + #imm]    ; 通用形式
Rd, #byte_offset   ; VLOAD 专用形式（Ra 隐含为 R0，等价于 Rd, [R0 + #imm]）
```

适用指令：VLOAD, VSTORE, LD, ST。

> **VLOAD 特殊约定**：VLOAD 的 Ra 字段**硬编码为 R0**（zero register），指令编码中的 Ra 字段被忽略，实际寻址为 `VBO_base + byte_offset`。这是因为 VLOAD专用于顺序访问顶点缓冲区的流式加载场景，无需通用基址寄存器。
>
> **VOUTPUT 扩展 I-type**：`VOUTPUT Rd, #offset` 中 offset 字段为顶点输出缓冲区内的属性布局偏移量（单位：字节），用于指定当前顶点属性在 VOUTPUTBUF 内的写入位置（非地址偏移）。

### 2.5 B-type — 条件分支

```
CBR  Ra, #signed_offset   ; VS 专用：Ra 为条件寄存器，Ra ≠ 0 时跳转
```

Ra 为条件寄存器（语义与 FS BRA 中的 Ra 一致：Ra ≠ 0 则跳转，Ra == 0 则 fall-through），offset 为符号扩展的 10-bit 立即数（单位：4字节）。

适用指令：CBR（条件分支）。

> **设计说明**：VS CBR 与 FS BRA 使用完全相同的 B-type 格式，Ra 均为条件寄存器字段。这确保了 VS/FS 统一解码器可共用同一分支解析逻辑，仅在执行单元（EU_VTX vs EU_ALU）上分流。

### 2.6 J-type — 无条件跳转 / 程序终结

```
JMP  #signed_offset    ; PC ← PC + offset × 4（立即数寻址）
NOP                    ; 空操作
HALT                   ; 终止程序执行
```

适用指令：NOP, HALT, JUMP。

> **JUMP 操作数说明**：JUMP 指令的操作数是 `#signed_offset`（符号扩展立即数，编码在 immediate field），不是显式目标地址。PC 跳转计算为 `PC + offset × 4`。这与通用 J-type 格式的"target"描述不冲突——target 在本 ISA 中**始终是相对偏移量**，而非绝对地址。

---

## 第3章：寄存器文件

### 3.1 Register File 规格

| 参数 | 值 |
|------|-----|
| 寄存器数量 | 64 个标量寄存器 |
| 寄存器宽度 | 32-bit float（IEEE 754 single precision）|
| 寄存器编号 | R0 – R63 |
| 特殊寄存器 | R0 恒为 0.0f（zero register，硬件级硬连线）|
| 寻址方式 | 5-bit 直接寻址 |

### 3.2 行为约束

- **R0 为只读 0.0f**：无论写入什么值，Read(R0) 始终返回 0.0f
- **越界访问**：未定义，编译器负责保证不越界
- **数据类型**：所有寄存器均为 float32 bit pattern，整数操作通过 `reinterpret_cast<uint32_t&>` 实现

### 3.3 特殊寻址约定

**DOT3 指令**：`DOT3`（opcode 0x40）要求 Ra 和 Rb 为 **4-aligned**（编号能被 4 整除），将 Ra, Ra+1, Ra+2 作为 xyz 三分量向量，Rb, Rb+1, Rb+2 作为另一向量。

**NORMALIZE 指令**：`NORMALIZE`（opcode 0x44）要求 Ra 为 4-aligned，读取 Ra, Ra+1, Ra+2 作为输入向量，结果写入 Rd。

**MAT_MUL 指令**：`MAT_MUL` 矩阵寄存器跨 4 个连续 VREG 槽位（16 个 float32），向量形式占用 Rd, Rd+1, Rd+2, Rd+3。

---

## 第4章：Vertex Shader 指令集详细规格（Phase 1 — 12条）

### 4.1 控制流指令

#### NOP — 空操作

| 字段 | 值 |
|------|-----|
| Opcode | 0x30 |
| 类型 | J-type |
| 操作数 | 无 |
| 执行周期 | 1 |
| 执行单元 | EU_VTX |

**操作数约束**：无。

**功能**：空操作，不修改任何寄存器或 PC 之外的任何状态。

```
NOP:
    pc_.addr += 4
```

---

#### HALT — 终止程序执行

| 字段 | 值 |
|------|-----|
| Opcode | 0x2A |
| 类型 | J-type |
| 操作数 | 无 |
| 执行周期 | 1 |
| 执行单元 | EU_VTX |

**操作数约束**：无。

**功能**：终止当前 Vertex Shader 程序的执行，解释器主循环停止。

```
HALT:
    running_ = false
    return false
```

> **注意**：Vertex Shader 程序必须以 VOUTPUT 或 HALT 结尾。若以 HALT 结尾，当前顶点的输出数据不会被送往 Rasterizer（适用于调试场景）。

---

#### JUMP — 无条件跳转

| 字段 | 值 |
|------|-----|
| Opcode | 0x32 |
| 类型 | J-type |
| 操作数 | #signed_offset（10-bit 符号扩展，单位：4字节）|
| 执行周期 | 1 |
| 执行单元 | EU_VTX |

**操作数约束**：无。

**功能**：PC ← PC + offset × 4。

```
JUMP:
    offset = inst.GetSignedImm()
    pc_.addr += static_cast<uint32_t>(offset * 4)
```

---

### 4.2 算术指令

#### ADD — 加法

| 字段 | 值 |
|------|-----|
| Opcode | 0x34 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 执行单元 | EU_ALU |

**操作数约束**：无。

**功能**：Rd = Ra + Rb。

```
ADD:
    reg_file_.Write(rd, reg_file_.Read(ra) + reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### SUB — 减法

| 字段 | 值 |
|------|-----|
| Opcode | 0x35 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 执行单元 | EU_ALU |

**功能**：Rd = Ra - Rb。

```
SUB:
    reg_file_.Write(rd, reg_file_.Read(ra) - reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### MUL — 乘法

| 字段 | 值 |
|------|-----|
| Opcode | 0x36 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 执行单元 | EU_ALU |

**功能**：Rd = Ra × Rb。

```
MUL:
    reg_file_.Write(rd, reg_file_.Read(ra) * reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### DIV — 除法

| 字段 | 值 |
|------|-----|
| Opcode | 0x37 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | **7**（DIV_LATENCY = 7）|
| 执行单元 | EU_SFU |

**功能**：Rd = Ra / Rb；结果延迟 7 个周期写入寄存器（通过 PendingDiv 队列模拟长延迟）。

```
DIV:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    result = (val_b != 0.0f) ? (val_a / val_b) : +infinity
    pending.rd = rd
    pending.result = result
    pending.completion_cycle = stats_.cycles + DIV_LATENCY
    m_pending_divs.push_back(pending)
    pc_.addr += 4
```

> **实现注**：DIV 结果通过 `PendingDiv` 队列延迟写入寄存器。`drainPendingDIVs()` 在每个 `Step()` 开始时调用，将已完成 latency 的结果写回寄存器文件。这模拟了真实 GPU 中 DIV 单元的 7-cycle latency。

---

### 4.3 向量指令

#### DOT3 — 三分量点积

| 字段 | 值 |
|------|-----|
| Opcode | 0x40 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 执行单元 | EU_ALU |

**操作数约束**：Ra, Rb 必须为 4-aligned（Ra%4==0, Rb%4==0），读取 Ra,Ra+1,Ra+2 和 Rb,Rb+1,Rb+2。

**功能**：Rd = dot(Ra.xyz, Rb.xyz) = Ra.x×Rb.x + Ra.y×Rb.y + Ra.z×Rb.z。

```
DOT3:
    v0 = reg_file_.Read(ra)        // Ra   = x
    v1 = reg_file_.Read(ra + 1)   // Ra+1 = y
    v2 = reg_file_.Read(ra + 2)   // Ra+2 = z
    r0 = reg_file_.Read(rb)        // Rb   = x
    r1 = reg_file_.Read(rb + 1)   // Rb+1 = y
    r2 = reg_file_.Read(rb + 2)   // Rb+2 = z
    result = v0*r0 + v1*r1 + v2*r2
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### NORMALIZE — 向量归一化

| 字段 | 值 |
|------|-----|
| Opcode | 0x44 |
| 类型 | R-type（内部使用 Ra 的 3 个连续寄存器）|
| 操作数 | Rd, Ra |
| 执行周期 | **5**（展开序列：DOT3 + RSQ + MUL×3）|
| 执行单元 | EU_ALU + EU_SFU |

**操作数约束**：Ra 必须为 4-aligned（Ra%4==0），读取 Ra, Ra+1, Ra+2 作为 xyz 输入。

**功能**：将输入向量 (x,y,z) 归一化为单位长度，结果写入 Rd。

> **零向量保护**：若输入向量模长接近 0（`sq ≈ 0`，即 x=y=z=0），RSQ(0) 会产生无穷大或 NaN 结果。编译器应生成断言或 special-case 代码，或由硬件 RSQ 单元对零输入返回 +infinity（后续 MUL 产生 NaN 传播）。本 ISA **不硬性规定**零输入行为，编译器负责避免零向量归一化。

```
NORMALIZE:
    x = reg_file_.Read(ra)
    y = reg_file_.Read(ra + 1)
    z = reg_file_.Read(ra + 2)
    sq = x*x + y*y + z*z
    inv_len = RSQ(sq)            // EU_SFU 1-cycle，产生 1.0f/sqrt(sq)
    reg_file_.Write(rd, x * inv_len)
    reg_file_.Write(rd + 1, y * inv_len)
    reg_file_.Write(rd + 2, z * inv_len)
    pc_.addr += 4
```

> **展开序列周期分析**：[DOT3] → [RSQ] → [MUL×3]，总计 1 + 1 + 3 = **5 周期**（含 RSQ 的 EU_SFU 1-cycle latency）。

---

### 4.4 矩阵指令

#### MAT_MUL — 4×4 矩阵乘法

| 字段 | 值 |
|------|-----|
| Opcode | 保留（见 ISA_DESIGN.md 0x28）|
| 类型 | R4-type |
| 操作数 | Rd, Rm, Rv（向量形式）/ Rd, Ra, Rb（矩阵形式）|
| 执行周期 | **4** |
| 执行单元 | EU_VTX（矩阵乘法专用流水线）|

**操作数约束**：
- 向量形式：Rd, Rm, Rv 必须是不同的寄存器编号；Rm 占用 4 个连续寄存器（Rm, Rm+1, Rm+2, Rm+3）；Rv 占用 Rd+1,Rd+2,Rd+3
- 矩阵形式：Rd, Ra, Rb 必须是不同的寄存器编号；每个矩阵占用 4 个连续寄存器

> **Rc 字段说明**：MAT_MUL 采用 R4-type 格式（4寄存器参数），但当前实现中 **Rc 字段被忽略**，第三个操作数由指令功能（向量 vs 矩阵模式）隐含确定。这是 R4-type 格式的过渡设计，未来可在 Rc 中编码矩阵乘法选项（转置左矩阵、转置结果等）。编译器当前应将 Rc 字段置为 0。

**SoftGPU 采用 column-major 存储约定**，矩阵元素在寄存器中的物理布局为：

```
M = | m0  m4  m8  m12 |   → VREG 内容: {m0, m4, m8, m12, m1, m5, m9, m13, ...}
    | m1  m5  m9  m13 |
    | m2  m6  m10 m14 |
    | m3  m7  m11 m15 |
```

**向量形式**：`Rd = Rm × Rv`

```
MAT_MUL_V(Rd, Rm, Rv):
    for i in 0..3:
        sum = 0.0
        for j in 0..3:
            sum += Rm[j].f[i] * Rv.f[j]
        Rd.f[i] = sum
    // 4-cycle latency
```

**数学背景（column-major）**：

```
| a  b  c  d |     | x |
| e  f  g  h | ×   | y |
| i  j  k  l |     | z |
| m  n  o  p |     | w |
```

Column-major 意味着矩阵的列连续排列，结果向量为：

```
r.x = m0*x + m4*y + m8*z + m12*w
r.y = m1*x + m5*y + m9*z + m13*w
r.z = m2*x + m6*y + m10*z + m14*w
r.w = m3*x + m7*y + m11*z + m15*w
```

> **时序图**：
> ```
> Cycle:  1    2    3    4    5
> MAT_MUL: [I] [I] [I] [I] [O]
>           └───────────────┘
>             4-cycle latency
> ```

---

### 4.5 属性指令

#### VLOAD — 从 Vertex Buffer 加载顶点属性

| 字段 | 值 |
|------|-----|
| Opcode | 0x29 |
| 类型 | I-type（Ra 字段隐含硬编码为 R0）|
| 操作数 | Rd, #byte_offset（Ra 隐含为 R0）|
| 执行周期 | **2** |
| 执行单元 | EU_MEM |

**操作数约束**：
- Rd 必须是 4-aligned（Rd % 4 == 0），因为一次加载 4 个 float32
- byte_offset 必须是 4 的倍数

**功能**：从 Vertex Buffer（VBO）按偏移量加载顶点属性到 Rd 开始的连续寄存器。

```
VLOAD(Rd, byte_offset):
    vbo_base = get_vbo_base()
    for i in 0..3:
        addr = vbo_base + byte_offset + i * 4
        value = memory_.Load32(addr)
        reg_file_.Write(rd + i, value)
    stats_.loads++
    pc_.addr += 4
```

> **与 LD 指令的区别**：VLOAD 专用于 Vertex Shader 阶段，从 VBO 按顶点流顺序加载；LD 是通用内存访问指令，可用于任意地址。VLOAD 使用 EU_MEM 单元，可与 EU_VTX（MAT_MUL）并行发射。

---

#### VOUTPUT — 输出裁剪坐标到 Rasterizer

| 字段 | 值 |
|------|-----|
| Opcode | 保留（见 ISA_DESIGN.md 0x26）|
| 类型 | J-type（扩展）|
| 操作数 | Rd, #offset |
| 执行周期 | **2** |
| 执行单元 | EU_VTX |

**操作数约束**：
- Rd 必须是 4-aligned（Rd % 4 == 0），内容布局为 {x, y, z, w}
- VOUTPUTBUF 是专用物理输出缓冲区，共 256 字节（可容纳 16 个完整顶点）

**功能**：将计算所得的裁剪坐标 (x, y, z, w) 输出至 Rasterizer。

```
VOUTPUT(Rd, offset):
    for i in 0..3:
        VOUTPUTBUF[vertex_idx * 4 + i] = Rd.f[i]
    vertex_idx += 1
    pc_.addr += 4
```

> **注意**：VOUTPUT 是**终结指令**。每个顶点着色器程序**必须**以 VOUTPUT 结尾，缺少 VOUTPUT 将导致 Rasterizer 无法接收几何数据，渲染管线挂起。VOUTPUT 固定占用 2 个周期（第二个周期为 bubble）。

---

## 第5章：指令汇总表（Phase 1 — 12条）

| # | 指令 | Opcode | 类型 | 周期 | 执行单元 | 功能 |
|---|------|--------|------|------|---------|------|
| 1 | NOP | 0x30 | J | 1 | EU_VTX | 空操作 |
| 2 | HALT | 0x2A | J | 1 | EU_VTX | 终止程序执行 |
| 3 | JUMP | 0x32 | J | 1 | EU_VTX | 无条件跳转 |
| 4 | ADD | 0x34 | R | 1 | EU_ALU | 加法 |
| 5 | SUB | 0x35 | R | 1 | EU_ALU | 减法 |
| 6 | MUL | 0x36 | R | 1 | EU_ALU | 乘法 |
| 7 | DIV | 0x37 | R | **7** | EU_SFU | 除法（长延迟）|
| 8 | DOT3 | 0x40 | R | 1 | EU_ALU | 三分量点积 |
| 9 | NORMALIZE | 0x44 | R | **5（含RSQ）** | EU_ALU+EU_SFU | 向量归一化 |
| 10 | MAT_MUL | 0x28 | R4 | **4** | EU_VTX | 4×4 矩阵乘法 |
| 11 | VLOAD | 0x29 | I | **2** | EU_MEM | 从 VBO 加载顶点属性 |
| 12 | VOUTPUT | 0x26 | J | **2** | EU_VTX | 输出裁剪坐标 |

**注**：NORMALIZE 展开序列为 DOT3 + RSQ + MUL×3，总计 5 周期（含 RSQ 的 1-cycle latency）。<sup>[注①：NORMALIZE 内部等价发射 DOT3 + RSQ + MUL×3，EU_ALU 执行 DOT3 和三次 MUL，EU_SFU 执行 RSQ，流水线串行总计 5 周期。]</sup>

---

## 第6章：Opcode 编码表

### 6.1 VS Opcode 完整编码表（Phase 1 + Phase 2）

| Opcode (hex) | 名称 | 类型 | 周期 | 执行单元 | 所属阶段 |
|-------------|------|------|------|---------|---------|
| 0x26 | VOUTPUT | J | 2 | EU_VTX | Phase 1 |
| 0x28 | MAT_MUL | R4 | 4 | EU_VTX | Phase 1 |
| 0x29 | VLOAD | I | 2 | EU_MEM | Phase 1 |
| 0x2A | HALT | J | 1 | EU_VTX | Phase 1 |
| **0x30** | **NOP** | J | 1 | EU_VTX | **Phase 1** |
| **0x32** | **JUMP** | J | 1 | EU_VTX | **Phase 1** |
| 0x33 | CBR | B | 1 | EU_VTX | Phase 2 |
| **0x34** | **ADD** | R | 1 | EU_ALU | **Phase 1** |
| **0x35** | **SUB** | R | 1 | EU_ALU | **Phase 1** |
| **0x36** | **MUL** | R | 1 | EU_ALU | **Phase 1** |
| **0x37** | **DIV** | R | **7** | EU_SFU | **Phase 1** |
| 0x38 | MAD | R4 | 1 | EU_ALU | Phase 2 |
| 0x3A | SQRT | U | 1 | EU_SFU | Phase 2 |
| 0x3B | RSQ | U | 1 | EU_SFU | Phase 2 |
| 0x3C | CMP | R | 1 | EU_ALU | Phase 2 |
| 0x3D | MIN | R | 1 | EU_ALU | Phase 2 |
| 0x3E | MAX | R | 1 | EU_ALU | Phase 2 |
| 0x3F | SETP | R | 1 | EU_ALU | Phase 2 |
| **0x40** | **DOT3** | R | 1 | EU_ALU | **Phase 1** |
| 0x41 | DOT4 | R | 1 | EU_ALU | Phase 2 |
| 0x42 | CROSS | R | 1 | EU_ALU | Phase 2 |
| 0x43 | LENGTH | R | 1 | EU_ALU | Phase 2 |
| **0x44** | **NORMALIZE** | R | **5（含RSQ）** | EU_ALU+EU_SFU | **Phase 1** |
| 0x45 | MAT_ADD | R4 | 1 | EU_ALU | Phase 2 |
| 0x46 | MAT_TRANSPOSE | R4 | **4** | EU_ALU | Phase 2 |
| 0x48 | ATTR | I | 1 | EU_MEM | Phase 2 |
| 0x4A | VSTORE | I | 1 | EU_MEM | Phase 2 |
| 0x50 | SIN | U | 1 | EU_SFU | Phase 2 |
| 0x51 | COS | U | 1 | EU_SFU | Phase 2 |
| 0x52 | EXPD2 | U | 1 | EU_SFU | Phase 2 |
| 0x53 | LOGD2 | U | 1 | EU_SFU | Phase 2 |
| 0x54 | POW | R | 1 | EU_SFU | Phase 2 |
| 0x58 | AND | R | 1 | EU_ALU | Phase 2 |
| 0x59 | OR | R | 1 | EU_ALU | Phase 2 |
| 0x5A | XOR | R | 1 | EU_ALU | Phase 2 |
| 0x5B | NOT | U | 1 | EU_ALU | Phase 2 |
| 0x5C | SHL | R | 1 | EU_ALU | Phase 2 |
| 0x5D | SHR | R | 1 | EU_ALU | Phase 2 |
| 0x60 | CVT_F32_S32 | U | 1 | EU_ALU | Phase 2 |
| 0x61 | CVT_F32_U32 | U | 1 | EU_ALU | Phase 2 |
| 0x62 | CVT_S32_F32 | U | 1 | EU_ALU | Phase 2 |
| 0x63 | MOV | U | 1 | EU_ALU | Phase 2 |
| 0x64 | MOV_IMM | U | 1 | EU_ALU | Phase 2 |
| 0x66 | ABS | U | 1 | EU_ALU | Phase 2 |
| 0x67 | NEG | U | 1 | EU_ALU | Phase 2 |
| 0x68 | FLOOR | U | 1 | EU_ALU | Phase 2 |
| 0x69 | CEIL | U | 1 | EU_ALU | Phase 2 |

> **Opcode 去重说明**：HALT 统一为 0x2A（FS ISA 已占用），原 0x31 条目已删除；VLOAD 统一为 0x29，原 0x49 条目已删除。VS opcode 空间以 0x30–0x5F 为准，0x26/0x28/0x29/0x2A 为 VS 专属扩展区（位于 FS 空间 0x00–0x2F 上界，bit5=0，但通过解码器**第一级精确匹配**优先路由至 VS，不受 bit5 路由规则约束）。

### 6.2 VS/FS Opcode 空间划分总览

```
bit5 = 0  →  FS 指令  (0x00 – 0x2F)  ← 原有 ISA_DESIGN.md
bit5 = 1  →  VS 指令  (0x30 – 0x5F)  ← 本文档定义
```

解码器通过检查 opcode 的 bit 5（次高位）自动分流：bit5=0 路由至 FS Interpreter，bit5=1 路由至 VS Interpreter。共用 R-type/I-type/B-type/J-type/U-type 解码逻辑。

---

## 第7章：执行流水线

### 7.1 流水线阶段

VS 指令执行沿用 SoftGPU 通用五级流水线：

```
IF → ID → EX → MEM → WB
```

| 阶段 | 名称 | 功能 |
|------|------|------|
| IF | Instruction Fetch | 从指令缓存取指，PC 更新 |
| ID | Instruction Decode | Opcode 解析，操作数读取，寄存器重命名检查 |
| EX | Execute | 算术/逻辑运算，EU 调度 |
| MEM | Memory Access | VLOAD/VSTORE 的 EU_MEM 访问（1个额外周期）|
| WB | Write Back | 结果写回寄存器文件 |

### 7.2 执行单元并行调度

VS 指令涉及两类执行单元，可并行发射：

| 执行单元 | 指令 | 占用周期 |
|---------|------|---------|
| EU_VTX | MAT_MUL, VOUTPUT, NOP, HALT, JUMP | 矩阵单元流水线 |
| EU_MEM | VLOAD | 独立内存访问端口 |
| EU_ALU | ADD, SUB, MUL, DOT3, MAD 等 | 通用算术逻辑 |
| EU_SFU | DIV, SQRT, RSQ 等 | 特殊功能单元 |

**并行发射约束**：
- VLOAD（EU_MEM）与 MAT_MUL（EU_VTX）可在同一 Cycle 并行发射，互不阻塞
- VOUTPUT（EU_VTX）与 VLOAD（EU_MEM）不可并行（共享顶点进度计数器）

### 7.3 DIV 长延迟机制

DIV 指令（opcode 0x37）具有 7-cycle latency，通过 PendingDiv 队列实现非阻塞执行：

```
Cycle 1:  DIV 发射，结果进入 PendingDiv 队列（completion_cycle = 当前+7）
Cycle 2-7: 其他指令正常执行（无 stall）
Cycle 8:  drainPendingDIVs() 将结果写回寄存器
```

### 7.4 MAT_MUL 流水线

MAT_MUL（opcode 0x28）占用 EU_VTX 矩阵流水线 4 个周期：

```
Cycle  1    2    3    4    5
MAT_MUL: [I] [I] [I] [I] [O]
```

同一线程在 MAT_MUL 流水线占用期间不可发射新的 EU_VTX 指令，但可以发射 EU_MEM（VLOAD）。

---

## 第8章：分类索引

### 8.1 按执行周期分类

**1-cycle 指令**：NOP, HALT, JUMP, ADD, SUB, MUL, DOT3, CBR, MAD, SQRT, RSQ, CMP, MIN, MAX, SETP, DOT4, CROSS, LENGTH, MAT_ADD, SIN, COS, EXPD2, LOGD2, POW, AND, OR, XOR, NOT, SHL, SHR, MOV, MOV_IMM, CVT_F32_S32, CVT_F32_U32, CVT_S32_F32, ATTR, VSTORE

**2-cycle 指令**：VLOAD, VOUTPUT

**4-cycle 指令**：MAT_MUL, MAT_TRANSPOSE

**7-cycle 指令**：DIV

**5-cycle 指令**：NORMALIZE（展开序列：DOT3 + RSQ + MUL×3 = 1+1+3=5 周期）

### 8.2 按指令类型分类

**J-type（5条）**：NOP, HALT, JUMP, VOUTPUT, (CBR in B-type)

**R-type（18条）**：ADD, SUB, MUL, DIV, DOT3, DOT4, CROSS, LENGTH, NORMALIZE, CMP, MIN, MAX, SETP, AND, OR, XOR, SHL, SHR, POW

**R4-type（4条）**：MAD, MAT_MUL, MAT_ADD, MAT_TRANSPOSE

**U-type（15条）**：MOV, MOV_IMM, SQRT, RSQ, ABS, NEG, FLOOR, CEIL, SIN, COS, EXPD2, LOGD2, CVT_F32_S32, CVT_F32_U32, CVT_S32_F32

**I-type（3条）**：VLOAD, VSTORE, ATTR

**B-type（1条）**：CBR

> **DOT3 与 DP3 功能重叠说明**：VS DOT3（0x40）与 FS DP3 功能完全相同（均为 xyz 三分量点积）。VS 保持独立 opcode 空间（0x30–0x5F）而非复用 FS opcode，原因是：VS 与 FS 在不同渲染管线阶段分时使用，共享 opcode 空间会增加解码器复杂度；此外 VS 专属指令（如 VLOAD、VOUTPUT、MAT_MUL）在 FS 空间无对应物，独立空间更清晰。在统一解码框架下，bit5 路由已将 VS/FS 分流，共用 EU_ALU 单元执行 DOT3，物理上无冗余。

### 8.3 按执行单元分类

**EU_VTX**：NOP, HALT, JUMP, MAT_MUL, VOUTPUT

**EU_MEM**：VLOAD, VSTORE, ATTR, (LD/ST if applicable)

**EU_ALU**：ADD, SUB, MUL, DOT3, DOT4, CROSS, MAD, CMP, MIN, MAX, SETP, MAT_ADD, MAT_TRANSPOSE, AND, OR, XOR, SHL, SHR, MOV, MOV_IMM, CVT_F32_S32, CVT_F32_U32, CVT_S32_F32, (LD/ST)

**EU_SFU**：DIV, SQRT, RSQ, SIN, COS, EXPD2, LOGD2, POW

---

## 第9章：与 Fragment Shader ISA 的共存方案

### 9.1 统一解码框架

VS 与 FS 指令共用同一个 32-bit 指令格式，解码器通过两级分流自动路由：

**第一级：精确匹配（优先级最高）**
```
if (opcode == 0x26 ||   // VOUTPUT
    opcode == 0x28 ||   // MAT_MUL
    opcode == 0x29 ||   // VLOAD
    opcode == 0x2A)     // HALT
      → Dispatch to VS Interpreter (EU_VTX/EU_MEM)
```

**第二级：bit5 路由（精确匹配未命中时）**
```
if (opcode[5] == 0)  →  Dispatch to FS Interpreter (EU_ALU/EU_SFU/EU_MEM/EU_TEX)
if (opcode[5] == 1)  →  Dispatch to VS Interpreter (EU_VTX/EU_MEM)
```

> **注意**：VOUTPUT(0x26)、MAT_MUL(0x28)、VLOAD(0x29)、HALT(0x2A) 的 opcode bit5 均为 0，精确匹配是防止它们被错误路由至 FS 的关键设计。

寄存器文件（64×32-bit float R0–R63）被 VS 和 FS **共享**，但在不同渲染阶段分时使用：
- Vertex Shader 阶段：寄存器由 VS 程序写入，VOUTPUT 后数据送 Rasterizer
- Fragment Shader 阶段：寄存器由 FS 程序写入，FRAMEBUF_WRITE 后数据送 Framebuffer

### 9.2 共存执行流程

```
DrawCall
  └─> VertexShaderInterpreter
        └─> VOUTPUT → VOUTPUTBUF → Rasterizer
              └─> FragmentShaderInterpreter
                    └─> FRAMEBUF_WRITE → Framebuffer
```

### 9.3 寄存器隔离约定

虽然 VS 和 FS 共享同一寄存器文件硬件，但编译器应遵循以下隔离约定：

- **VS 阶段**：R0–R63 由 VS 程序使用，FS 不可见
- **Rasterizer**：通过专用 VOUTPUTBUF 传递数据，不经通用寄存器
- **FS 阶段**：R0–R63 由 FS 程序使用，与 VS 阶段完全隔离（时间维度）

### 9.4 Opcode 空间管理

| Opcode 区间 | 归属 | 说明 |
|------------|------|------|
| 0x00–0x2F | Fragment Shader ISA | 已有 38 条 FS 指令 |
| 0x30–0x5F | Vertex Shader ISA | 32 条 VS 指令空间 |
| 0x60–0x7F | 保留 | 扩展用 |

### 9.5 EU 资源隔离

| 执行单元 | VS 指令 | FS 指令 |
|---------|--------|--------|
| EU_VTX | MAT_MUL, VOUTPUT, VPOINT_SIZE | 无 |
| EU_MEM | VLOAD | LD, ST |
| EU_ALU | ADD, SUB, MUL, DOT3, ... | ADD, SUB, MUL, ... |
| EU_SFU | DIV, SQRT, RSQ, ... | DIV, SQRT, RSQ, ... |
| EU_TEX | 无 | TEX, SAMPLE |

### 9.6 VS 指令跨阶段兼容性说明

以下 FS 指令在 VS 上下文中**语义相同**，可直接复用 EU_ALU/EU_SFU：
- ADD, SUB, MUL, DIV（opcode 映射：0x01→0x34, 0x02→0x35, 0x03→0x36, 0x04→0x37）
- CMP, MIN, MAX（opcode 映射：0x0B→0x3C, 0x0D→0x3D, 0x0E→0x3E）
- SQRT, RSQ（opcode 映射：0x07→0x3A, 0x08→0x3B）
- SHL, SHR（opcode 映射：0x1D→0x5C, 0x1E→0x5D）

---

## 附录A：Phase 2 指令清单（详细规格待补充）

以下 20 条指令为 Phase 2 扩展计划，**本文档不包含详细伪代码规格**，仅列出 opcode 和功能摘要：

### A.1 控制流

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x33 | CBR | 条件分支：若 Ra ≠ 0 则跳转（Ra 为条件寄存器）|

### A.2 算术扩展

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x38 | MAD | 乘加：Rd = Ra × Rb + Rc |
| 0x3A | SQRT | 平方根：Rd = sqrt(Ra) |
| 0x3B | RSQ | 倒数平方根：Rd = 1/sqrt(Ra) |

### A.3 比较与选择

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x3C | CMP | 比较：Rd = (Ra < Rb) ? 1.0f : 0.0f |
| 0x3D | MIN | 最小值：Rd = min(Ra, Rb) |
| 0x3E | MAX | 最大值：Rd = max(Ra, Rb) |
| 0x3F | SETP | 谓词设置：设置内部谓词寄存器（供 CBR 使用）|

### A.4 向量扩展

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x41 | DOT4 | 四分量点积：Rd = dot(Ra.xyzw, Rb.xyzw) |
| 0x42 | CROSS | 叉积：Rd = cross(Ra.xyz, Rb.xyz)（结果写 xyz）|
| 0x43 | LENGTH | 向量长度：Rd = sqrt(dot(Ra.xyz, Ra.xyz)) |

### A.5 矩阵扩展

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x45 | MAT_ADD | 矩阵加法：Rd = Ra + Rb（4×4，逐元素加）|
| 0x46 | MAT_TRANSPOSE | 矩阵转置：Rd = transpose(Ra) |

### A.6 超越函数（SFU）

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x50 | SIN | 正弦：Rd = sin(Ra) |
| 0x51 | COS | 余弦：Rd = cos(Ra) |
| 0x52 | EXPD2 | 以2为底的指数：Rd = 2^Ra |
| 0x53 | LOGD2 | 以2为底的对数：Rd = log2(Ra) |
| 0x54 | POW | 幂函数：Rd = Ra^Rb |

### A.7 逻辑操作

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x58 | AND | 按位与：Rd = Ra & Rb（float bit pattern）|
| 0x59 | OR | 按位或：Rd = Ra \| Rb |
| 0x5A | XOR | 按位异或：Rd = Ra ^ Rb |
| 0x5B | NOT | 按位取反：Rd = ~Ra |
| 0x5C | SHL | 按位左移：Rd = Ra << Rb |
| 0x5D | SHR | 按位右移：Rd = Ra >> Rb |

### A.8 类型转换

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x60 | CVT_F32_S32 | Float→Signed Int 位转换 |
| 0x61 | CVT_F32_U32 | Float→Unsigned Int 位转换 |
| 0x62 | CVT_S32_F32 | Signed Int→Float 位转换 |

### A.9 通用寄存器操作

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x63 | MOV | 移动：Rd = Ra（float 复制）|
| 0x64 | MOV_IMM | 移动立即数：Rd = #imm（符号/无符号扩展）|

### A.10 插值与属性

| Opcode | 名称 | 功能摘要 |
|--------|------|---------|
| 0x48 | ATTR | 顶点属性插值请求：从 Rasterizer 获取指定属性的插值结果 |
| 0x4A | VSTORE | 存储到 Vertex Buffer：写入 VBO（用于 transform feedback 等特性）|

---

## 附录B：Vertex Shader 程序模板（Phase 1 MVP 变换）

```
; ============================================================
; SoftGPU Vertex Shader ISA — MVP Transform (Phase 1)
; 输入:   Vertex Buffer 含 3 个属性（位置、法线、UV）
;         模型矩阵(M)、视图矩阵(V)、投影矩阵(P) 已预加载
; 输出:   裁剪坐标到 Rasterizer
; 图元类型: TRIANGLE（VOUTPUT 触发光栅化）
; ============================================================

.alias  VTX_POS    R0      ; 顶点位置 (x,y,z,w)
.alias  VTX_NRM    R1      ; 顶点法线 (nx,ny,nz)
.alias  VTX_UV     R2      ; 顶点纹理坐标 (u,v)
.alias  TEMP0      R3      ; 临时向量
.alias  TEMP1      R4      ; 临时向量
.alias  CLIP_POS   R5      ; 裁剪坐标（VOUTPUT 输入）

.alias  M_MAT      R8      ; Model 矩阵（占 R8-R11）
.alias  V_MAT      R12     ; View 矩阵（占 R12-R15）
.alias  P_MAT      R16     ; Projection 矩阵（占 R16-R19）

; --- 程序入口 ---
.entry:
    ; [阶段1] 从 Vertex Buffer 加载顶点属性
    VLOAD   VTX_POS, #0          ; 加载: x=offset+0, y=+4, z=+8, w=+12
    VLOAD   VTX_NRM, #16         ; 加载法线: nx=+16, ny=+20, nz=+24
    VLOAD   VTX_UV,  #32         ; 加载 UV: u=+32, v=+36

    ; [阶段2] Model 变换: TEMP0 = M_MAT × VTX_POS
    MAT_MUL_V  TEMP0, M_MAT, VTX_POS

    ; [阶段3] View 变换: TEMP1 = V_MAT × TEMP0
    MAT_MUL_V  TEMP1, V_MAT, TEMP0

    ; [阶段4] Projection 变换: CLIP_POS = P_MAT × TEMP1
    MAT_MUL_V  CLIP_POS, P_MAT, TEMP1

    ; [阶段5] 输出到 Rasterizer（每个顶点程序必须以 VOUTPUT 结尾）
    VOUTPUT  CLIP_POS, #0

.exit:
    HALT
```

**时序分析（单顶点执行）**：
```
Cycle   1      2      3      4      5      6      7      8      9     10
VLOAD   [====] [====]
VLOAD                            [====] [====]
MAT_MUL       [====] [====] [====] [====] [O]
MAT_MUL                                           [====] [====] [====] [====] [O]
MAT_MUL                                                                            [====] [====] [====] [====] [O]
VOUTPUT                                                                [====] [====]
```

---

## 附录C：验收标准（Phase 1）

### C.1 功能验收

| ID | 测试项 | 验证方法 |
|----|--------|---------|
| VA-1 | NOP 正确推进 PC 4 字节 | 执行 10 条 NOP 后 PC = 40 |
| VA-2 | HALT 正确停止解释器 | running_ == false |
| VA-3 | JUMP 正确跳转 | 跳转后 PC 符合 offset 计算 |
| VA-4 | ADD/SUB/MUL 算术正确 | 随机向量对比参考实现 |
| VA-5 | DIV 7-cycle 延迟正确 | 检查 PendingDiv 队列 |
| VA-6 | DOT3 4-aligned 约束 | Ra/Rb 非对齐时行为 undefined |
| VA-7 | NORMALIZE 输出模长为 1 | 误差 < 1e-6 |
| VA-8 | MAT_MUL column-major 正确 | 与 BLAS 对比 |
| VA-9 | VLOAD 加载 4 个 float32 | 对比 VBO 原始数据 |
| VA-10 | VOUTPUT 写入 VOUTPUTBUF | 检查缓冲区内容 |

### C.2 性能验收

| ID | 测试项 | 目标 |
|----|--------|------|
| VP-1 | 单顶点 MVP 变换延迟 | ≤ 20 周期 |
| VP-2 | MAT_MUL 吞吐率 | 每周期完成 1 次（流水线）|
| VP-3 | VLOAD + MAT_MUL 并行发射 | 无 hazard stall |

### C.3 边界条件

| ID | 测试项 | 预期行为 |
|----|--------|---------|
| VB-1 | clip_w = 0 时 VOUTPUT | 产生除零标记，不崩溃 |
| VB-2 | VLOAD byte_offset 越界 | 返回 0.0f，stats_.loads++ |
| VB-3 | MAT_MUL Rd == Rm | 编译器应拒绝 |
| VB-4 | DOT3 Ra 未 4-aligned | 编译器应拒绝 |
