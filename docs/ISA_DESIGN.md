# SoftGPU ISA Design Specification

**版本：** 1.0  
**作者：** SoftGPU Architecture Team  
**日期：** 2026-03-25

---

## 第1章：概述

### 1.1 设计目标

SoftGPU ISA（Instruction Set Architecture）是专为软渲染GPU仿真器设计的类GPU着色器指令集。它模拟真实GPU的并行执行模型（SIMT），支持在CPU端高效仿真fragment shader和vertex shader的执行。设计目标如下：

- **可仿真性**：指令语义清晰，适合在通用CPU上解释执行（interpreted execution）
- **固定长度**：所有指令为32-bit，简化fetch/decode逻辑
- **RISC风格**：寄存器-寄存器操作为主，少量立即数寻址，减少访存复杂度
- **真实GPU特性**：支持 warp/thread 概念、texture sampling、特殊功能单元（SFU）
- **可扩展**：预留opcode空间，支持后续阶段（PHASE1-3）功能扩展

### 1.2 指令编码格式

所有指令为32-bit固定长度，采用以下位域布局：

```
 31    25 24    20 19    15 14    10 9              0
+--------+-------+-------+-------+------------------+
| Opcode |  Rd   |  Ra   |  Rb   |   Immediate     |
| 7 bit  | 5 bit | 5 bit | 5 bit |     10 bit      |
+--------+-------+-------+-------+------------------+
```

- **Opcode (7 bit)**：指令操作码，bit[31:25]，共128个编码空间
- **Rd (5 bit)**：目标寄存器，bit[24:20]，可寻址 R0-R63
- **Ra (5 bit)**：源寄存器A，bit[19:15]
- **Rb (5 bit)**：源寄存器B，bit[14:10]
- **Immediate (10 bit)**：立即数，bit[9:0]

### 1.3 执行模型

SoftGPU ISA 在**单线程解释器**（Interpreter）中顺序执行。每条指令执行后 `stats_.cycles++`，反映真实GPU的周期级计费模型。指令间通过 `PendingDiv` 队列模拟长延迟操作（如DIV）的周期占用。

---

## 第2章：寄存器模型

### 2.1 Register File 规格

| 参数 | 值 |
|------|-----|
| 寄存器数量 | 64 个标量寄存器 |
| 寄存器宽度 | 32-bit float（IEEE 754 single precision） |
| 寄存器编号 | R0 – R63 |
| 特殊寄存器 | R0 恒为 0.0f（zero register，硬件级硬连线）|
| 寻址方式 | 5-bit 直接寻址 |

### 2.2 行为约束

- **R0 为只读 0.0f**：无论写入什么值，Read(R0) 始终返回 0.0f
- **越界访问**：未定义（访问 R64+ 的行为取决于物理实现，编译器负责保证不越界）
- **数据类型**：所有寄存器均为 float32 bit pattern，整数操作通过 `reinterpret_cast<uint32_t&>` 实现

### 2.3 特殊寻址约定（DP3 指令）

`DP3`（opcode 0x25）指令要求 Ra 和 Rb 为 **4-aligned**（编号能被4整除），将 Ra, Ra+1, Ra+2 作为 xyz 三分量向量，Rb, Rb+1, Rb+2 作为另一向量。这是软渲染场景下 SIMD vectorization 的编译器级约定。

---

## 第3章：指令格式分类

SoftGPU ISA 将指令分为以下类型（IType）：

### 3.1 R-type — 三寄存器指令

```
Rd, Ra, Rb
```

格式：`[Opcode(7)] [Rd(5)] [Ra(5)] [Rb(5)] [0000000000(10)]`

| 操作码 | 指令 |
|--------|------|
| ADD | Rd = Ra + Rb |
| SUB | Rd = Ra - Rb |
| MUL | Rd = Ra × Rb |
| DIV | Rd = Ra / Rb（7-cycle latency）|
| AND | Rd = Ra & Rb（按位与）|
| OR | Rd = Ra \| Rb（按位或）|
| CMP | Rd = (Ra < Rb) ? 1.0f : 0.0f |
| MIN | Rd = min(Ra, Rb) |
| MAX | Rd = max(Ra, Rb) |

### 3.2 R4-type — 四寄存器指令（含隐含第三操作数）

```
Rd, Ra, Rb, Rc
```

Ra/Rb/Rc 均为寄存器编号，Rc 编码在 imm field 的高5位 `[9:5]`。

| 操作码 | 指令 |
|--------|------|
| MAD | Rd = Ra × Rb + Rc |
| SEL | Rd = (Rc != 0) ? Ra : Rb |
| TEX | TEX Rd, Ra(u), Rb(v), Rc(tex_id)；写入 Rd,Rd+1,Rd+2,Rd+3（rgba）|
| SAMPLE | 同 TEX（简化版纹理采样）|
| SMOOTHSTEP | Rd = smoothstep(Ra, Rb, Rc)（Hermite插值）|

### 3.3 U-type — 单寄存器 + 立即数指令

```
Rd, Ra [, #imm]
```

| 操作码 | 指令 |
|--------|------|
| RCP | Rd = 1.0f / Ra |
| SQRT | Rd = sqrt(Ra) |
| RSQ | Rd = 1.0f / sqrt(Ra) |
| MOV | Rd = Ra |
| F2I | Rd = bitcast(float→int, Ra) |
| I2F | Rd = bitcast(int→float, Ra) |
| FRACT | Rd = Ra - floor(Ra) |
| LDC | Rd = CONST_BUF[Ra][#imm] |
| NOT | Rd = ~Ra（按位取反）|
| FLOOR | Rd = floor(Ra) |
| CEIL | Rd = ceil(Ra) |
| ABS | Rd = abs(Ra) |
| NEG | Rd = -Ra |

### 3.4 I-type — 两寄存器 + 偏移立即数（内存访问）

```
LD  Rd, [Ra + #imm]    ; 加载
ST  [Ra + #imm], Rb    ; 存储（Ra=基址，Rb=数据）
```

### 3.5 B-type — 条件分支

```
BRA  Rc, #signed_offset
```

Rc 为条件寄存器（非零则跳转），offset 为符号扩展的10-bit立即数（单位：4字节）。

### 3.6 J-type — 无条件跳转 / 函数调用

```
JMP  target            ; 相对跳转
CALL target            ; 调用（保存返回地址到R1）
RET                    ; 返回（从R1恢复PC）
NOP                    ; 空操作
BAR                    ; Warp内线程同步
```

---

## 第4章：Fragment Shader 指令集（38条）

### 4.1 控制流指令

#### NOP — 空操作

| 字段 | 值 |
|------|-----|
| Opcode | 0x00 |
| 类型 | J-type |
| 操作数 | 无 |
| 执行周期 | 1 |
| 功能 | 空操作，不修改任何寄存器或PC之外的任何状态 |

```
NOP:
    pc_.addr += 4
```

---

#### JMP — 无条件跳转

| 字段 | 值 |
|------|-----|
| Opcode | 0x12 |
| 类型 | J-type |
| 操作数 | #signed_offset（10-bit符号扩展，单位：4字节）|
| 执行周期 | 1 |
| 功能 | PC ← PC + offset × 4 |

```
JMP:
    offset = inst.GetSignedImm()
    pc_.addr += static_cast<uint32_t>(offset * 4)
```

---

#### BRA — 条件分支

| 字段 | 值 |
|------|-----|
| Opcode | 0x11 |
| 类型 | B-type |
| 操作数 | Rc（条件寄存器）, #signed_offset（10-bit符号扩展）|
| 执行周期 | 1 |
| 功能 | 若 Rc ≠ 0，则 PC ← PC + offset × 4 |

```
BRA:
    cond = reg_file_.Read(inst.GetRc())
    offset = inst.GetSignedImm()
    if (cond != 0.0f) {
        pc_.addr += static_cast<uint32_t>(offset * 4)
        stats_.branches_taken++
    } else {
        pc_.addr += 4
    }
```

---

#### CALL — 函数调用

| 字段 | 值 |
|------|-----|
| Opcode | 0x13 |
| 类型 | J-type |
| 操作数 | #signed_offset |
| 执行周期 | 1 |
| 功能 | 保存返回地址到 R1，跳转到目标地址 |

```
CALL:
    pc_.link = pc_.addr + 4
    offset = inst.GetSignedImm()
    pc_.addr += static_cast<uint32_t>(offset * 4)
    reg_file_.Write(1, *reinterpret_cast<float*>(&pc_.link))
```

---

#### RET — 函数返回

| 字段 | 值 |
|------|-----|
| Opcode | 0x14 |
| 类型 | J-type |
| 操作数 | 无（返回地址从 R1 读取）|
| 执行周期 | 1 |
| 功能 | PC ← R1（link register）|

```
RET:
    pc_.addr = pc_.link
```

---

### 4.2 算术指令

#### ADD — 加法

| 字段 | 值 |
|------|-----|
| Opcode | 0x01 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra + Rb |

```
ADD:
    reg_file_.Write(rd, reg_file_.Read(ra) + reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### SUB — 减法

| 字段 | 值 |
|------|-----|
| Opcode | 0x02 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra - Rb |

```
SUB:
    reg_file_.Write(rd, reg_file_.Read(ra) - reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### MUL — 乘法

| 字段 | 值 |
|------|-----|
| Opcode | 0x03 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra × Rb |

```
MUL:
    reg_file_.Write(rd, reg_file_.Read(ra) * reg_file_.Read(rb))
    pc_.addr += 4
```

---

#### DIV — 除法

| 字段 | 值 |
|------|-----|
| Opcode | 0x04 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | **7**（DIV_LATENCY = 7）|
| 功能 | Rd = Ra / Rb；结果写入被延迟7个周期（通过 PendingDiv 队列模拟长延迟）|

```
DIV:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    result = (val_b != 0.0f) ? (val_a / val_b) : +infinity
    pending.rd = rd
    pending.result = result
    pending.completion_cycle = stats_.cycles + DIV_LATENCY
    m_pending_divs.push_back(pending)
    // 注意：结果不立即写入 reg_file_，而是在 completion_cycle 时写入
    pc_.addr += 4
```

> **实现注**：DIV 结果通过 `PendingDiv` 队列延迟写入寄存器。`drainPendingDIVs()` 在每个 `Step()` 开始时调用，将已完成 latency 的结果写回寄存器文件。这模拟了真实GPU中DIV单元的7-cycle latency，期间允许其他指令执行。

---

#### MAD — 乘加

| 字段 | 值 |
|------|-----|
| Opcode | 0x05 |
| 类型 | R4-type |
| 操作数 | Rd, Ra, Rb, Rc |
| 执行周期 | 1 |
| 功能 | Rd = Ra × Rb + Rc |

```
MAD:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    val_c = reg_file_.Read(inst.GetRc())
    reg_file_.Write(rd, val_a * val_b + val_c)
    pc_.addr += 4
```

---

#### RCP — 倒数

| 字段 | 值 |
|------|-----|
| Opcode | 0x06 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = 1.0f / Ra；若 Ra=0，结果为 +infinity |

```
RCP:
    val_a = reg_file_.Read(ra)
    result = (val_a != 0.0f) ? (1.0f / val_a) : +infinity
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### SQRT — 平方根

| 字段 | 值 |
|------|-----|
| Opcode | 0x07 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = sqrt(Ra)；若 Ra < 0，结果为 NaN |

```
SQRT:
    val_a = reg_file_.Read(ra)
    result = (val_a >= 0.0f) ? std::sqrt(val_a) : std::nanf("")
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### RSQ — 倒数平方根

| 字段 | 值 |
|------|-----|
| Opcode | 0x08 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = 1.0f / sqrt(Ra)；若 Ra=0，结果为 +infinity；若 Ra<0，结果为 NaN |

```
RSQ:
    val_a = reg_file_.Read(ra)
    if (val_a > 0.0f)
        result = 1.0f / std::sqrt(val_a)
    else if (val_a == 0.0f)
        result = +infinity
    else
        result = std::nanf("")
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

### 4.3 逻辑与比较指令

#### AND — 按位与

| 字段 | 值 |
|------|-----|
| Opcode | 0x09 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra & Rb（IEEE-754 float bit pattern 按位与）|

```
AND:
    ia = *reinterpret_cast<uint32_t*>(&reg_file_.Read(ra))
    ib = *reinterpret_cast<uint32_t*>(&reg_file_.Read(rb))
    *reinterpret_cast<uint32_t*>(&result) = ia & ib
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### OR — 按位或

| 字段 | 值 |
|------|-----|
| Opcode | 0x0A |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra \| Rb（IEEE-754 float bit pattern 按位或）|

```
OR:
    ia = *reinterpret_cast<uint32_t*>(&reg_file_.Read(ra))
    ib = *reinterpret_cast<uint32_t*>(&reg_file_.Read(rb))
    *reinterpret_cast<uint32_t*>(&result) = ia | ib
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### CMP — 比较

| 字段 | 值 |
|------|-----|
| Opcode | 0x0B |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = (Ra < Rb) ? 1.0f : 0.0f |

```
CMP:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, (val_a < val_b) ? 1.0f : 0.0f)
    pc_.addr += 4
```

---

#### SEL — 条件选择

| 字段 | 值 |
|------|-----|
| Opcode | 0x0C |
| 类型 | R4-type |
| 操作数 | Rd, Ra, Rb, Rc |
| 执行周期 | 1 |
| 功能 | Rd = (Rc != 0) ? Ra : Rb |

```
SEL:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    val_c = reg_file_.Read(inst.GetRc())
    reg_file_.Write(rd, (val_c != 0.0f) ? val_a : val_b)
    pc_.addr += 4
```

---

### 4.4 数学指令

#### MIN — 最小值

| 字段 | 值 |
|------|-----|
| Opcode | 0x0D |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = min(Ra, Rb) |

```
MIN:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, (val_a < val_b) ? val_a : val_b)
    pc_.addr += 4
```

---

#### MAX — 最大值

| 字段 | 值 |
|------|-----|
| Opcode | 0x0E |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = max(Ra, Rb) |

```
MAX:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, (val_a > val_b) ? val_a : val_b)
    pc_.addr += 4
```

---

### 4.5 内存访问指令

#### LD — 加载

| 字段 | 值 |
|------|-----|
| Opcode | 0x0F |
| 类型 | I-type |
| 操作数 | Rd, [Ra + #imm] |
| 执行周期 | 1 |
| 功能 | 从内存地址 (Ra + imm) 加载一个 float32 到 Rd |
| 异常处理 | 若地址无效（NaN/Inf/越界），返回 0.0f；stats_.loads++ |

```
LD:
    val_a = reg_file_.Read(ra)  // base address
    offset = inst.GetImm()
    if valid_addr(val_a, offset):
        addr = static_cast<uint32_t>(val_a) + offset
        value = memory_.Load32(addr)
    else:
        value = 0.0f
    reg_file_.Write(rd, value)
    stats_.loads++
    pc_.addr += 4
```

> **地址验证**：val_a 必须为非NaN、非Inf、非负数，且 val_a + offset + 4 不超过 memory size。

---

#### ST — 存储

| 字段 | 值 |
|------|-----|
| Opcode | 0x10 |
| 类型 | I-type |
| 操作数 | [Ra + #imm] = Rb |
| 执行周期 | 1 |
| 功能 | 将 Rb 的值存入内存地址 (Ra + imm) |
| 异常处理 | 若地址无效则静默忽略；stats_.stores++ |

```
ST:
    val_a = reg_file_.Read(ra)  // base address
    val_b = reg_file_.Read(rb)  // value to store
    offset = inst.GetImm()
    if valid_addr(val_a, offset):
        addr = static_cast<uint32_t>(val_a) + offset
        memory_.Store32(addr, val_b)
    stats_.stores++
    pc_.addr += 4
```

---

### 4.6 数据转换指令

#### MOV — 移动

| 字段 | 值 |
|------|-----|
| Opcode | 0x15 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = Ra（float 复制）|

```
MOV:
    reg_file_.Write(rd, reg_file_.Read(ra))
    pc_.addr += 4
```

---

#### F2I — Float to Integer（位转换）

| 字段 | 值 |
|------|-----|
| Opcode | 0x16 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | 将 Ra 的 IEEE-754 bit pattern 解释为 int32_t，结果写入 Rd |

```
F2I:
    i = *reinterpret_cast<int32_t*>(&reg_file_.Read(ra))
    reg_file_.Write(rd, *reinterpret_cast<float*>(&i))
    pc_.addr += 4
```

---

#### I2F — Integer to Float（位转换）

| 字段 | 值 |
|------|-----|
| Opcode | 0x17 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | 将 Ra 的 bit pattern 从 int32_t 转换为 float |

```
I2F:
    i = *reinterpret_cast<int32_t*>(&reg_file_.Read(ra))
    reg_file_.Write(rd, static_cast<float>(i))
    pc_.addr += 4
```

---

#### FRACT — 小数部分

| 字段 | 值 |
|------|-----|
| Opcode | 0x18 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = Ra - floor(Ra)（返回 Ra 的小数部分）|

```
FRACT:
    val_a = reg_file_.Read(ra)
    reg_file_.Write(rd, val_a - std::floor(val_a))
    pc_.addr += 4
```

---

### 4.7 特殊操作指令（TEX/SAMPLE/LDC/BAR）

#### TEX — 纹理采样

| 字段 | 值 |
|------|-----|
| Opcode | 0x19 |
| 类型 | R4-type |
| 操作数 | Rd, Ra(u), Rb(v), Rc(tex_id) |
| 执行周期 | **8** |
| 功能 | 对纹理 tex_id 进行二维采样，结果写入 Rd, Rd+1, Rd+2, Rd+3（rgba）|
| Fallback | 若纹理缓冲区无效，执行 checkerboard 图案（8×8 黑白色块）|

```
TEX:
    u = reg_file_.Read(ra)
    v = reg_file_.Read(rb)
    tex_id = static_cast<int>(reg_file_.Read(inst.GetRc()))
    if tex_id valid && m_textureBuffers[tex_id] != nullptr:
        color = m_textureBuffers[tex_id]->sampleNearest(u, v)
        reg_file_.Write(rd,   color.r)
        reg_file_.Write(rd+1, color.g)
        reg_file_.Write(rd+2, color.b)
        reg_file_.Write(rd+3, color.a)
    else:
        // checkerboard fallback
        cx = floor(u * 8.0f); cy = floor(v * 8.0f)
        is_white = ((cx + cy) % 2) == 0
        color = is_white ? 1.0f : 0.0f
        reg_file_.Write(rd,   color)
        reg_file_.Write(rd+1, color)
        reg_file_.Write(rd+2, color)
        reg_file_.Write(rd+3, 1.0f)
    pc_.addr += 4
```

> **实现注**：TEX 指令将颜色写入 **4个连续寄存器**（Rd 到 Rd+3），因此编译器应确保这些寄存器槽无冲突。纹理坐标 (u,v) 范围通常为 [0,1]。

---

#### SAMPLE — 简化纹理采样

| 字段 | 值 |
|------|-----|
| Opcode | 0x1A |
| 类型 | R4-type |
| 操作数 | Rd, Ra(u), Rb(v), Rc(tex_id) |
| 执行周期 | **4** |
| 功能 | 等同于 TEX（nearest-neighbor 采样），为 v1.x 简化版接口 |

```
SAMPLE:
    // 与 TEX 实现完全相同
    // 等同于 TEX
```

---

#### LDC — 加载常量

| 字段 | 值 |
|------|-----|
| Opcode | 0x1B |
| 类型 | U-type |
| 操作数 | Rd, Ra(const_buf_id), #imm(offset) |
| 执行周期 | 1 |
| 功能 | 从常量缓冲区 Ra 的 offset 处加载一个 float 到 Rd（stub 实现）|

```
LDC:
    // Stub for v1.0: 不实际访问常量缓冲区
    pc_.addr += 4
```

> **状态**：v1.0 stub，常量缓冲区访问功能待实现。

---

#### BAR — Warp 同步栅栏

| 字段 | 值 |
|------|-----|
| Opcode | 0x1C |
| 类型 | J-type |
| 操作数 | 无 |
| 执行周期 | 1 |
| 功能 | 同步同一 warp 内所有线程（stub 实现，v1.0 不实际等待）|

```
BAR:
    // Stub for v1.0: 不实际等待
    pc_.addr += 4
```

> **状态**：v1.0 stub，硬件同步功能待实现。

---

### 4.8 PHASE3 扩展指令（位操作与数学扩展）

#### SHL — 按位左移

| 字段 | 值 |
|------|-----|
| Opcode | 0x1D |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra << Rb（将 Ra 的 IEEE-754 bit pattern 左移 Rb 位）|

```
SHL:
    ia = *reinterpret_cast<uint32_t*>(&reg_file_.Read(ra))
    shift = static_cast<int>(reg_file_.Read(rb))
    *reinterpret_cast<uint32_t*>(&result) = ia << shift
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### SHR — 按位右移

| 字段 | 值 |
|------|-----|
| Opcode | 0x1E |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra >> Rb（将 Ra 的 IEEE-754 bit pattern 右移 Rb 位）|

```
SHR:
    ia = *reinterpret_cast<uint32_t*>(&reg_file_.Read(ra))
    shift = static_cast<int>(reg_file_.Read(rb))
    *reinterpret_cast<uint32_t*>(&result) = ia >> shift
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### NOT — 按位取反

| 字段 | 值 |
|------|-----|
| Opcode | 0x1F |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = ~Ra（IEEE-754 float bit pattern 按位取反）|

```
NOT:
    ia = *reinterpret_cast<uint32_t*>(&reg_file_.Read(ra))
    *reinterpret_cast<uint32_t*>(&result) = ~ia
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### FLOOR — 向下取整

| 字段 | 值 |
|------|-----|
| Opcode | 0x20 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = floor(Ra) |

```
FLOOR:
    reg_file_.Write(rd, std::floor(reg_file_.Read(ra)))
    pc_.addr += 4
```

---

#### CEIL — 向上取整

| 字段 | 值 |
|------|-----|
| Opcode | 0x21 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = ceil(Ra) |

```
CEIL:
    reg_file_.Write(rd, std::ceil(reg_file_.Read(ra)))
    pc_.addr += 4
```

---

#### ABS — 绝对值

| 字段 | 值 |
|------|-----|
| Opcode | 0x22 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = abs(Ra) |

```
ABS:
    reg_file_.Write(rd, std::fabs(reg_file_.Read(ra)))
    pc_.addr += 4
```

---

#### NEG — 取负

| 字段 | 值 |
|------|-----|
| Opcode | 0x23 |
| 类型 | U-type |
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | Rd = -Ra（算术取负）|

```
NEG:
    reg_file_.Write(rd, -reg_file_.Read(ra))
    pc_.addr += 4
```

---

#### SMOOTHSTEP — Hermite 平滑插值

| 字段 | 值 |
|------|-----|
| Opcode | 0x24 |
| 类型 | R4-type |
| 操作数 | Rd, Ra(edge0), Rb(edge1), Rc(x) |
| 执行周期 | 1 |
| 功能 | GLSL 风格 smoothstep：若 x < edge0 → 0；若 x > edge1 → 1；否则执行 Hermite 插值 t²(3-2t) |

```
SMOOTHSTEP:
    edge0 = reg_file_.Read(ra)
    edge1 = reg_file_.Read(rb)
    x = reg_file_.Read(inst.GetRc())
    if edge1 == edge0:
        result = 0.0f
    else if x < edge0:
        result = 0.0f
    else if x > edge1:
        result = 1.0f
    else:
        t = (x - edge0) / (edge1 - edge0)
        result = t * t * (3.0f - 2.0f * t)
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

#### DP3 — 三分量点积

| 字段 | 值 |
|------|-----|
| Opcode | 0x25 |
| 类型 | R-type |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = dot(Ra.xyz, Rb.xyz) = Ra.x×Rb.x + Ra.y×Rb.y + Ra.z×Rb.z |
| 约束 | Ra, Rb 必须为 4-aligned（即 Ra%4==0, Rb%4==0），读取 Ra,Ra+1,Ra+2 和 Rb,Rb+1,Rb+2 |

```
DP3:
    v0 = reg_file_.Read(ra)       // Ra   = x
    v1 = reg_file_.Read(ra + 1)  // Ra+1 = y
    v2 = reg_file_.Read(ra + 2)  // Ra+2 = z
    r0 = reg_file_.Read(rb)       // Rb   = x
    r1 = reg_file_.Read(rb + 1)  // Rb+1 = y
    r2 = reg_file_.Read(rb + 2)  // Rb+2 = z
    result = v0*r0 + v1*r1 + v2*r2
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

## 第5章：执行周期汇总

| # | 指令 | Opcode | 类型 | 周期 |
|---|------|--------|------|------|
| 1 | NOP | 0x00 | J | 1 |
| 2 | ADD | 0x01 | R | 1 |
| 3 | SUB | 0x02 | R | 1 |
| 4 | MUL | 0x03 | R | 1 |
| 5 | DIV | 0x04 | R | **7** |
| 6 | MAD | 0x05 | R4 | 1 |
| 7 | RCP | 0x06 | U | 1 |
| 8 | SQRT | 0x07 | U | 1 |
| 9 | RSQ | 0x08 | U | 1 |
| 10 | AND | 0x09 | R | 1 |
| 11 | OR | 0x0A | R | 1 |
| 12 | CMP | 0x0B | R | 1 |
| 13 | SEL | 0x0C | R4 | 1 |
| 14 | MIN | 0x0D | R | 1 |
| 15 | MAX | 0x0E | R | 1 |
| 16 | LD | 0x0F | I | 1 |
| 17 | ST | 0x10 | I | 1 |
| 18 | BRA | 0x11 | B | 1 |
| 19 | JMP | 0x12 | J | 1 |
| 20 | CALL | 0x13 | J | 1 |
| 21 | RET | 0x14 | J | 1 |
| 22 | MOV | 0x15 | U | 1 |
| 23 | F2I | 0x16 | U | 1 |
| 24 | I2F | 0x17 | U | 1 |
| 25 | FRACT | 0x18 | U | 1 |
| 26 | TEX | 0x19 | R4 | **8** |
| 27 | SAMPLE | 0x1A | R4 | **4** |
| 28 | LDC | 0x1B | U | 1 |
| 29 | BAR | 0x1C | J | 1 |
| 30 | SHL | 0x1D | R | 1 |
| 31 | SHR | 0x1E | R | 1 |
| 32 | NOT | 0x1F | U | 1 |
| 33 | FLOOR | 0x20 | U | 1 |
| 34 | CEIL | 0x21 | U | 1 |
| 35 | ABS | 0x22 | U | 1 |
| 36 | NEG | 0x23 | U | 1 |
| 37 | SMOOTHSTEP | 0x24 | R4 | 1 |
| 38 | DP3 | 0x25 | R | 1 |

**总计：38 条指令**，长延迟指令包括：TEX(8-cycle)、DIV(7-cycle)、SAMPLE(4-cycle)、MAT_MUL(4-cycle)、VOUTPUT(2-cycle)、VLOAD(2-cycle)。

---

## 第6章：指令编码表

| Opcode (hex) | 名称 | 类型 | 周期 |
|-------------|------|------|------|
| 0x00 | NOP | J | 1 |
| 0x01 | ADD | R | 1 |
| 0x02 | SUB | R | 1 |
| 0x03 | MUL | R | 1 |
| 0x04 | DIV | R | 7 |
| 0x05 | MAD | R4 | 1 |
| 0x06 | RCP | U | 1 |
| 0x07 | SQRT | U | 1 |
| 0x08 | RSQ | U | 1 |
| 0x09 | AND | R | 1 |
| 0x0A | OR | R | 1 |
| 0x0B | CMP | R | 1 |
| 0x0C | SEL | R4 | 1 |
| 0x0D | MIN | R | 1 |
| 0x0E | MAX | R | 1 |
| 0x0F | LD | I | 1 |
| 0x10 | ST | I | 1 |
| 0x11 | BRA | B | 1 |
| 0x12 | JMP | J | 1 |
| 0x13 | CALL | J | 1 |
| 0x14 | RET | J | 1 |
| 0x15 | MOV | U | 1 |
| 0x16 | F2I | U | 1 |
| 0x17 | I2F | U | 1 |
| 0x18 | FRACT | U | 1 |
| 0x19 | TEX | R4 | **8** |
| 0x1A | SAMPLE | R4 | **4** |
| 0x1B | LDC | U | 1 |
| 0x1C | BAR | J | 1 |
| 0x1D | SHL | R | 1 |
| 0x1E | SHR | R | 1 |
| 0x1F | NOT | U | 1 |
| 0x20 | FLOOR | U | 1 |
| 0x21 | CEIL | U | 1 |
| 0x22 | ABS | U | 1 |
| 0x23 | NEG | U | 1 |
| 0x24 | SMOOTHSTEP | R4 | 1 |
| 0x25 | DP3 | R | 1 |
| 0x26 | VOUTPUT | — | 2 |
| 0x27 | VPOINT_SIZE | — | 1 |
| 0x28 | MAT_MUL | — | 4 |
| 0x29 | VLOAD | I | 2 |
| 0x2A | HALT | J | 1 |

---

## 第7章：伪代码执行框架

### 7.1 Interpreter 伪代码

```
Interpreter.Step():
    // 1. 排空已完成的 DIV 结果（每个 cycle 开始时检查）
    current_cycle = stats_.cycles
    for each pending in m_pending_divs:
        if pending.completion_cycle <= current_cycle:
            reg_file_.Write(pending.rd, pending.result)
            remove pending from m_pending_divs

    // 2. Fetch（从 I-cache 取指，此处简化）
    instruction_word = Fetch(pc_.addr)
    inst = Instruction(instruction_word)

    // 3. Decode
    op = inst.GetOpcode()
    if op == INVALID:
        return false  // 停止执行

    // 4. Execute
    ExecuteInstruction(inst)

    // 5. 推进周期
    stats_.cycles++
    return (op != RET)
```

### 7.2 周期计费说明

- 每调用一次 `Step()`，`stats_.cycles` 增加 1
- DIV 指令在 `ExecuteInstruction` 结束后即 `pc_.addr += 4`（不阻塞），但其结果写入被延迟 DIV_LATENCY=7 个 `stats_.cycles` 周期
- `drainPendingDIVs()` 在每个 `Step()` 开始时被调用，将已完成的 DIV 结果写入寄存器文件

---

## 第8章：指令功能分类索引

### 8.1 按功能分类

**控制流**：NOP, JMP, BRA, CALL, RET  
**算术**：ADD, SUB, MUL, DIV, MAD, RCP, SQRT, RSQ  
**位操作**：AND, OR, SHL, SHR, NOT  
**比较/选择**：CMP, SEL, MIN, MAX  
**数据转换**：MOV, F2I, I2F, FRACT, FLOOR, CEIL, ABS, NEG  
**数学扩展**：SMOOTHSTEP, DP3  
**内存**：LD, ST  
**纹理采样**：TEX, SAMPLE  
**常量/同步**：LDC, BAR  

### 8.2 按执行周期分类

**1-cycle 指令（33条）**：NOP, ADD, SUB, MUL, AND, OR, CMP, MIN, MAX, LD, ST, JMP, CALL, RET, MOV, F2I, I2F, FRACT, LDC, BAR, NOT, FLOOR, CEIL, ABS, NEG, SMOOTHSTEP, DP3, VPOINT_SIZE, BRA, RCP, SQRT, RSQ  
**2-cycle 指令（2条）**：VOUTPUT, VLOAD  
**4-cycle 指令（2条）**：SAMPLE, MAT_MUL  
**7-cycle 指令（1条）**：DIV  
**8-cycle 指令（1条）**：TEX  

### 8.3 按指令类型分类

**R-type（11条）**：ADD, SUB, MUL, DIV, AND, OR, CMP, MIN, MAX, SHL, SHR  
**R4-type（5条）**：MAD, SEL, TEX, SAMPLE, SMOOTHSTEP  
**U-type（12条）**：RCP, SQRT, RSQ, MOV, F2I, I2F, FRACT, LDC, NOT, FLOOR, CEIL, ABS, NEG  
**I-type（2条）**：LD, ST  
**B-type（1条）**：BRA  
**J-type（5条）**：NOP, JMP, CALL, RET, BAR  

---

## 第9章：Vertex ISA 扩展

> 本章内容由原始文档保留，未变动。

### 9.1 概述

Vertex ISA 是 SoftGPU 执行模型中用于顶点着色器（Vertex Shader）阶段的指令集扩展。Vertex Shader 负责将顶点数据从对象空间（Object Space）经由模型空间（Model Space）、世界空间（World Space）变换至裁剪空间（Clip Space），是 GPU 可编程管线的第一阶段。

SoftGPU Vertex ISA 在现有通用 ISA 基础上，新增三类专用指令，分别用于：**输出裁剪坐标**（VOUTPUT）、**输出点大小**（VPOINT_SIZE）和 **4×4 矩阵乘法加速**（MAT_MUL）。这些指令与 Fragment Shader ISA 共享同一套寄存器文件和执行单元调度框架，但存在若干关键差异（详见 9.5 节）。

#### 9.1.1 新增指令汇总

| 操作码 | 名称 | 功能 |
|--------|------|------|
| 0x26 | VOUTPUT | 输出裁剪坐标到 Rasterizer |
| 0x27 | VPOINT_SIZE | 输出点大小 |
| 0x28 | MAT_MUL | 4×4 矩阵乘法（column-major）|
| 0x29 | VLOAD | 从 Vertex Buffer 加载顶点属性 |
| 0x2A | HALT | 终止程序执行 |

---

### 9.2 指令规格

#### 9.2.1 VOUTPUT — 输出裁剪坐标

**操作码：** `0x26`

**功能：** 将计算所得的裁剪坐标 `(x, y, z, w)` 输出至 Rasterizer 阶段的输入寄存器。执行此指令后，当前进度计数器推进到下一个顶点（每 4 个 float32 存储一个顶点分量，按 x→y→z→w 顺序排列于 `VOUTPUTBUF`）。

**指令格式：**

```
VOUTPUT  Rd, #offset
```

| 字段 | 说明 |
|------|------|
| Rd | 源寄存器，存放 4 个 float32 的裁剪坐标 |
| #offset | 可选立即数偏移（对齐到 4 字节，默认 0）|

**操作数约束：**
- Rd 必须是 `VREG[0..255]`，内容布局为 `{x, y, z, w}`（float32×4）
- `VOUTPUTBUF` 是 VOUTPUT 指令专用的物理输出缓冲区，共 256 字节（可容纳最多 16 个完整顶点）

**执行周期：** 固定 2 周期（不接受跨周期 stall 优化）

**执行单元：** `EU_VTX`，专属顶点执行单元，不可与其他 EU 混合调度

**行为描述：**

```
// 伪代码
VOUTPUT(Rd):
    for i in 0..3:
        VOUTPUTBUF[vertex_idx * 4 + i] = Rd.f[i]
    vertex_idx += 1
    EU_VTX.bubble(1)   // 强制插入 1 个 bubble cycle
```

**注意：** VOUTPUT 是 **终结指令**（terminating instruction）。每个顶点着色器程序 **必须** 以 VOUTPUT 结尾，缺少 VOUTPUT 将导致 Rasterizer 无法接收几何数据，渲染管线挂起。

---

#### 9.2.2 VPOINT_SIZE — 输出点大小

**操作码：** `0x27`

**功能：** 将点精灵（point sprite）的点大小输出到 Rasterizer。当渲染图元类型为 `POINTS` 时，Rasterizer 根据此值决定每个 fragment 的覆盖范围。

**指令格式：**

```
VPOINT_SIZE  Rs
```

| 字段 | 说明 |
|------|------|
| Rs | 源寄存器，存放点大小（float32）|

**操作数约束：**
- Rs 必须是 `VREG[0..255]`，其 `.f[0]` 字段为点大小值（像素单位）
- 若从未执行 VPOINT_SIZE，默认点大小为 **1.0**

**执行周期：** 固定 1 周期

**执行单元：** `EU_VTX`（与 VOUTPUT 共用 EU，但可与之并行发射）

**行为描述：**

```
// 伪代码
VPOINT_SIZE(Rs):
    POINT_SIZE_BUFFER[vertex_idx] = Rs.f[0]
```

**与 VOUTPUT 的时序关系：**
- VPOINT_SIZE 可出现在 VOUTPUT 之前任意位置
- 典型模式：`... → VPOINT_SIZE → VOUTPUT`
- 不可出现在 VOUTPUT 之后（VOUTPUT 之后 Rasterizer 已接管，无意义）

---

#### 9.2.3 VLOAD — 从 Vertex Buffer 加载顶点属性

**操作码：** `0x29`

**功能：** 从 Vertex Buffer（VBO）中按偏移量加载顶点属性（一个顶点含多个分量，按顺序排列），将数据写入 Rd 开始的连续寄存器。典型用法见 9.3.2 程序模板。

**指令格式：**

```
VLOAD  Rd, #byte_offset
```

| 字段 | 说明 |
|------|------|
| Rd | 目标寄存器起始编号（写入 Rd, Rd+1, Rd+2, Rd+3 共4个 float32）|
| #byte_offset | 相对于 VBO 基址的字节偏移量（4字节对齐）|

**操作数约束：**
- Rd 必须是 4-aligned（Rd % 4 == 0），因为一次加载 4 个 float32
- byte_offset 必须是 4 的倍数

**执行周期：** 固定 **2 周期**（使用 EU_MEM 单元，可与 EU_VTX 并行发射）

**执行单元：** `EU_MEM`（独立于 EU_VTX）

**行为描述：**

```
// 伪代码
VLOAD(Rd, byte_offset):
    vbo_base = get_vbo_base()
    for i in 0..3:
        addr = vbo_base + byte_offset + i * 4
        value = memory_.Load32(addr)
        reg_file_.Write(rd + i, value)
    stats_.loads++
```

> **与 LD 指令的区别**：VLOAD 专用于 Vertex Shader 阶段，从 VBO 按顶点流顺序加载；LD 是通用内存访问指令，可用于任意地址。

---

#### 9.2.4 HALT — 终止程序执行

**操作码：** `0x2A`

**功能：** 终止当前 Shader 程序的执行。Fragment Shader 或 Vertex Shader 程序以此指令结尾。

**指令格式：**

```
HALT
```

| 字段 | 说明 |
|------|------|
| 无 | 无操作数 |

**执行周期：** 1

**执行单元：** 通用（解释器级停止）

**行为描述：**

```
// 伪代码
HALT:
    running_ = false
    return false  // 解释器主循环停止
```

> **注意**：HALT 是显式程序终止指令。Vertex Shader 程序也可使用 `HALT` 代替 `RET` 返回来结束程序。Fragment Shader 正常执行完所有指令后自然结束（无需 HALT）。

---

#### 9.2.5 MAT_MUL — 4×4 矩阵乘法

**操作码：** `0x28`

**功能：** 执行 4×4 矩阵与四维向量的乘法（`result = M × v`），或两个 4×4 矩阵相乘（`C = A × B`）。SoftGPU 采用 **column-major** 存储约定，因此矩阵元素在寄存器中的物理布局为：

```
M = | m0  m4  m8  m12 |   → VREG 内容: {m0, m4, m8, m12, m1, m5, m9, m13, ...}
    | m1  m5  m9  m13 |
    | m2  m6  m10 m14 |
    | m3  m7  m11 m15 |
```

**指令格式（向量形式）：**

```
MAT_MUL_V  Rd, Rm, Rv    ; Rd = Rm × Rv
```

| 字段 | 说明 |
|------|------|
| Rd | 目标寄存器，存放结果向量 |
| Rm | 4×4 矩阵寄存器（16 个 float32）|
| Rv | 输入四维向量寄存器 |

**指令格式（矩阵形式）：**

```
MAT_MUL_M  Rd, Ra, Rb    ; Rd = Ra × Rb
```

| 字段 | 说明 |
|------|------|
| Rd | 目标寄存器，存放结果 4×4 矩阵 |
| Ra | 左手侧 4×4 矩阵寄存器 |
| Rb | 右手侧 4×4 矩阵寄存器 |

**操作数约束：**
- Rd, Rm, Rv/Ra, Rb 必须是不同的 `VREG[0..255]`
- 矩阵寄存器跨 4 个连续 VREG 槽位（Rd 占用 VREG[d], VREG[d+1], VREG[d+2], VREG[d+3]）
- 指令流水中，Rd 的物理 VREG 编号必须与 Rm/Ra 的物理编号不重叠

**执行周期：** 固定 **4 周期**（接受跨指令调度优化，见 9.5.3）

**执行单元：** `EU_VTX`（矩阵乘法专用流水线）

**行为描述（向量形式）：**

```
// 伪代码：result = M × v
MAT_MUL_V(Rd, Rm, Rv):
    // M 为 column-major: M[j*4+i] = Rm[j].f[i]
    for i in 0..3:
        sum = 0.0
        for j in 0..3:
            sum += Rm[j].f[i] * Rv.f[j]
        Rd.f[i] = sum
    // 4-cycle latency: 中间 2 个周期不可读取 Rd
```

**数学背景（column-major）：**

Column-major 存储意味着矩阵的列连续排列。对于变换：

```
| a  b  c  d |     | x |
| e  f  g  h | ×   | y |
| i  j  k  l |     | z |
| m  n  o  p |     | w |
```

向量 v = (x, y, z, w)^T，矩阵 M = [col0 col1 col2 col3]，结果 r = M × v：

```
r.x = col0.x * x + col1.x * y + col2.x * z + col3.x * w
r.y = col0.y * x + col1.y * y + col2.y * z + col3.y * w
r.z = col0.z * x + col1.z * y + col2.z * z + col3.z * w
r.w = col0.w * x + col1.w * y + col2.w * z + col3.w * w
```

**时序图：**

```
Cycle:  1    2    3    4    5
MAT_MUL: [I] [I] [I] [I] [O]
          └───────────────┘
            4-cycle latency
```

---

### 9.3 Vertex Shader 执行模型

#### 9.3.1 整体架构

Vertex Shader 执行模型与 Fragment Shader（参考第 8 章）共享同一套 **EU_VTX 执行单元** 和 **VREG 寄存器文件**，但在调度粒度、资源分配和数据流方向上存在根本差异：

| 维度 | Vertex Shader | Fragment Shader |
|------|--------------|-----------------|
| 输入数据源 | Vertex Buffer（VBO）| Rasterizer（光栅化插值）|
| 输出数据目标 | Rasterizer | Framebuffer（FB）|
| 并行粒度 | 顶点级（每线程独立顶点）| 片段级（每线程独立片段）|
| 线程束（Warp）| 32 顶点/线程束 | 32 片段/线程束 |
| 典型指令密度 | 低（主要数学运算）| 高（条件分支+插值）|
| 内存访问模式 | 顺序读 VBO | 散射写 FB |

#### 9.3.2 顶点着色器程序结构

一个完整的 Vertex Shader 程序由以下语义段组成：

```
; ===== Vertex Shader ISA Program Template =====
.entry:
    ; 1. 从 Vertex Buffer 加载属性
    VLOAD   R0, #0              ; 加载位置属性 (x,y,z,w)
    VLOAD   R1, #16             ; 加载法线属性 (nx,ny,nz)
    VLOAD   R2, #32             ; 加载纹理坐标 (u,v)

    ; 2. 执行 MVP 矩阵变换
    ; 假设矩阵已预加载到 M_MAT, V_MAT, P_MAT
    MAT_MUL_V  T0, M_MAT, R0    ; T0 = Model × position
    MAT_MUL_V  T1, V_MAT, T0    ; T1 = View × T0
    MAT_MUL_V  T2, P_MAT, T1    ; T2 = Projection × T1  (= clip pos)

    ; 3. 可选：法线变换（仅需 3×3 旋转部分）
    ; MAT_MUL_V  T3, N_MAT, R1  ; T3 = Normal matrix × normal

    ; 4. 可选：输出点大小
    VPOINT_SIZE  R_SIZE         ; 输出 gl_PointSize = 8.0

    ; 5. 输出到 Rasterizer（终结指令）
    VOUTPUT  T2, #0
.exit:
    HALT
```

#### 9.3.3 VOUTPUTBUF 与 Rasterizer 接口

`VOUTPUTBUF` 是 EU_VTX 与 Rasterizer 之间的物理握手缓冲区：

```
VOUTPUTBUF[0..63]   → Vertex 0: (x,y,z,w) × 4 × 4字节 = 16 字节
VOUTPUTBUF[64..127]  → Vertex 1: (x,y,z,w)
VOUTPUTBUF[192..255] → Vertex 3
...
```

当 `vertex_idx` 达到配置阈值（默认 128 个顶点，或程序末尾），VOUTPUTBUF 整体 flush 到 Rasterizer 输入FIFO，触发光栅化阶段。

**Rasterizer 接收的数据格式：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `clip_x` | float32 | 裁剪空间 X 坐标 |
| `clip_y` | float32 | 裁剪空间 Y 坐标 |
| `clip_z` | float32 | 裁剪空间 Z 坐标（透视除前）|
| `clip_w` | float32 | 裁剪空间 W 坐标（透视除数）|
| `point_size` | float32 | 点大小（仅 POINTS 图元）|

---

### 9.4 手写 ISA 程序模板：MVP 变换示例

```
; ============================================================
; SoftGPU Vertex Shader ISA — MVP Transform Example
; 输入:   Vertex Buffer 中每个顶点含 3 个属性（位置、法线、UV）
;         模型矩阵(M)、视图矩阵(V)、投影矩阵(P) 常量已加载
; 输出:   裁剪坐标到 Rasterizer
; 图元类型: TRIANGLE（VOUTPUT 触发光栅化）
; ============================================================

.alias  VTX_POS    R0      ; 顶点位置 (x,y,z,w)
.alias  VTX_NRM    R1      ; 顶点法线 (nx,ny,nz)
.alias  VTX_UV     R2      ; 顶点纹理坐标 (u,v)
.alias  TEMP0      R3      ; 临时向量
.alias  TEMP1      R4      ; 临时向量
.alias  CLIP_POS  R5      ; 裁剪坐标（VOUTPUT 输入）

.alias  M_MAT      R8      ; Model 矩阵（占 R8-R11）
.alias  V_MAT      R12     ; View 矩阵（占 R12-R15）
.alias  P_MAT      R16     ; Projection 矩阵（占 R16-R19）

; --- 程序入口 ---
.entry:
    ; [阶段1] 从 Vertex Buffer 加载顶点属性
    ; VBO base 地址由 drawcall 参数指定，此处假设 #0 偏移为位置
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

; --- 矩阵数据定义（运行时由 driver 填充到对应 VREG） ---
.data
M_MAT_DATA:
    ; Column-major 4×4 单位矩阵示例（替换为实际模型矩阵）
    .float  1, 0, 0, 0   ; col0
    .float  0, 1, 0, 0   ; col1
    .float  0, 0, 1, 0   ; col2
    .float  0, 0, 0, 1   ; col3

V_MAT_DATA:
    .float  1, 0, 0, 0
    .float  0, 1, 0, 0
    .float  0, 0, 1, 0
    .float  0, 0, 0, 1

P_MAT_DATA:
    .float  1, 0, 0, 0
    .float  0, 1, 0, 0
    .float  0, 0, 1, 0
    .float  0, 0, 0, 1
```

#### 9.4.1 时序分析（单顶点执行）

```
Cycle   1      2      3      4      5      6      7      8      9
VLOAD   [====] [====]
MAT_MUL       [====] [====] [====] [====] [O]              ; MVP×3
MAT_MUL                                           [====] [====] [====] [====] [O]
VOUTPUT                                                          [====] [====]
```

**关键观察：**
- 3 次矩阵乘法各需 4 周期，但由于是数据流式链接（上一条 MAT_MUL 的输出直接作为下一条输入），总延迟 = 3 × 4 + overhead = **12 周期（可流水线化至 ~5 周期有效吞吐）**
- VOUTPUT 在最后一个 MAT_MUL 完成后立即发射，无需等待

---

### 9.5 指令调度约束

#### 9.5.1 EU_VTX 调度规则

1. **VOUTPUT 和 VPOINT_SIZE** 可在同一个 Cycle 发射到 EU_VTX（端口不同）
2. **MAT_MUL** 占用 EU_VTX 矩阵流水线 4 个周期，期间同一线程的 EU_VTX 其他端口 **不可发射新指令**（但其他线程的 EU_VTX 不受影响）
3. **VLOAD** 使用 EU_MEM 单元（独立于 EU_VTX），可与 MAT_MUL **并行**发射

#### 9.5.2 寄存器占用约束

| 指令 | 读寄存器 | 写寄存器 | 生命周期（cycle）|
|------|---------|---------|-----------------|
| VLOAD | — | Rd | 2 |
| MAT_MUL_V | Rm, Rv | Rd | 4 |
| VPOINT_SIZE| Rs | — | 1 |
| VOUTPUT | Rd | VOUTPUTBUF | 2 |

**寄存器重命名：** SoftGPU 编译器负责插入重命名以消除 WAR/WAW hazard。当无法重命名时，插入 `NOP` bubble。

#### 9.5.3 矩阵乘法吞吐优化

对于连续顶点执行同一 MVP 变换的场景，矩阵寄存器（M_MAT, V_MAT, P_MAT）在相邻顶点间 **不变化**，编译器应检测到此 pattern 并允许 MAT_MUL 流水线化：

```
; 顶点 0
MAT_MUL_V  T0, M_MAT, R0
MAT_MUL_V  T1, V_MAT, T0
MAT_MUL_V  CLIP0, P_MAT, T1
VOUTPUT  CLIP0

; 顶点 1（编译器检测到矩阵寄存器相同，发射间隔可重叠）
; MAT_MUL_V T0, M_MAT, R1  ← 可在顶点0的MAT_MUL第4周期后立即发射
```

---

### 9.6 与 Fragment Shader ISA 的关键差异

| 特性 | Vertex Shader ISA | Fragment Shader ISA |
|------|-------------------|---------------------|
| 终结指令 | `VOUTPUT`（必须）| 无（FRAMEBUF_WRITE 等）|
| 输出缓冲区 | `VOUTPUTBUF` → Rasterizer | `FB_COLOR` / `FB_DEPTH`|
| 插值支持 | 无（输入未经插值）| 内置插值（LINEAR/Persp）|
| 纹理采样 | 可选（顶点纹理提取）| 主要操作（大量 SAMPLE）|
| 条件分支典型频率 | 低（几何计算分支少）| 高（逐像素光照分支）|
| 矩阵乘法指令 | 有（MAT_MUL）| 无（需软件循环展开）|
| 输出点大小 | 有（VPOINT_SIZE）| 无 |

---

### 9.7 验收标准

#### 9.7.1 功能验收

| ID | 测试项 | 验证方法 |
|----|--------|---------|
| VA-1 | VOUTPUT 正确输出 clip space (x,y,z,w) | 对比参考实现，误差 < 0.001 ulp |
| VA-2 | VPOINT_SIZE 正确传递点大小 | 渲染 POINTS 图元，测量像素覆盖 |
| VA-3 | MAT_MUL 向量形式：`result = M × v` 正确 | 与参考 BLAS 实现逐元素比对 |
| VA-4 | MAT_MUL 矩阵形式：`C = A × B` 正确 | 与参考 BLAS 实现逐元素比对 |
| VA-5 | Column-major 布局与 OpenGL 规范一致 | 使用标准 glMatrixLoadIdentity 验证 |
| VA-6 | MVP 串联变换结果与参考实现 一致 | 使用 5 组随机输入矩阵/向量，比对 |
| VA-7 | VOUTPUTBUF flush 触发 Rasterizer | 观察 Rasterizer FIFO接收信号 |
| VA-8 | 无 VOUTPUT 时程序行为（应拒绝编译或运行时错误）| 编译器检测 + 仿真器验证 |

#### 9.7.2 性能验收

| ID | 测试项 | 目标 |
|----|--------|------|
| VP-1 | 单顶点 MVP 变换延迟 | ≤ 20 周期（实测 < 16 周期）|
| VP-2 | MAT_MUL 4×4×vec 吞吐率 | 每周期完成 1 次（流水线）|
| VP-3 | VOUTPUT 带宽 | 256 字节 / 2周期 = 128 B/cycle |
| VP-4 | 寄存器重命名无 WAR/WAW hazard 导致 stall | 0 stall（编译器负责）|

#### 9.7.3 边界条件验收

| ID | 测试项 | 预期行为 |
|----|--------|---------|
| VB-1 | clip_w = 0 时 VOUTPUT 行为（透视除零）| 应产生除零标记，不崩溃 |
| VB-2 | clip坐标超出 NDC 范围（culling 测试）| Rasterizer 接收后正确裁剪 |
| VB-3 | 矩阵寄存器跨 VREG 边界（如 R62 作为 Rm）| 编译器拒绝（越界访问）|
| VB-4 | VOUTPUTBUF 溢出（超过 16 顶点）| 自动 flush 后继续写入 |
| VB-5 | MAT_MUL 中 Rm == Rd（原地操作）| 行为 undefined，编译器应拒绝 |

---

## 附录A：指令全集速查表

| Opcode | 名称 | 类型 | 周期 | 功能摘要 |
|--------|------|------|------|---------|
| 0x00 | NOP | J | 1 | 空操作 |
| 0x01 | ADD | R | 1 | 加法 |
| 0x02 | SUB | R | 1 | 减法 |
| 0x03 | MUL | R | 1 | 乘法 |
| 0x04 | DIV | R | 7 | 除法（长延迟）|
| 0x05 | MAD | R4 | 1 | 乘加 |
| 0x06 | RCP | U | 1 | 倒数 |
| 0x07 | SQRT | U | 1 | 平方根 |
| 0x08 | RSQ | U | 1 | 倒数平方根 |
| 0x09 | AND | R | 1 | 按位与 |
| 0x0A | OR | R | 1 | 按位或 |
| 0x0B | CMP | R | 1 | 比较（<）|
| 0x0C | SEL | R4 | 1 | 条件选择 |
| 0x0D | MIN | R | 1 | 最小值 |
| 0x0E | MAX | R | 1 | 最大值 |
| 0x0F | LD | I | 1 | 加载 |
| 0x10 | ST | I | 1 | 存储 |
| 0x11 | BRA | B | 1 | 条件分支 |
| 0x12 | JMP | J | 1 | 无条件跳转 |
| 0x13 | CALL | J | 1 | 函数调用 |
| 0x14 | RET | J | 1 | 函数返回 |
| 0x15 | MOV | U | 1 | 移动 |
| 0x16 | F2I | U | 1 | Float→Int 位转换 |
| 0x17 | I2F | U | 1 | Int→Float 位转换 |
| 0x18 | FRACT | U | 1 | 小数部分 |
| 0x19 | TEX | R4 | **8** | 纹理采样 |
| 0x1A | SAMPLE | R4 | **4** | 简化纹理采样 |
| 0x1B | LDC | U | 1 | 加载常量（stub）|
| 0x1C | BAR | J | 1 | Warp 同步（stub）|
| 0x1D | SHL | R | 1 | 左移 |
| 0x1E | SHR | R | 1 | 右移 |
| 0x1F | NOT | U | 1 | 按位取反 |
| 0x20 | FLOOR | U | 1 | 向下取整 |
| 0x21 | CEIL | U | 1 | 向上取整 |
| 0x22 | ABS | U | 1 | 绝对值 |
| 0x23 | NEG | U | 1 | 算术取负 |
| 0x24 | SMOOTHSTEP | R4 | 1 | Hermite 平滑插值 |
| 0x25 | DP3 | R | 1 | 三分量点积 |
| 0x26 | VOUTPUT | — | 2 | 输出裁剪坐标（Vertex）|
| 0x27 | VPOINT_SIZE | — | 1 | 输出点大小（Vertex）|
| 0x28 | MAT_MUL | — | 4 | 4×4 矩阵乘法（Vertex）|
| 0x29 | VLOAD | I | 2 | 从 Vertex Buffer 加载顶点属性 |
| 0x2A | HALT | J | 1 | 终止程序执行 |
