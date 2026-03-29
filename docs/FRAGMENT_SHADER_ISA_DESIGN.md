# FragmentShader ISA Execution Design Document
# v1.3 - Phase 1 Technical Analysis

## 1. Architecture Overview

### 1.1 Current Flow
```
FragmentShader::shade()
    └─> Passthrough (clamp color only)
        Input: Fragment {x, y, z, r, g, b, a, u, v}
        Output: Fragment {x, y, z, clamped(r,g,b,a)}
```

### 1.2 Target Flow (After Phase 2)
```
FragmentShader::shade()
    └─> ShaderCore::executeFragment()
        ├─> Load ISA shader code
        ├─> Setup FragmentContext → Registers
        ├─> Interpreter::ExecuteInstruction() loop
        └─> Capture Registers → Fragment output
```

---

## 2. Fragment to Register Mapping

### 2.1 Input Registers (R1-R9)
| Register | Field | Description |
|---------|-------|-------------|
| R1 | pos_x | Fragment X coordinate |
| R2 | pos_y | Fragment Y coordinate |
| R3 | pos_z | Fragment Z (depth) |
| R4 | color_r | Interpolated red |
| R5 | color_g | Interpolated green |
| R6 | color_b | Interpolated blue |
| R7 | color_a | Interpolated alpha |
| R8 | u | Texture U coordinate |
| R9 | v | Texture V coordinate |

### 2.2 Output Registers (R10-R15)
| Register | Field | Description |
|---------|-------|-------------|
| R10 | out_r | Output red |
| R11 | out_g | Output green |
| R12 | out_b | Output blue |
| R13 | out_a | Output alpha |
| R14 | out_z | Output depth |
| R15 | killed | Discard flag (0=keep, 1=discard) |

### 2.3 Temporary Registers (R16-R31)
Available for shader computations.

---

## 3. FragmentContext ↔ Fragment Mapping

### 3.1 Input Mapping (Fragment → FragmentContext)
```cpp
ctx.pos_x = frag.x;
ctx.pos_y = frag.y;
ctx.pos_z = frag.z;
ctx.color_r = frag.r;
ctx.color_g = frag.g;
ctx.color_b = frag.b;
ctx.color_a = frag.a;
ctx.u = frag.u;
ctx.v = frag.v;
```

### 3.2 Output Mapping (FragmentContext → Fragment)
```cpp
out.r = ctx.out_r;
out.g = ctx.out_g;
out.b = ctx.out_b;
out.a = ctx.out_a;
out.z = ctx.out_z;
out.killed = ctx.killed;
```

---

## 4. Interpreter Integration

### 4.1 ShaderCore Components
```cpp
class ShaderCore {
    Interpreter m_interpreter;    // ISA executor
    ShaderFunction m_currentShader;  // Currently loaded shader
    Stats m_stats;               // Execution statistics
    
    void setupFragmentInput(FragmentContext& ctx);
    void captureFragmentOutput(FragmentContext& ctx);
    void executeFragmentInternal(FragmentContext& ctx, const ShaderFunction& shader);
};
```

### 4.2 Execution Flow
```cpp
void ShaderCore::executeFragmentInternal(FragmentContext& ctx, const ShaderFunction& shader) {
    // 1. Load shader
    m_currentShader = shader;
    
    // 2. Setup input registers
    setupFragmentInput(ctx);  // R1-R9 from ctx
    
    // 3. Reset interpreter PC
    m_interpreter.Reset();
    setupFragmentInput(ctx);  // Re-setup after reset
    
    // 4. Execute instruction loop
    while (instr_count < max_instructions) {
        uint32_t instr_word = m_currentShader.code[pc / 4];
        Instruction inst(instr_word);
        m_interpreter.ExecuteInstruction(inst);
        if (op == Opcode::NOP || op == Opcode::RET) break;
    }
    
    // 5. Capture output registers
    captureFragmentOutput(ctx);  // ctx.out_* from R10-R15
}
```

---

## 5. shade() Modification Plan

### 5.1 Current Implementation
```cpp
Fragment FragmentShader::shade(const Fragment& input) const {
    Fragment out = input;
    out.r = std::max(0.0f, std::min(1.0f, out.r));
    out.g = std::max(0.0f, std::min(1.0f, out.g));
    out.b = std::max(0.0f, std::min(1.0f, out.b));
    out.a = std::max(0.0f, std::min(1.0f, out.a));
    return out;
}
```

### 5.2 Target Implementation
```cpp
Fragment FragmentShader::shade(const Fragment& input) const {
    // Convert Fragment to FragmentContext
    FragmentContext ctx;
    ctx.pos_x = input.x;
    ctx.pos_y = input.y;
    ctx.pos_z = input.z;
    ctx.color_r = input.r;
    ctx.color_g = input.g;
    ctx.color_b = input.b;
    ctx.color_a = input.a;
    ctx.u = input.u;
    ctx.v = input.v;
    
    // Execute ISA shader
    m_shaderCore.executeFragment(ctx, m_currentShader);
    
    // Handle killed fragment
    if (ctx.killed) {
        return Fragment::discarded();  // Special marker
    }
    
    // Convert FragmentContext to Fragment output
    Fragment out = input;
    out.r = ctx.out_r;
    out.g = ctx.out_g;
    out.b = ctx.out_b;
    out.a = ctx.out_a;
    out.z = ctx.out_z;
    
    // Clamp color
    out.r = std::max(0.0f, std::min(1.0f, out.r));
    out.g = std::max(0.0f, std::min(1.0f, out.g));
    out.b = std::max(0.0f, std::min(1.0f, out.b));
    out.a = std::max(0.0f, std::min(1.0f, out.a));
    
    return out;
}
```

---

## 6. ISA Shader Code Examples

### 6.1 Flat Color Shader
```cpp
ShaderFunction ShaderCore::getFlatColorShader(float r, float g, float b, float a) {
    ShaderFunction shader;
    
    using softgpu::isa::Instruction;
    using softgpu::isa::Opcode;
    
    // MOV OUT_R, COLOR_R    ; R10 = R4
    // MOV OUT_G, COLOR_G    ; R11 = R5
    // MOV OUT_B, COLOR_B    ; R12 = R6
    // MOV OUT_A, COLOR_A    ; R13 = R7
    // MOV OUT_Z, FRAG_Z     ; R14 = R3
    // NOP                   ; end
    
    shader.code = {
        Instruction::MakeU(Opcode::MOV, 10, 4).raw,  // OUT_R = COLOR_R
        Instruction::MakeU(Opcode::MOV, 11, 5).raw,  // OUT_G = COLOR_G
        Instruction::MakeU(Opcode::MOV, 12, 6).raw,  // OUT_B = COLOR_B
        Instruction::MakeU(Opcode::MOV, 13, 7).raw,  // OUT_A = COLOR_A
        Instruction::MakeU(Opcode::MOV, 14, 3).raw,  // OUT_Z = FRAG_Z
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}
```

### 6.2 Barycentric Color Shader
```cpp
// Interpolates vertex colors across the triangle
// Assumes barycentric weights are in TEX_U, TEX_V
ShaderFunction ShaderCore::getBarycentricColorShader() {
    ShaderFunction shader;
    
    // MOV OUT_R, COLOR_R
    // MOV OUT_G, COLOR_G
    // MOV OUT_B, COLOR_B
    // MOV OUT_A, COLOR_A
    // MOV OUT_Z, FRAG_Z
    // NOP
    
    shader.code = {
        Instruction::MakeU(Opcode::MOV, 10, 4).raw,
        Instruction::MakeU(Opcode::MOV, 11, 5).raw,
        Instruction::MakeU(Opcode::MOV, 12, 6).raw,
        Instruction::MakeU(Opcode::MOV, 13, 7).raw,
        Instruction::MakeU(Opcode::MOV, 14, 3).raw,
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}
```

### 6.3 Depth Test Shader
```cpp
// Compares fragment depth against depth buffer
// Input: R3 = fragment depth, R14 = output depth (to write)
ShaderFunction ShaderCore::getDepthTestShader() {
    ShaderFunction shader;
    
    // CMP TMP0, FRAG_Z, DEPTH_BUF  ; TMP0 = (frag_z < depth_buf) ? 1 : 0
    // SEL OUT_Z, FRAG_Z, DEPTH_BUF, TMP0  ; if TMP0, write frag_z, else keep depth_buf
    // MOV OUT_R, COLOR_R
    // ...
    // NOP
    
    shader.code = {
        Instruction::MakeR(Opcode::CMP, 16, 3, 17).raw,  // TMP0 = frag_z < depth_buf
        Instruction::MakeU(Opcode::MOV, 14, 3).raw,  // OUT_Z = FRAG_Z (passthrough for now)
        // ... color moves ...
        Instruction::MakeNOP().raw
    };
    shader.start_addr = 0;
    
    return shader;
}
```

---

## 7. FragmentShader Class Modifications

### 7.1 Required Changes
1. Add `ShaderCore` member variable
2. Add `ShaderFunction m_currentShader` member
3. Add method to set active shader: `setShader(const ShaderFunction& shader)`
4. Modify `shade()` to use ShaderCore

### 7.2 Header Changes (FragmentShader.hpp)
```cpp
class FragmentShader : public IStage {
private:
    // ... existing members ...
    
    // ISA shader support (NEW)
    mutable ShaderCore m_shaderCore;
    ShaderFunction m_currentShader;
    
public:
    // ... existing methods ...
    
    // Set the active ISA shader (NEW)
    void setShader(const ShaderFunction& shader) { m_currentShader = shader; }
};
```

---

## 8. Execution Pipeline

```
Pipeline::execute()
    │
    ├─> VertexShader::execute()
    │       └─> Output: std::vector<Vertex>
    │
    ├─> PrimitiveAssembly::execute()
    │       └─> Output: std::vector<Triangle>
    │
    ├─> Rasterizer::execute()
    │       └─> Output: std::vector<Fragment>
    │
    ├─> FragmentShader::execute()  <-- MODIFY THIS
    │       │
    │       └─> For each Fragment:
    │               ctx = Fragment → FragmentContext
    │               m_shaderCore.executeFragment(ctx, shader)
    │               output = FragmentContext → Fragment
    │
    └─> Framebuffer::execute()
            └─> Output: RGBA image
```

---

## 9. Testing Strategy

### 9.1 Phase 3: Flat Color Shader Test
- Load flat color shader with green (0, 1, 0, 1)
- Run scene_001_green_triangle
- Verify output is solid green

### 9.2 Phase 4: Barycentric Color Test
- Load barycentric shader
- Run scene_002_rgb_interpolation
- Verify RGB gradient across triangle

### 9.3 Phase 5: Depth Test
- Load depth test shader
- Run scene_003_depth_test
- Verify proper occlusion

### 9.4 Phase 6: Multi-Triangle
- Load multi-triangle shader
- Run scene_005_multi_triangle
- Verify all triangles render correctly

---

## 10. Key Constants

From `ShaderCore.cpp`:
```cpp
namespace ShaderRegs {
    constexpr uint8_t FRAG_X = 1;
    constexpr uint8_t FRAG_Y = 2;
    constexpr uint8_t FRAG_Z = 3;
    constexpr uint8_t COLOR_R = 4;
    constexpr uint8_t COLOR_G = 5;
    constexpr uint8_t COLOR_B = 6;
    constexpr uint8_t COLOR_A = 7;
    constexpr uint8_t TEX_U = 8;
    constexpr uint8_t TEX_V = 9;
    constexpr uint8_t OUT_R = 10;
    constexpr uint8_t OUT_G = 11;
    constexpr uint8_t OUT_B = 12;
    constexpr uint8_t OUT_A = 13;
    constexpr uint8_t OUT_Z = 14;
    constexpr uint8_t KILLED = 15;
    constexpr uint8_t TMP0 = 16;
    constexpr uint8_t TMP1 = 17;
    constexpr uint8_t TMP2 = 18;
}
```

---

## 11. Implementation Notes

1. **ShaderCore is already partially implemented** with:
   - `ShaderFunction` struct
   - `FragmentContext` class
   - `executeFragment()` method
   - Built-in shaders (`getDefaultFragmentShader()`, `getFlatColorShader()`)

2. **FragmentShader needs minimal changes**:
   - Add `ShaderCore` member
   - Add `setShader()` method
   - Modify `shade()` to use `ShaderCore::executeFragment()`

3. **Keep VertexShader unchanged** as specified

4. **ISA instruction format**: 32-bit fixed length
   - 7-bit opcode | 5-bit Rd | 5-bit Ra | 5-bit Rb | 10-bit immediate

5. **Interpreter already handles**: MOV, ADD, SUB, MUL, MAD, CMP, SEL, MIN, MAX, RCP, SQRT, RSQ, DIV, LD, ST, BRA, JMP, CALL, RET, NOP, etc.
