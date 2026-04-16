# SoftGPU Unified ISA Design Specification v2.5

**版本：** 2.5  
**作者：** 陈二虎（SoftGPU Architect Agent）  
**日期：** 2026-04-10  
**文档刷新：** 2026-04-16（小钻风，更新 VSTORE dual-word 实现细节；文档化 Known Issues）  
**状态：** **核实版 v2.5**（对照 isa_v2_5.hpp + interpreter_v2_5.hpp 核实；ATTR 自始为 Format-B（从未改变），SEL/SMOOTHSTEP 条件/x 参数自始来自 rd；修正 6 处历史遗留与代码不符：RET/CALL link register R63、分支描述、VSTORE format/cycle、MOV opcode 0x63 补录）  

---

## 第1章：设计背景与目标

### 1.1 为什么做这次重建

原 ISA（v1.0/v1.5）存在以下架构缺陷：

| 问题 | 根因 | 影响 |
|------|------|------|
| **bit5 路由 hack** | 4 条 VS 指令（HALT/VOUTPUT/MAT_MUL/VLOAD）占据 FS opcode 空间，需要精确匹配优先于 bit5 路由 | 解码器逻辑脆弱，新增指令容易踩坑 |
| **5-bit opcode 空间不足** | VS Phase 2 规划 45 条指令，但 VS pipeline 只有 32 个 base opcode（bit5=1） | Phase 2 物理上无法容纳 |
| **寄存器文件太小** | 64 个寄存器，VS 矩阵运算和 FS 多采样场景都压力较大 | 编译器寄存器分配受限 |
| **两级解码维护成本** | bit5 路由 + 精确匹配两套逻辑并行 | 解释器扩展容易出错 |

### 1.2 重建目标

**核心变更三件事：**

1. **8-bit opcode** → 256 个 opcode 值，彻底消除 bit5 路由 hack
2. **128 个通用寄存器** → 支持更大的寄存器分配空间，VS 矩阵运算不再局促
3. **统一 opcode 空间** → VS 和 FS 指令真正共存于同一解码框架，不再按 bit5 分流

**不变的原则：**

- 32-bit 固定指令长度（可混用单字/双字格式）
- RISC 风格，解释器友好
- 周期精确计费模型
- Warp/thread SIMT 执行模型

### 1.3 兼容性说明

**此版本与 v1.0/v1.5 不向后兼容。**

所有已编译的 shader blob、汇编代码、Interpreter 实现均需重写。这是一次完整的 ISA 版本切换，不是增量改良。

---

## 第2章：寄存器文件规格

### 2.1 Register File 规格

| 参数 | 值 |
|------|-----|
| 寄存器数量 | **128 个标量寄存器** |
| 寄存器宽度 | 32-bit float（IEEE 754 single precision） |
| 寄存器编号 | **R0 – R127** |
| 特殊寄存器 | R0 恒为 0.0f（zero register，硬件级硬连线）|
| 寻址方式 | **7-bit 直接寻址**（支持全部 128 个寄存器）|
| 物理实现 | 单一统一物理寄存器文件，VS/FS 共享 |

### 2.2 行为约束

- **R0 为硬件级硬连线到 0.0f**：无论写入什么值，Read(R0) 始终返回 0.0f。此为物理实现，非软件约定。
- **越界访问**：未定义，编译器负责保证不越界
- **数据类型**：所有寄存器均为 float32 bit pattern，整数操作通过 `reinterpret_cast<uint32_t&>` 实现

### 2.3 VS/FS 寄存器使用约定

由于 VS 和 FS 物理共享同一寄存器文件，通过编译期约定分配使用范围：

| 区间 | 用途 | 保留数量 |
|------|------|---------|
| R0 | Zero register | 1 |
| R1 – R31 | VS 工作寄存器（矩阵、顶点属性等）| 31 |
| R32 – R63 | FS 工作寄存器（纹理坐标、颜色等）| 32 |
| R64 – R95 | VS 矩阵临时区 / 编译器分配的临时变量 | 32 |
| R96 – R127 | FS 扩展区 / 编译器分配的临时变量 | 32 |

> **注**：此为软约定，不排除编译器在需要时跨区使用。硬件层面 128 个寄存器完全平等。

---

## 第3章：指令编码格式

### 3.1 指令字长策略：单字 + 双字混用

由于 128 个寄存器需要 7-bit 寄存器字段，三寄存器格式在单字 32-bit 内无法容纳足够的立即数空间。因此引入**单字（1-cycle fetch）和双字（2-cycle fetch）两种格式**：

- **单字格式**：指令流中连续两条 32-bit 字，解释器一次 Fetch 两字
- **双字格式仅用于需要较大立即数的指令**：LD/ST（内存访问）、BRA/JUMP/CALL（控制流）、LDC（常量加载）
- **所有三寄存器 R-type 指令均为单字**：ADD/SUB/MUL/DIV/MAD/MAT_MUL 等

### 3.2 Format-A：R-type（单字，三寄存器）

```
31       24 23    17 16    10 9     3 2      0
+--------+--------+--------+--------+--------+
| Opcode |   Rd  |   Ra  |   Rb  |  Reserved(3bit) |
| 8 bit  | 7 bit | 7 bit | 7 bit |  000            |
+--------+--------+--------+--------+--------+
```

- **Opcode**：8-bit，0x00 – 0xFF
- **Rd**：7-bit，目标寄存器 R0 – R127
- **Ra**：7-bit，源寄存器 A
- **Rb**：7-bit，源寄存器 B
- **Reserved**：必须置 0

适用于：ADD, SUB, MUL, DIV, MAD, CMP, MIN, MAX, AND, OR, DOT3, DOT4, CROSS, NORMALIZE, SEL, SMOOTHSTEP, SAMPLE, TEX, POW 等。

> **R4-type 说明（MAD）**：Format-A 原本只有 Rd/Ra/Rb 三个寄存器字段。MAD 需要第四个寄存器 Rc，编码方式为**将 Rc 打包到 bits[9:5]**（5-bit，与 Rb 的 bits[9:3] 低位重叠）。因此 Rc 实际只能取 Rb 的低 5 位值（0–31），编译期必须确保所需 Rc 在此范围内。

### 3.3 Format-B：RI-type（双字，二寄存器 + 立即数）

```
Word 1:
31       24 23    17 16    10 9                      0
+--------+--------+--------+-------------------------+
| Opcode |   Rd  |   Ra  |        000000           |
| 8 bit  | 7 bit | 7 bit |        (10 bit)          |
+--------+--------+--------+-------------------------+

Word 2:
31       25 24             14 13                    0
+---------+-----------------+------------------------+
|  0000000 |  Rb/RegSel    |     Immediate          |
|  (7 bit) |    (7 bit)    |     (10 bit)           |
+---------+-----------------+------------------------+
```

- **Word 1**：Opcode + Rd + Ra（Ra 通常为 R0/zero register 表示"无"）
- **Word 2**：Rb/RegSel + 10-bit 立即数（符号扩展或零扩展取决于指令）
- **Fetch 周期**：2 个周期（连续取两字）
- **执行时机**：Word 2 fetch 完成后才能开始 EX

适用于：LD, ST, LDC, BRA, JUMP, CALL, VLOAD, MOV_IMM, OUTPUT, OUTPUT_VS 等。

> **10-bit Immediate 范围**：
> - 无符号：0 – 1023（适用于内存字节偏移）
> - 符号扩展：-512 – +511（适用于分支 offset，单位：4 字节，即 ±2KB）

### 3.4 Format-C：U-type（单字，单寄存器 + func）

```
31       24 23    17 16          8 7               0
+--------+--------+--------+--------+----------------+
| Opcode |   Rd  |   Func   | Reserved |
| 8 bit  | 7 bit | 9 bit   | 8 bit   |
+--------+--------+--------+--------+----------------+
```

适用于：ABS, NEG, FLOOR, CEIL, FRACT, SQRT, RSQ, RCP, SIN, COS, EXPD2, LOGD2, F2I, I2F, NOT, MOV 等单目操作（Func 字段当前未使用，为 Phase 2 CVT 族扩展预留）。

> **Func 字段**：当前设计（v2.5）中 U-type 指令（如 ABS/NEG/FLOOR/CEIL/MOV 等单目操作）不使用 Func 子操作码，每条指令有独立 opcode。若未来需要同一 opcode 下的多子操作（如 CVT 族的 signed/unsigned 变体），可在 Func 字段中编码，高 5-bit 填 0 保留给扩展。

### 3.5 Format-D：J-type（单字，无操作数控制流）

```
31       24 23                                        0
+--------+-------------------------------------------+
| Opcode |              Reserved (24 bit)             |
| 8 bit  |                   (24 bit)                |
+--------+-------------------------------------------+
```

适用于：NOP, RET, HALT, BAR 等。

### 3.6 Format-E：特殊双字格式（单寄存器 + word2 立即数）

```
Word 1:
31       24 23    17 16                                 0
+--------+--------+-------------------------------------+
| Opcode |   Rd  |           Reserved (17 bit)         |
| 8 bit  | 7 bit |              (17 bit)              |
+--------+--------+-------------------------------------+

Word 2:
31                                           10 9      0
+------------------------------------------+-----------+
|              Immediate (10 bit)           | Reserved |
|                                          | (22 bit)  |
+------------------------------------------+-----------+
```

- **Word 1**：Opcode + Rd（目标/源寄存器）+ Reserved
- **Word 2**：10-bit 立即数（字节偏移量，零扩展）+ Reserved
- **Fetch 周期**：2 个周期

适用于：VSTORE（将顶点属性存入 VATTR buffer）、OUTPUT_VS（VS 统一输出，与 Format-B OUTPUT 等价但用于明确区分 VS 上下文）。

---

## 第4章：统一 Opcode 空间分配（256 值）

### 4.1 总体划分

```
0x00 – 0x0F  ： 控制流 & 系统指令（VS/FS 共用）
0x10 – 0x1F  ： 统一算术 ALU（VS/FS 共用，无 _VS 变体）
0x20 – 0x2F  ： 统一特殊功能 SFU（VS/FS 共用，无 _VS 变体）
0x30 – 0x3F  ： 统一内存/纹理访问（VS/FS 共享，stage 由执行上下文决定）
0x40 – 0x5F  ： VS 专属指令（矩阵、VBO、顶点输出——语义与 FS 本质不同）
0x60 – 0x7F  ： 保留（Phase 2 扩展）
0x80 – 0xBF  ： 保留（Phase 2+ 扩展，64 个 opcode）
0xC0 – 0xFF  ： 特殊扩展 / 保留给未来
```

> **设计原则：** VS 和 FS 共享同一物理寄存器文件（128 个）和 ALU/SFU 执行单元。凡是语义相同的指令（如 ADD、SIN、ABS），使用同一 opcode，由各自的程序 PC 寻址执行。VS 专属指令（VLOAD/VOUTPUT/MAT_MUL 等）语义与 FS 本质不同，保留独立 opcode。
>
> **消除的冗余（v2.0 → v2.1）：** 原设计在 0x44-0x5F 存在大量 _VS 变体（ADD_VS、SUB_VS、SIN_VS 等），这些指令的语义与 0x10-0x2F 中的对应指令完全相同，仅因 VS/FS 程序分离而被不必要地复制。优化后共释放 **15 个 opcode**，全部移入 VS 专属区域用于真正的 VS 专用指令。
>
> **矩阵运算 lower（v2.1 → v2.2）：** MAT_MUL（0x40）、MAT_ADD（0x41）、MAT_SUB（0x43）从硬件指令集中删除，统一由编译器负责将矩阵运算 lower 为向量指令（4×DOT4 / 4×ADD / 4×SUB）。释放出的 3 个 opcode（0x40/0x41/0x43）留给 Phase 2 扩展。~~保留 MAT_TRANSPOSE（0x42）——无等价向量操作。~~
>
> **MAT_TRANSPOSE 删除（v2.2 → v2.3）：** MAT_TRANSPOSE（0x42）从硬件指令集中删除，opcode 0x42 释放为 Phase 2 保留。Phase 2 重新引入时将携带更优的微架构实现（可能为 2-cycle 或者与 VOUTPUT 共用硅片面积）。

### 4.2 Opcode 完整映射表

#### 控制流 & 系统指令（0x00 – 0x0F，VS/FS 共用）

| Opcode | 指令 | Format | 类型 | 周期 | 功能 |
|--------|------|--------|------|------|------|
| 0x00 | NOP | D | J | 1 | 空操作 |
| 0x01 | BRA | B(双字) | B | 1 | 条件分支：Ra≠0 则跳转（VS/FS 通用，原 CBR 废除）|
| 0x02 | CALL | B(双字) | J | 1 | 调用：保存返回地址到 **R63**，跳转 |
| 0x03 | RET | D | J | 1 | 返回：从 **R63** 恢复 PC |
| 0x04 | JMP | B(双字) | J | 1 | 无条件跳转 |
| 0x05 | BAR | D | J | 1 | Warp 内线程同步 |
| 0x06 – 0x0E | — | — | — | — | **保留** |
| 0x0F | HALT | D | J | 1 | 程序终止（VS/FS 共用） |

#### 统一算术 ALU（0x10 – 0x1F，VS/FS 共用）

| Opcode | 指令 | Format | 类型 | 周期 | 功能 |
|--------|------|--------|------|------|------|
| 0x10 | ADD | A | R | 1 | Rd = Ra + Rb |
| 0x11 | SUB | A | R | 1 | Rd = Ra - Rb |
| 0x12 | MUL | A | R | 1 | Rd = Ra × Rb |
| 0x13 | DIV | A | R | **7** | Rd = Ra / Rb（长延迟）|
| 0x14 | MAD | A | R | 1 | Rd = Ra × Rb + Rc（R4-type，Rc 编码于 bits[9:5]，= Rb[4:0]）|
| 0x15 | CMP | A | R | 1 | Rd = (Ra < Rb) ? 1.0f : 0.0f |
| 0x16 | MIN | A | R | 1 | Rd = min(Ra, Rb) |
| 0x17 | MAX | A | R | 1 | Rd = max(Ra, Rb) |
| 0x18 | AND | A | R | 1 | Rd = Ra & Rb（按位与）|
| 0x19 | OR | A | R | 1 | Rd = Ra \| Rb（按位或）|
| 0x1A | XOR | A | R | 1 | Rd = Ra ^ Rb（按位异或）|
| 0x1B | SHL | A | R | 1 | Rd = Ra << Rb（按位左移）|
| 0x1C | SHR | A | R | 1 | Rd = Ra >> Rb（按位右移）|
| 0x1D | SEL | A | R | 1 | Rd = (rd != 0) ? Ra : Rb（条件来自 rd）|
| 0x1E | SMOOTHSTEP | A | R | 1 | Rd = smoothstep(Ra, Rb, rd)（x 参数来自 rd）|
| 0x1F | SETP | A | R | 1 | 设置谓词寄存器 |

#### 统一特殊功能 SFU（0x20 – 0x2F，VS/FS 共用）

| Opcode | 指令 | Format | 类型 | 周期 | 功能 |
|--------|------|--------|------|------|------|
| 0x20 | RCP | C | U | 1 | Rd = 1.0f / Ra |
| 0x21 | SQRT | C | U | 1 | Rd = sqrt(Ra) |
| 0x22 | RSQ | C | U | 1 | Rd = 1.0f / sqrt(Ra) |
| 0x23 | SIN | C | U | 1 | Rd = sin(Ra) |
| 0x24 | COS | C | U | 1 | Rd = cos(Ra) |
| 0x25 | EXPD2 | C | U | 1 | Rd = exp2(Ra) |
| 0x26 | LOGD2 | C | U | 1 | Rd = log2(Ra) |
| 0x27 | POW | A | R | 1 | Rd = pow(Ra, Rb) |
| 0x28 | ABS | C | U | 1 | Rd = abs(Ra) |
| 0x29 | NEG | C | U | 1 | Rd = -Ra |
| 0x2A | FLOOR | C | U | 1 | Rd = floor(Ra) |
| 0x2B | CEIL | C | U | 1 | Rd = ceil(Ra) |
| 0x2C | FRACT | C | U | 1 | Rd = Ra - floor(Ra) |
| 0x2D | F2I | C | U | 1 | Rd = bitcast(float→int, Ra) |
| 0x2E | I2F | C | U | 1 | Rd = bitcast(int→float, Ra) |
| 0x2F | NOT | C | U | 1 | Rd = ~Ra（按位取反）|

#### 统一内存/纹理访问（0x30 – 0x3F，VS/FS 共享）

| Opcode | 指令 | Format | 类型 | 周期 | 功能 |
|--------|------|--------|------|------|------|
| 0x30 | LD | B(双字) | I | 2 | Rd = memory[Ra + imm] |
| 0x31 | ST | B(双字) | I | 2 | memory[Ra + imm] = Rb |
| 0x32 | TEX | A | R | **10+** | 纹理采样（SFU 级别延迟）|
| 0x33 | SAMPLE | A | R | **10+** | 简化纹理采样 |
| 0x34 | OUTPUT | B(双字) | I | 2 | **统一输出**：VS → 裁剪坐标输出到 Rasterizer；FS → 颜色输出到 Framebuffer（由执行上下文路由）|
| 0x35 – 0x3F | — | — | — | — | **保留** |

> **OUTPUT（0x34）统一设计**：原设计 VOUTPUT_FS（0x34）和 VOUTPUT_VS（0x56）分别占用不同 opcode，但两者格式和语义相似——都是将寄存器数据输出到固定管线阶段。统一为单一 opcode，由 EU 在执行时根据当前是 VS 上下文还是 FS 上下文路由到对应管线单元（原 VOUTPUT_VS opcode 0x56 废除）。

#### VS 专属指令（0x40 – 0x5F）

> **v2.4 变更说明**：VLOAD（0x49）从 Phase 2 候选移回 Phase 1，VS 程序需要 VLOAD 从 VBO 加载顶点属性才能开展 E2E 测试。DOT3（0x4E）和 DOT4（0x4F）补为独立 Phase 1 opcode（原为 Phase 1 指令列表中提到但 opcode 缺失）。MOV_IMM（0x48）补为 Phase 1 opcode（Chapter 5 原来缺少规格定义）。MAT_TRANSPOSE 已删除（v2.3），Phase 1 不支持矩阵转置，编译器需避免生成此类操作。
>
> **Phase 1 VS 限制**：Phase 1 不支持矩阵转置操作（MAT_TRANSPOSE 已于 v2.3 删除）。编译器需将矩阵转置 lower 为标量序列，或等待 Phase 2 重新引入带更优实现的版本。

| Opcode | 指令 | Format | 类型 | 周期 | 功能 |
|--------|------|--------|------|------|------|
| 0x40 | — | — | — | — | **保留（Phase 2 扩展，原 MAT_MUL 已删除）** |
| 0x41 | — | — | — | — | **保留（Phase 2 扩展，原 MAT_ADD 已删除）** |
| 0x42 | — | — | — | — | **保留（Phase 2 扩展，原 MAT_TRANSPOSE 已删除，v2.3）** |
| 0x43 | — | — | — | — | **保留（Phase 2 扩展，原 MAT_SUB 已删除）** |
| 0x44 | — | — | — | — | **保留（Phase 2 扩展）** |
| 0x45 | — | — | — | — | **保留（Phase 2 扩展）** |
| 0x46 | — | — | — | — | **保留（Phase 2 扩展）** |
| 0x47 | — | — | — | — | **保留（Phase 2 扩展）** |
| 0x48 | MOV_IMM | B(双字) | I | 2 | 将 10-bit 立即数（零扩展）写入 Rd |
| 0x49 | VLOAD | B(双字) | I | **1** | 从 VBO 加载顶点属性到 Rd–Rd+3（addr 4-byte aligned）|
| 0x4A | VSTORE | **E(双字)** | — | **1** | 存储顶点属性到 VATTR buffer（word2=byte_offset，rd=源寄存器）|
| 0x4B | OUTPUT_VS | B(双字) | I | 2 | 输出裁剪坐标到 Rasterizer（VS 终结指令，与 0x34 OUTPUT 等价）|
| 0x4C | LDC | B(双字) | I | 2 | VS 常量数据加载（Rd = const_data[imm]）|
| 0x4D | ATTR | B(双字) | I | 2 | 顶点属性提取（从 VBO 按 float index 加载单个分量到 Rd，word2=float_index）|
| 0x4E | DOT3 | A | R | 1 | 3D 点积：Rd = Ra·Rb（dot product of 3-component vectors，低 3 分量参与计算）|
| 0x4F | DOT4 | A | R | 1 | 4D 点积：Rd = Ra·Rb（dot product of 4-component vectors，4 分量全部参与计算）|
| 0x50 – 0x5F | — | — | — | — | **保留（Phase 2 扩展）** |
| **0x63** | **MOV** | **C** | **U** | **1** | **寄存器移动：Rd = Ra（Format-C 单字，v2.5 新增）** |
| 0x64 – 0xBF | — | — | — | — | **保留（Phase 2 扩展）** |

#### 特殊扩展（0xC0 – 0xFF）

| Opcode | 用途 |
|--------|------|
| 0xC0 | DEBUG_BREAK（调试断点）|
| 0xC1 | CYCLE_COUNT（读取 stats_.cycles 到寄存器）|
| 0xFE | EXTENDED（扩展指令前缀，secondary opcode 在 immediate 中）|
| 0xFF | RESERVED（保留）|

**Phase 2 候选扩展指令：**
- 0x40: 保留（原 MAT_MUL，已删除）
- 0x41: 保留（原 MAT_ADD，已删除）
- 0x42: 保留（原 MAT_TRANSPOSE，已删除，v2.3）
- 0x43: 保留（原 MAT_SUB，已删除）
- 0x44–0x47: 保留（Phase 2 扩展）
- 0x50–0x5F: 保留（Phase 2 扩展）
- 0x60: CVT_F32_S32
- 0x61: CVT_F32_U32
- 0x62: CVT_S32_F32
- 0x63: CVT_U32_F32（注意：v2.5 此处为 MOV，别名）
- 0x70: TEX_LOD（带细节层级参数的纹理采样）
- 0x80: FMA（Fused Multiply-Add，精确版）
- 0xC0 – 0xFF: 未来扩展（协处理器接口、调试指令等）

---

## 第5章：指令详细规格

### 5.1 NOP

| 字段 | 值 |
|------|------|
| Opcode | 0x00 |
| Format | D（J-type） |
| 操作数 | 无 |
| 执行周期 | 1 |
| 功能 | 空操作，PC + 4 |

```
NOP:
    // no-op
    pc_.addr += 4
```

---

### 5.2 HALT

| 字段 | 值 |
|------|------|
| Opcode | 0x0F |
| Format | D（J-type） |
| 操作数 | 无 |
| 执行周期 | 1 |
| 功能 | 终止程序执行，解释器主循环退出 |

```
HALT:
    running_ = false
    return false
```

> **注意**：VS 程序必须以 OUTPUT_VS（0x4B）或 OUTPUT（0x34）或 HALT（0x0F）结尾。FS 程序通常以 OUTPUT（0x34）输出颜色后自然结束。

---

### 5.3 ADD

| 字段 | 值 |
|------|------|
| Opcode | 0x10（VS/FS 统一）|
| Format | A（R-type） |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra + Rb |

```
ADD:
    reg_file_.Write(rd, reg_file_.Read(ra) + reg_file_.Read(rb))
    pc_.addr += 4
```

---

### 5.4 DIV

| 字段 | 值 |
|------|------|
| Opcode | 0x13（VS/FS 统一）|
| Format | A（R-type） |
| 操作数 | Rd, Ra, Rb |
| 执行周期 | **7**（DIV_LATENCY = 7）|
| 功能 | Rd = Ra / Rb；结果延迟 7 个周期写入寄存器 |

```
DIV:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    result = (val_b != 0.0f) ? (val_a / val_b) : infinity
    pending.rd = rd
    pending.result = result
    pending.completion_cycle = stats_.cycles + DIV_LATENCY
    m_pending_divs.push_back(pending)
    pc_.addr += 4
```

---

### 5.5 SUB

| 字段 | 值 |
|------|------|
| Opcode | 0x11（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra - Rb |

```
SUB:
    reg_file_.Write(rd, reg_file_.Read(ra) - reg_file_.Read(rb))
    pc_.addr += 4
```

---

### 5.6 MUL

| 字段 | 值 |
|------|------|
| Opcode | 0x12（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = Ra × Rb |

```
MUL:
    reg_file_.Write(rd, reg_file_.Read(ra) * reg_file_.Read(rb))
    pc_.addr += 4
```

---

### 5.7 MAD

| 字段 | 值 |
|------|------|
| Opcode | 0x14（VS/FS 统一）|
| Format | A（R-type，R4-type 语义）|
| 操作数 | Rd, Ra, Rb, Rc（Rc 编码于 bits[9:5]，= Rb[4:0]）|
| 执行周期 | 1 |
| 功能 | Rd = Ra × Rb + Rc |

> **R4-type 编码说明**：MAD 需要四个寄存器字段（Rd/Ra/Rb/Rc）。Format-A 编码中 Rb 占用 bits[9:3]（7-bit），Rc 打包到 bits[9:5]（5-bit），两者在 bits[9:5] 重叠。因此 Rc 实际值被约束为 Rb 的低 5 位（0–31）。编译期必须确保所需 Rc 在此范围内。若 Rc 需要超出此范围，请参见 Phase 2 扩展 FMA（0x80）。

```
MAD:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    val_c = reg_file_.Read(inst_.GetRc())  // Rc = bits[9:5] = Rb[4:0]
    result = val_a * val_b + val_c
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

### 5.8 CMP

| 字段 | 值 |
|------|------|
| Opcode | 0x15（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | Rd = (Ra < Rb) ? 1.0f : 0.0f（less-than 比较，结果为浮点）|

```
CMP:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, (val_a < val_b) ? 1.0f : 0.0f)
    pc_.addr += 4
```

---

### 5.9 MIN / MAX

| 字段 | 值 |
|------|------|
| MIN Opcode | 0x16 |
| MAX Opcode | 0x17 |
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | MIN: Rd = min(Ra, Rb)；MAX: Rd = max(Ra, Rb)（IEEE-754 语义，含 NaN 处理）|

```
MIN:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, fmin(val_a, val_b))  // IEEE-754 fmin
    pc_.addr += 4

MAX:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, fmax(val_a, val_b))  // IEEE-754 fmax
    pc_.addr += 4
```

---

### 5.10 AND / OR / XOR / SHL / SHR

| 字段 | 值 |
|------|------|
| AND Opcode | 0x18 |
| OR Opcode | 0x19 |
| XOR Opcode | 0x1A |
| SHL Opcode | 0x1B |
| SHR Opcode | 0x1C |
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1 |
| 功能 | 按位二进制运算（操作数按 float bit pattern 解释为 uint32_t）|

> **注意**：寄存器值 IEEE-754 float → 按位转换为 uint32_t 后运算，结果再按位写回寄存器（bit pattern 不变）。

```
AND:
    val_a = bitcast<float,uint32_t>(reg_file_.Read(ra))
    val_b = bitcast<float,uint32_t>(reg_file_.Read(rb))
    reg_file_.Write(rd, bitcast<uint32_t,float>(val_a & val_b))
    pc_.addr += 4

OR:
    val_a = bitcast<float,uint32_t>(reg_file_.Read(ra))
    val_b = bitcast<float,uint32_t>(reg_file_.Read(rb))
    reg_file_.Write(rd, bitcast<uint32_t,float>(val_a | val_b))
    pc_.addr += 4

XOR:
    val_a = bitcast<float,uint32_t>(reg_file_.Read(ra))
    val_b = bitcast<float,uint32_t>(reg_file_.Read(rb))
    reg_file_.Write(rd, bitcast<uint32_t,float>(val_a ^ val_b))
    pc_.addr += 4

SHL:
    val_a = bitcast<float,uint32_t>(reg_file_.Read(ra))
    shamt = static_cast<uint32_t>(reg_file_.Read(rb)) & 0x1F
    reg_file_.Write(rd, bitcast<uint32_t,float>(val_a << shamt))
    pc_.addr += 4

SHR:
    val_a = bitcast<float,uint32_t>(reg_file_.Read(ra))
    shamt = static_cast<uint32_t>(reg_file_.Read(rb)) & 0x1F
    reg_file_.Write(rd, bitcast<uint32_t,float>(val_a >> shamt))
    pc_.addr += 4
```

---

### 5.11 SEL（条件选择）

| 字段 | 值 |
|------|------|
| Opcode | 0x1D（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd（条件，同时也是结果写入目标）, Ra（true-value）, Rb（false-value）|
| 执行周期 | 1 |
| 功能 | Rd = (Rd != 0) ? Ra : Rb（条件取自 Rd 自身） |

> **语义说明**：SEL 读取 Rd 作为条件值。若 Rd ≠ 0 则结果为 Ra，否则为 Rb。Rd 同时也是结果写入目标寄存器（即条件寄存器必须和目标寄存器为同一物理寄存器）。此设计下编译器需确保将条件值置于 Rd，再以 Rd 作为目标寄存器写入结果。
>
> **硬件 ordering 保证**：同一指令周期内先读条件（Rd），再写结果（Rd），硬件保证先读后写语义，即使 Rd == Ra 或 Rd == Rb 也安全。

```
SEL:
    // 条件取自 Rd（同时也是结果写入目标）
    cond_val = reg_file_.Read(rd)
    true_val = reg_file_.Read(ra)
    false_val = reg_file_.Read(rb)
    result = (cond_val != 0.0f) ? true_val : false_val
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

### 5.12 SMOOTHSTEP

| 字段 | 值 |
|------|------|
| Opcode | 0x1E（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd（x 参数，同时也是结果写入目标）, Ra（edge0）, Rb（edge1）|
| 执行周期 | 1 |
| 功能 | Hermite 平滑插值：Rd = smoothstep(edge0, edge1, Rd)（x 取自 Rd 自身） |

> **语义说明**：SMOOTHSTEP 的 x 参数取自 Rd（即 Rd 同时也是结果写入目标），edge0 取自 Ra，edge1 取自 Rb。此设计与 SEL 类似：x/条件值必须放在与目标寄存器相同的寄存器中。
>
> **硬件 ordering 保证**：同一指令周期内先读 x（Rd）和 edge 值，再写 result，硬件保证先读后写语义。

```
SMOOTHSTEP:
    // x 取自 Rd（同时也是结果写入目标）
    edge0 = reg_file_.Read(ra)     // Ra = edge0
    edge1 = reg_file_.Read(rb)     // Rb = edge1
    x = reg_file_.Read(rd)         // Rd = x
    t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f)
    result = t * t * (3.0f - 2.0f * t)
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

---

### 5.13 SETP（谓词设置）

| 字段 | 值 |
|------|------|
| Opcode | 0x1F（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd（谓词结果）, Ra |
| 执行周期 | 1 |
| 功能 | 设置谓词寄存器：Rd = (Ra != 0) ? 1.0f : 0.0f（类似 CMP 但无条件，仅 Ra≠0 即为 true）|

> **注意**：SETP 与 CMP 的区别在于 CMP 执行 Ra < Rb（less-than），SETP 仅检查 Ra 是否非零（non-zero）。谓词寄存器用于后续 SEL/BRA 等指令的条件判断。

```
SETP:
    val_a = reg_file_.Read(ra)
    reg_file_.Write(rd, (val_a != 0.0f) ? 1.0f : 0.0f)
    pc_.addr += 4
```

---

### 5.14 RCP / SQRT / RSQ / SIN / COS / EXPD2 / LOGD2

| 字段 | 值 |
|------|------|
| RCP Opcode | 0x20 |
| SQRT Opcode | 0x21 |
| RSQ Opcode | 0x22 |
| SIN Opcode | 0x23 |
| COS Opcode | 0x24 |
| EXPD2 Opcode | 0x25 |
| LOGD2 Opcode | 0x26 |
| Format | C（U-type）|
| 操作数 | Rd, Ra |
| 执行周期 | 1（SFU 发射，不阻塞）|
| 功能 | 通用特殊函数单目操作（IEEE-754 语义，含 NaN/Inf 处理）|

```
RCP:
    reg_file_.Write(rd, 1.0f / reg_file_.Read(ra))
    pc_.addr += 4

SQRT:
    reg_file_.Write(rd, sqrtf(reg_file_.Read(ra)))
    pc_.addr += 4

RSQ:
    val = reg_file_.Read(ra)
    reg_file_.Write(rd, 1.0f / sqrtf(val))
    pc_.addr += 4

SIN:
    reg_file_.Write(rd, sinf(reg_file_.Read(ra)))
    pc_.addr += 4

COS:
    reg_file_.Write(rd, cosf(reg_file_.Read(ra)))
    pc_.addr += 4

EXPD2:
    reg_file_.Write(rd, exp2f(reg_file_.Read(ra)))
    pc_.addr += 4

LOGD2:
    reg_file_.Write(rd, log2f(reg_file_.Read(ra)))
    pc_.addr += 4
```

---

### 5.15 POW

| 字段 | 值 |
|------|------|
| Opcode | 0x27（VS/FS 统一）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | 1（SFU 长延迟）|
| 功能 | Rd = pow(Ra, Rb)（指数运算，Ra 为底，Rb 为指数）|

```
POW:
    val_a = reg_file_.Read(ra)
    val_b = reg_file_.Read(rb)
    reg_file_.Write(rd, powf(val_a, val_b))
    stats_.sfu_ops++
    pc_.addr += 4
```

---

### 5.16 ABS / NEG / FLOOR / CEIL / FRACT

| 字段 | 值 |
|------|------|
| ABS Opcode | 0x28 |
| NEG Opcode | 0x29 |
| FLOOR Opcode | 0x2A |
| CEIL Opcode | 0x2B |
| FRACT Opcode | 0x2C |
| Format | C（U-type）|
| 操作数 | Rd, Ra |
| 执行周期 | 1 |
| 功能 | 通用单目浮点操作（IEEE-754 语义）|

```
ABS:
    reg_file_.Write(rd, fabsf(reg_file_.Read(ra)))
    pc_.addr += 4

NEG:
    reg_file_.Write(rd, -reg_file_.Read(ra))
    pc_.addr += 4

FLOOR:
    reg_file_.Write(rd, floorf(reg_file_.Read(ra)))
    pc_.addr += 4

CEIL:
    reg_file_.Write(rd, ceilf(reg_file_.Read(ra)))
    pc_.addr += 4

FRACT:
    val = reg_file_.Read(ra)
    reg_file_.Write(rd, val - floorf(val))
    pc_.addr += 4
```

---

### 5.17 F2I / I2F / NOT

| 字段 | 值 |
|------|------|
| F2I Opcode | 0x2D |
| I2F Opcode | 0x2E |
| NOT Opcode | 0x2F |
| Format | C（U-type）|
| 操作数 | F2I/I2F: Rd, Ra；NOT: Rd, Ra |
| 执行周期 | 1 |
| 功能 | 类型转换 & 按位取反 |

```
F2I:
    // float bit pattern → int bit pattern（reinterpret_cast，位级转换）
    val_f = reg_file_.Read(ra)
    val_i = reinterpret_cast<uint32_t&>(val_f)   // 取 float 的 IEEE-754 bit pattern
    reg_file_.Write(rd, reinterpret_cast<float&>(val_i))  // 将 bit pattern 写入 float 寄存器
    pc_.addr += 4

I2F:
    // int bit pattern → float bit pattern（reinterpret_cast，位级转换）
    val_i = reinterpret_cast<uint32_t&>(reg_file_.Read(ra))  // 取 int 的 bit pattern
    reg_file_.Write(rd, reinterpret_cast<float&>(val_i))       // 将 bit pattern 作为 float 写入
    pc_.addr += 4

NOT:
    val = bitcast<float,uint32_t>(reg_file_.Read(ra))
    reg_file_.Write(rd, bitcast<uint32_t,float>(~val))
    pc_.addr += 4
```

---

### 5.18 BAR（Barrier 同步）

| 字段 | 值 |
|------|------|
| Opcode | 0x05（VS/FS 统一）|
| Format | D（J-type）|
| 操作数 | 无 |
| 执行周期 | 1 |
| 功能 | Warp 内所有线程同步，等待所有线程到达此 barrier 后继续执行 |

```
BAR:
    warp_barrier_sync()
    stats_.barriers++
    pc_.addr += 4
```

---

### 5.19 JMP / CALL / RET

| 字段 | 值 |
|------|------|
| JMP Opcode | 0x04 |
| CALL Opcode | 0x02 |
| RET Opcode | 0x03 |
| Format | B（双字，JMP/CALL）；D（J-type，RET）|
| 操作数 | JMP: #signed_offset；CALL: #signed_offset；RET: 无 |
| 执行周期 | 1 |
| 功能 | 无条件跳转、调用、返回 |

> **CALL/RET 约定（已修正）**：调用约定中，**R63** 保留用于存储返回地址（CALL 自动将 PC+8 写入 R63，RET 从 R63 恢复）。此为软约定，编译器负责维护调用栈。R63 相比 R1 更安全（R1 在 VS/FS 混合程序中可能被用作通用寄存器）。

> **JMP/CALL/BRA 统一说明**：Format-B 双字指令即使在 fall-through（不跳转/offset=0）时，PC 也应 +8 以跳过 immediate word。此行为与 BRA 的 fall-through 处理一致。

```
JMP(Ra, signed_offset):
    // Ra 字段填 R0（忽略），signed_offset 在 Word2 中
    offset = sign_extend(word2.immediate, 10)
    if (offset != 0) {
        pc_.addr += static_cast<uint32_t>(offset * 4)  // 跳转
    } else:
        pc_.addr += 8  // fall-through：跳过 immediate word（双字）

CALL(Ra, signed_offset):
    // R63 = PC + 8（双字，跳过紧跟的 immediate word）
    ret_addr = pc_.addr + 8
    reg_file_.Write(63, ret_addr)  // R63 = 返回地址
    offset = sign_extend(word2.immediate, 10)
    pc_.addr += static_cast<uint32_t>(offset * 4)
    stats_.calls++

RET:
    ret_addr = reg_file_.Read(63)  // R63 = 返回地址
    pc_.addr = ret_addr
    stats_.returns++
```

---

### 5.20 VSTORE

| 字段 | 值 |
|------|------|
| Opcode | 0x4A |
| Format | **E（双字）** |
| 操作数 | Rd（源寄存器，4-aligned）, word2=byte_offset |
| 执行周期 | **1** |
| 功能 | 将 R[rd]–R[rd+3] 存储到 VATTR buffer（偏移 = word2 imm10 / 4 的 float index）|

> **Format-E 双字格式**：与 Format-B 不同，Format-E 的 Word2 全部为立即数（10-bit byte_offset），不包含寄存器字段。VSTORE 将 Rd（源寄存器，4-aligned）的 4 个 float 值写入 `vabuf_[(word2/4) + i]`。
>
> **与旧版文档的差异**：旧版文档将 VSTORE 描述为 Format-B（2 cycles），实际代码使用 Format-E（1 cycle）。

**Word 1**：[Opcode=0x4A | Rd(7) | 0000000(7) | 00000000000000000000000(24)]

**Word 2**：[0000000000000000000000(22) | byte_offset(10)]

```
VSTORE(Rd, byte_offset):
    attr_base = byte_offset / 4
    for i in 0..3:
        vabuf_[attr_base + i] = reg_file_.Read(rd + i)
    stats_.stores++
    pc_.addr += 8  // 双字
```

---

### 5.21 LDC（常量加载）

| 字段 | 值 |
|------|------|
| Opcode | 0x4C |
| Format | B（双字）|
| 操作数 | Rd, #const_table_offset |
| 执行周期 | **2** |
| 功能 | 从常量表（Constant Buffer）按偏移加载 32-bit float 到 Rd |

**Word 1**：[Opcode=0x4C | Rd(7) | 0000000(7) | 0000000000(10)]

**Word 2**：[0000000 | 0000000 | const_offset(10)]

```
LDC(Rd, const_offset):
    base = get_constant_buffer_base()
    addr = base + const_offset * 4
    value = memory_.Load32(addr)
    reg_file_.Write(rd, value)
    pc_.addr += 8
```

---

### 5.22 ATTR（顶点属性提取）

| 字段 | 值 |
|------|------|
| Opcode | 0x4D |
| Format | **B（双字）** |
| 操作数 | Rd, word2=float_index（10-bit，VBO float 数组下标）|
| 执行周期 | **2** |
| 功能 | 从 VBO 按 float index 加载**单个**分量到 Rd（不同于 VLOAD 的一次性加载 4 分量）|

> **Format-B 双字格式**：Word2 为 10-bit float index（零扩展），直接用作 vbodata_ 数组下标。Ra 字段在 Word1 中被忽略（填 0）。

**Word 1**：[Opcode=0x4D | Rd(7) | 0000000(7) | 0000000000(10)]

**Word 2**：[0000000 | 0000000 | float_index(10)]

```
ATTR(Rd, float_index):
    if (float_index < vcount_)
        reg_file_.Write(rd, vbodata_[float_index])
    else
        reg_file_.Write(rd, 0.0f)
    stats_.loads++
    pc_.addr += 8
```

---

### 5.23 MOV_IMM（立即数移动）

| 字段 | 值 |
|------|------|
| Opcode | 0x48 |
| Format | B（双字）|
| 操作数 | Rd, #imm10（10-bit 立即数，零扩展）|
| 执行周期 | **2** |
| 功能 | 将 10-bit 立即数（零扩展至 32-bit）写入 Rd |

**Word 1**：[Opcode=0x48 | Rd(7) | 0000000(7) | 0000000000(10)]

**Word 2**：[0000000 | 0000000 | imm10(10)]

> **说明**：MOV_IMM 将 Word2 中的 10-bit 立即数（无符号，0–1023）零扩展为 32-bit 后写入 Rd。等价于 `LDI Rd, #imm`，常用于加载小整数常量（如纹理单元 ID、顶点属性索引等）。

**操作数约束**：Ra 字段填 0（忽略）。

```
MOV_IMM(Rd, imm10):
    reg_file_.Write(rd, static_cast<float>(imm10))  // 零扩展立即数写入 float 寄存器
    pc_.addr += 8  // 双字
```

---

### 5.24 MOV（寄存器移动）

| 字段 | 值 |
|------|------|
| Opcode | **0x63** |
| Format | **C（U-type，单字）** |
| 操作数 | Rd, Ra |
| 执行周期 | **1** |
| 功能 | Rd = Ra（寄存器值复制，Format-C 单字）|

> **v2.5 新增指令**：MOV 在 opcode 0x63，原 Phase 2 CVT 槽位上实现为单字寄存器移动指令。若 Phase 2 引入 CVT 指令，需使用独立 opcode（如 0x60–0x62）。

```
MOV:
    reg_file_.Write(rd, reg_file_.Read(ra))
    pc_.addr += 4
```

---

### 5.25 MAT_MUL（已删除，v2.2）

> **已删除。** MAT_MUL（0x40） opcode 于 v2.2 删除，矩阵乘法统一由编译器 lower 为 4×DOT4 指令序列，不再作为独立硬件指令实现。

---

### 5.26 VLOAD

| 字段 | 值 |
|------|------|
| Opcode | 0x49 |
| Format | B（双字）|
| 操作数 | Rd, #byte_offset（Ra 隐含为 VBO base pointer）|
| 执行周期 | **1** |
| 功能 | 从 VBO 按偏移加载顶点属性到 Rd, Rd+1, Rd+2, Rd+3 |

**Word 1**：[Opcode=0x49 | Rd(7) | Ra=ignored(7) | 0000000000(10)]

**Word 2**：[0000000 | 0000000 | byte_offset(10)]

> **说明**：VLOAD 的 Ra 字段被忽略，实际基址由 EU_MEM 单元的 VBO_base 寄存器提供。byte_offset 为字节偏移（0-1023），按 4-byte 对齐访问。
>
> **与旧版文档的差异**：旧版文档说 VLOAD 执行周期为 2，实际代码为 **1** 周期。

**操作数约束**：Rd 必须 4-aligned（Rd % 4 == 0）。

```
VLOAD(Rd, byte_offset):
    fi = byte_offset / 4  // 转换为 float index
    for i in 0..3:
        addr = vbo_base + (fi + i) * 4
        value = memory_.Load32(addr)  // 或直接从 vbodata_[fi+i] 读取
        reg_file_.Write(rd + i, value)
    stats_.loads++
    pc_.addr += 8  // 双字，下一条指令在 +8
```

---

### 5.27 OUTPUT / OUTPUT_VS（统一输出指令）

| 字段 | 值 |
|------|------|
| Opcode | 0x34（OUTPUT）/ 0x4B（OUTPUT_VS，两者功能等价）|
| Format | B（双字）|
| 操作数 | Rd, #offset（Rd 为输出数据，offset 为目标缓冲区内的字节偏移）|
| 执行周期 | **2** |
| 功能 | **VS 上下文**：将裁剪坐标 (x, y, z, w) 输出至 Rasterizer；**FS 上下文**：将颜色 (r, g, b, a) 输出至 Framebuffer |

> **统一设计说明**：0x34 和 0x4B 是同一指令的两种寻址别名（opcode 解码器对两者同等处理），由 EU 根据当前执行上下文决定路由目标。编译器对 VS 程序使用 OUTPUT_VS（0x4B）以明确意图，对 FS 程序使用 OUTPUT（0x34）。

**Word 1**：[Opcode=0x34/0x4B | Rd(7) | 0000000(7) | 0000000000(10)]

**Word 2**：[0000000 | 0000000 | offset(10)]

**操作数约束**：
- Rd 必须 4-aligned（Rd % 4 == 0）
- offset 为字节偏移，指定当前属性在输出缓冲区内的写入起始位置
- VOUTPUTBUF 容量：256 字节（可容纳 16 个顶点的 16 个属性）

> **注意**：VS 程序必须以此指令结尾，否则 Rasterizer 无法接收几何数据。

```
OUTPUT/OUTPUT_VS(Rd, offset):
    base = curvtx_ * 4
    for i in 0..3:
        OUTPUT_BUF[current_stage][base + offset + i * 4] = Rd.f[i]
    if is_vs_context:
        curvtx_ += 1
        vcnt_ += 1
    pc_.addr += 8  // 双字
```

---

### 5.28 BRA（条件分支）

| 字段 | 值 |
|------|------|
| Opcode | 0x01（VS/FS 统一）|
| Format | B（双字）|
| 操作数 | Ra（条件寄存器）, #signed_offset（10-bit 符号扩展）|
| 执行周期 | 1 |
| 功能 | 若 Ra ≠ 0，则 PC ← PC + offset × 4；**fall-through 时 PC +8**（跳过 immediate word）|

**Word 2**：[0000000 | 0000000 | signed_offset(10)]

**跳转范围**：signed_offset ∈ [-512, +511]，单位：4 字节 → 最大跳转范围 ±2KB（±512 条指令）

```
BRA(Ra, signed_offset):
    cond = reg_file_.Read(ra)
    offset = sign_extend(signed_offset, 10)
    next = pc_.addr + 8  // 跳过 immediate word
    if (cond != 0.0f) {
        pc_.addr = next + static_cast<uint32_t>(offset * 4)
        stats_.branches_taken++
    } else:
        pc_.addr = next
```

---

### 5.29 LD / ST（通用内存访问）

| 字段 | 值 |
|------|------|
| LD Opcode | 0x30 |
| ST Opcode | 0x31 |
| Format | B（双字）|
| 操作数 | LD: Rd, [Ra + #imm]；ST: [Ra + #imm], Rb |
| 执行周期 | **2** |
| 功能 | 通用 CPU 内存加载/存储 |

**Word 2**：[0000000 | 0000000 | byte_offset(10)]

**偏移范围**：0 – 1023 字节

> **对齐要求**：`addr = Ra + imm` 必须是 4-byte aligned；未对齐访问行为为 undefined（与 VLOAD 的对齐要求一致）。

```
LD(Rd, Ra, byte_offset):
    addr = reg_file_.Read(ra) + byte_offset
    value = memory_.Load32(addr)
    reg_file_.Write(rd, value)
    pc_.addr += 8

ST(Ra, byte_offset, Rb):
    addr = reg_file_.Read(ra) + byte_offset
    value = reg_file_.Read(rb)
    memory_.Store32(addr, value)
    pc_.addr += 8
```

---

### 5.30 TEX / SAMPLE（纹理采样）

| 字段 | 值 |
|------|------|
| TEX Opcode | 0x32 |
| SAMPLE Opcode | 0x33 |
| Format | A（R-type）|
| 操作数 | TEX: Rd, Ra(u), Rb(v)（Rc 隐含为 tex_id=0）|
| 执行周期 | **10+**（SFU 级别延迟）|
| 功能 | 纹理采样，返回 (r, g, b, a) 到 Rd – Rd+3 |

**操作数约束**：
- Rd 必须 4-aligned
- Ra, Rb 为纹理坐标（float）
- Rc 在实际代码中未使用（tex_id 硬编码为 0），保留给 Phase 2 扩展

```
TEX(Rd, Ra, Rb):
    tex_id = 0  // 当前实现：固定纹理单元 0
    u = reg_file_.Read(ra)
    v = reg_file_.Read(rb)
    rgba = texture_sample(tex_id, u, v)
    reg_file_.Write(rd,     rgba.r)
    reg_file_.Write(rd + 1, rgba.g)
    reg_file_.Write(rd + 2, rgba.b)
    reg_file_.Write(rd + 3, rgba.a)
    stats_.tex_samples++
    pc_.addr += 4
```

---

### 5.31 DOT3（3D 点积）

| 字段 | 值 |
|------|------|
| Opcode | 0x4E（VS 专属）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | **1** |
| 功能 | Rd = Ra · Rb（3-component 向量点积，仅低 3 个分量 x/y/z 参与计算，w 分量忽略）|

**操作数约束**：
- Ra, Rb, Rd 均必须 **4-aligned**（Ra % 4 == 0，Rb % 4 == 0，Rd % 4 == 0）
- 寄存器按分量解释：Ra.x=R[Ra], Ra.y=R[Ra+1], Ra.z=R[Ra+2]
- Rb 同理

```
DOT3(Rd, Ra, Rb):
    // 仅取 Ra 和 Rb 的低 3 个分量计算点积，w 分量不参与
    val_ax = reg_file_.Read(ra)
    val_ay = reg_file_.Read(ra + 1)
    val_az = reg_file_.Read(ra + 2)

    val_bx = reg_file_.Read(rb)
    val_by = reg_file_.Read(rb + 1)
    val_bz = reg_file_.Read(rb + 2)

    result = val_ax * val_bx + val_ay * val_by + val_az * val_bz
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

> **典型用途**：光照计算中的法线向量点积（normalize(DOT3) 后取 saturate 即得兰伯特反射系数）。

---

### 5.32 DOT4（4D 点积）

| 字段 | 值 |
|------|------|
| Opcode | 0x4F（VS 专属）|
| Format | A（R-type）|
| 操作数 | Rd, Ra, Rb |
| 执行周期 | **1** |
| 功能 | Rd = Ra · Rb（4-component 向量点积，x/y/z/w 全部 4 个分量参与计算）|

**操作数约束**：
- Ra, Rb, Rd 均必须 **4-aligned**（Ra % 4 == 0，Rb % 4 == 0，Rd % 4 == 0）
- 寄存器按分量解释：Ra.x=R[Ra], Ra.y=R[Ra+1], Ra.z=R[Ra+2], Ra.w=R[Ra+3]
- Rb 同理

```
DOT4(Rd, Ra, Rb):
    // 4 个分量全部参与点积
    val_ax = reg_file_.Read(ra)
    val_ay = reg_file_.Read(ra + 1)
    val_az = reg_file_.Read(ra + 2)
    val_aw = reg_file_.Read(ra + 3)

    val_bx = reg_file_.Read(rb)
    val_by = reg_file_.Read(rb + 1)
    val_bz = reg_file_.Read(rb + 2)
    val_bw = reg_file_.Read(rb + 3)

    result = val_ax * val_bx + val_ay * val_by + val_az * val_bz + val_aw * val_bw
    reg_file_.Write(rd, result)
    pc_.addr += 4
```

> **典型用途**：齐次坐标变换后的齐次除法前检（ homogeneous perspective divide 前可做 MVP 矩阵最后一行 4D 点积）；也用于四分量颜色向量（如 rgba）的加权点积。

---

## 第6章：执行流水线

### 6.1 单字指令流水线

```
IF → ID → EX → MEM → WB
 1    1    1    0/1   1   （总共 1-2 cycle）
```

- NOP, ADD, MUL, TEX 等：1 cycle effective（EX 1周期，无 MEM 访存）
- LD/VLOAD：MEM 占用 1 额外周期
- DIV：EX 1 周期，但结果延迟 7 周期写回（PendingDiv 队列）

### 6.2 双字指令流水线

```
IF1 → IF2 → ID → EX → MEM → WB
  1     1     1    1    0/1   1   （总共 2-3 cycles）
```

- JUMP/BRA/LD/ST 等：2 cycles（IF2 完成后才能 ID）
- VOUTPUT/VLOAD：2 cycles

### 6.3 执行单元并行

| 执行单元 | 指令 | 备注 |
|---------|------|------|
| EU_ALU | ADD, SUB, MUL, CMP, MIN, MAX, AND, OR... | 通用算术逻辑 |
| EU_SFU | DIV, SQRT, RSQ, SIN, COS, POW, TEX, SAMPLE | 特殊功能，长延迟 |
| EU_MEM | LD, ST, VLOAD, VSTORE | 独立内存端口 |
| EU_VTX | VOUTPUT, NORMALIZE（展开）| VS 矩阵/顶点输出 |

**并行发射约束**：
- EU_MEM 可与 EU_VTX/EU_ALU 并行（不同端口）
- EU_SFU 的长延迟操作（除 DIV/TEX）不阻塞其他单元
- 同一 cycle 内最多发射 1 条 EU_VTX 指令（VOUTPUT 独占流水线）

---

## 第7章：与 v1.0/v1.5 的差异总结

| 维度 | v1.0/v1.5 | v2.5（本文）|
|------|-----------|------------|
| Opcode 宽度 | 7-bit（128 值）| **8-bit（256 值）** |
| 寄存器数量 | 64（R0-R63）| **128（R0-R127）** |
| VS/FS 路由 | bit5 分流（脆弱）| **统一 opcode 空间（干净）** |
| 指令格式 | 单一固定格式 | **5 种格式（A/B/C/D/E）** |
| 双字指令 | 无 | **有（LD/ST/BRA/JUMP/VLOAD 等）** |
| MAT_MUL opcode | 0x28（FS 空间冲突）| **已删除（编译器 lower 为 4×DOT4，v2.2）** |
| MAT_TRANSPOSE opcode | 0x42（VS 专属）| **已删除（v2.3）** |
| VOUTPUT opcode | 0x26（FS 空间冲突）| **0x34/0x4B（统一 OUTPUT，VS/FS 路由由执行上下文决定）** |
| VLOAD opcode | 0x29（FS 空间冲突）| **0x49（VS 专属空间）** |
| HALT opcode | 0x2A（FS 空间冲突）| **0x0F（VS/FS 共用，统一）** |
| VS 算术冗余 | ADD_VS/SUB_VS/MUL_VS 等 15 条重复指令 | **全部废除（v2.1）；MAT_MUL/MAT_ADD/MAT_SUB 同步删除（v2.2，编译器 lower）** |
| 分支跳转范围 | ±512 指令（10-bit offset）| ±512 指令（10-bit signed offset）|
| 内存偏移范围 | 0-1023 字节 | 0-1023 字节（双字格式）|
| 扩展空间 | 0x1D-0x6F（保留少）| **0x40-0x5F（VS 专属）+ 0x60-0xFF（192 个 opcode 保留）** |
| **CALL/RET link register** | R1 | **R63（2026-04-14 修正）** |
| **VSTORE format** | Format-B, 2-cycle | **Format-E, 1-cycle（2026-04-14 修正）** |
| **VLOAD cycles** | 2 | **1（2026-04-14 修正）** |
| **MOV opcode** | 未定义 | **0x63, Format-C, 1-cycle（2026-04-14 新增）** |
| **ATTR format** | Format-B | Format-B |
| **R0 register** | 未特别说明 | **硬件级硬连线到 0.0f（2026-04-16 明确）** |
| **VSTORE word2 fetch** | 未明确 | **Execute 阶段从 prog_[(pc_+4)/4] 获取（2026-04-16 修正）** |

---

## 第8章：实现优先级建议

### Phase 0（Week 1-2）：设计冻结 & 解码器框架

1. 确定 opcode → handler 跳转表（256 项）
2. 实现 Format-A/B/C/D/E 的 decode 逻辑
3. 寄存器文件扩展至 128（R0-R127）
4. 解释器主循环重写（支持双字 fetch）

### Phase 1（Week 3-4）：核心指令实现

> **Phase 1 VS 限制**：Phase 1 不支持矩阵转置操作（MAT_TRANSPOSE 已于 v2.3 删除）。编译器需避免生成矩阵转置操作，或将其 lower 为标量序列（等待 Phase 2 重新引入带更优实现的版本）。

实现优先级顺序：
1. **Format-A R-type**：ADD, SUB, MUL, DIV, NOP, HALT（最基础）
2. **Format-D J-type**：JUMP, BRA, RET, CALL
3. **Format-B 双字**：LD, ST, VLOAD, VSTORE, OUTPUT, OUTPUT_VS, LDC, **MOV_IMM**
4. **Format-C U-type**：MOV, ABS, NEG, FLOOR, CEIL
5. **Format-A R-type 扩展**：MAD, CMP, MIN, MAX, AND, OR, DOT3, DOT4, NORMALIZE
6. **VS 专属**：ATTR, VLOAD, VSTORE, OUTPUT_VS, LDC, MOV_IMM, DOT3, DOT4
7. **FS 纹理**：TEX, SAMPLE

### Phase 2（Week 5）：集成测试

1. 所有 E2E 测试重新跑通
2. Benchmark 对比（v1.5 vs v2.0 性能）
3. Opcode 0xC0-0xFF 扩展槽位文档

---

## 第9章：Known Issues

### 9.1 Format-E 双字指令的 Fetch/Execute 交互问题

**问题描述**：Format-E 指令（如 VSTORE）的 word2 是数据而非操作码，但 Fetch 逻辑中 `idw_ = (fmt == Format::B || fmt == Format::E)` 将 Format-E 也标记为需要获取 word2 的双字指令。这导致 Format-E 指令需要通过 `after_format_e_` 机制在工作流中途取消这个行为，造成设计混乱。

**当前实现**：
- Fetch：Format-E 设置 `idw_=true`，`idf_=false`（等待 word2）
- Execute：VSTORE 手动从 `prog_[(pc_+4)/4]` 获取 word2，设置 `after_format_e_=true` 后返回
- 下一轮 Fetch：检测到 `after_format_e_`，跳过 word2（`pc_+=8`），重置 `idw_`

**根本问题**：Format-E 的 word2 是数据，不应该触发 Fetch 的 dual-word 获取逻辑。理想情况下应该像 Format-C 一样是单字指令。

**影响**：当前实现功能正确（103/103 测试通过），但架构不清晰，未来维护者可能误解 `idw_` 标志的用途。

**建议修复**：将 `idw_` 改为仅在 `fmt == Format::B` 时设置，Format-E 不设置 `idw_`，而是在 Execute(VSTORE) 中直接获取 word2 后正常返回。这需要重新设计 Fetch/Execute 的 dual-word 协调逻辑。

**状态**：已知问题，当前实现可工作，修复优先级低。

### 9.2 ATTR 指令未使用 ATTR table

**问题描述**：第 2 章描述了 ATTR table（attr_id → byte_offset），但 `ExATTR` 实现直接使用立即数索引，没有查表。

**影响**：不影响功能（ATTR 指令按规格直接索引 VBO），但文档与实现不符。

**建议**：统一 ATTR 的使用方式，要么使用 ATTR table（符合文档），要么更新文档说明 ATTR 直接使用立即数索引。

**状态**：文档与实现不一致，需要澄清设计意图。

---

*— 陈二虎，Architect Agent，SoftGPU，2026-04-10*  
*— 小钻风，2026-04-14（代码核实后修正 6 处与代码不符的描述；撤销 2 处虚假"历史修正"描述：ATTR/SEL/SMOOTHSTEP 相关）*  
*— 小钻风，2026-04-16（更新 VSTORE dual-word 实现细节；文档化 Known Issues）*
