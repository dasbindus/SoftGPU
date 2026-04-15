#pragma once
// implement: interpreter_v2_5.hpp - SoftGPU ISA v2.5 Interpreter Skeleton
// ISA v2.5: 8-bit opcodes, 128 registers (R0-R127), 5 formats
// Pipeline: IF -> ID -> EX -> MEM -> WB
// PC model: Fetch reads at pc_; Execute advances by 4 (single) or 8 (dual)
// Control flow: BRA/JMP/CALL/RET/HALT set pc_ directly (no fall-through advance)

#include "isa_v2_5.hpp"
#include "register_file_v2_5.hpp"
#include "../pipeline/TextureBuffer.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <limits>
#include <cstring>
#include <cstdio>

namespace softgpu { namespace isa { namespace v2_5 {

static constexpr uint32_t DIV_LATENCY = 7;

struct PendingDiv {
    uint8_t rd;
    float result;
    uint64_t completion_cycle;
};

struct Stats {
    uint64_t cycles = 0;
    uint64_t instructions_executed = 0;
    uint64_t branches_taken = 0;
    uint64_t loads = 0;
    uint64_t stores = 0;
    uint64_t calls = 0;
    uint64_t returns = 0;
    void Reset() { cycles = instructions_executed = branches_taken = loads = stores = calls = returns = 0; }
};

class Memory {
public:
    Memory(size_t s = 1048576) : sz_(s), data_(s, 0) {}
    float Load32(uint32_t a) const {
        if (a + 4 > sz_) return 0.0f;
        float v; memcpy(&v, &data_[a], 4); return v;
    }
    void Store32(uint32_t a, float v) {
        if (a + 4 > sz_) return;
        memcpy(&data_[a], &v, 4);
    }
    uint8_t* GetData() { return data_.data(); }
    size_t GetSize() const { return sz_; }
private:
    size_t sz_;
    std::vector<uint8_t> data_;
};

class Interpreter {
public:
    Interpreter();
    void LoadProgram(const uint32_t* code, size_t word_count, uint32_t start_addr = 0);
    void LoadProgramVBO(const uint32_t* code, size_t word_count) { LoadProgram(code, word_count, 0); }
    bool Step();
    void Run(uint64_t max_cycles = 1000000);
    void SetVBO(const float* data, size_t count);
    void SetAttrTable(const std::vector<size_t>& table);
    inline void SetTextureBuffer(int index, void* tex) {
        if (index >= 0 && index < 4) tex_[index] = tex;
    }
    float GetRegister(uint8_t reg) const { return rf_.Read(reg); }
    void SetRegister(uint8_t reg, float v) { rf_.Write(reg, v); }
    uint32_t GetPC() const { return pc_; }
    const Stats& GetStats() const { return st_; }
    size_t GetVertexCount() const { return vcnt_; }
    float GetVOutputFloat(int vi, int off) const;
    const float* GetVOutputBufData() const { return vobuf_.data(); }
    size_t GetVOutputBufSize() const { return vobuf_.size(); }
    void Reset();
    std::string DumpState() const;

    // ========================================================================
    // Compatibility methods (matching old ISA Interpreter interface)
    // ========================================================================

    // Set MVP uniforms: writes model/view/proj matrices to register ranges
    // M_MAT → R8..R23, V_MAT → R24..R39, P_MAT → R40..R55
    void SetUniforms(const float* model, const float* view, const float* proj) {
        if (model) {
            for (int i = 0; i < 16; ++i) m_model_matrix[i] = model[i];
            for (int i = 0; i < 16; ++i) rf_.Write(8 + i, model[i]);
        }
        if (view) {
            for (int i = 0; i < 16; ++i) m_view_matrix[i] = view[i];
            for (int i = 0; i < 16; ++i) rf_.Write(24 + i, view[i]);
        }
        if (proj) {
            for (int i = 0; i < 16; ++i) m_projection_matrix[i] = proj[i];
            for (int i = 0; i < 16; ++i) rf_.Write(40 + i, proj[i]);
        }
    }

    // Set viewport dimensions: writes to R56 (width), R57 (height)
    void SetViewport(float width, float height) {
        m_viewport_width = width;
        m_viewport_height = height;
        rf_.Write(56, width);
        rf_.Write(57, height);
    }

    // Run vertex program for N vertices
    // Reloads uniforms from cached matrices before each vertex
    void RunVertexProgram(const uint32_t* program, size_t vertex_count) {
        if (!program || vertex_count == 0) return;

        // Determine program size by scanning for HALT
        // Instructions have variable length: Format-D=1 word, Format-B/E=2 words
        size_t program_size = 0;
        while (program_size < 10000) {
            Instruction tmp(program[program_size]);
            Opcode op = tmp.GetOpcode();
            if (op == Opcode::HALT || op == Opcode::INVALID) {
                program_size += 1;  // HALT/INVALID is 1 word (Format-D)
                break;
            }
            Format fmt = Instruction::GetFormat(op);
            program_size += (fmt == Format::B || fmt == Format::E) ? 2 : 1;
        }
        cached_prog_.assign(program, program + program_size);

        for (size_t v = 0; v < vertex_count; ++v) {
            // Reset per-vertex state (but preserve vbodata_ which is set externally)
            rf_.Reset();
            pc_ = link_ = 0;
            st_.Reset();
            pd_.clear();
            iv_ = idw_ = idf_ = false;
            run_ = true;
            vobuf_.assign(64, 0.0f);
            vabuf_.assign(64, 0.0f);
            vcnt_ = curvtx_ = 0;
            // Note: do NOT clear vbodata_ here - it contains per-vertex VBO data set externally
            // vcount_ should be set by SetVBO() before RunVertexProgram is called

            // Reload uniforms from cached matrices (R8-R57)
            for (int i = 0; i < 16; ++i) rf_.Write(8 + i, m_model_matrix[i]);
            for (int i = 0; i < 16; ++i) rf_.Write(24 + i, m_view_matrix[i]);
            for (int i = 0; i < 16; ++i) rf_.Write(40 + i, m_projection_matrix[i]);
            rf_.Write(56, m_viewport_width);
            rf_.Write(57, m_viewport_height);

            // Load program and run until HALT
            LoadProgram(cached_prog_.data(), cached_prog_.size(), 0);
            Run(100000);
        }
    }

    // Reset VS state (VBO, VOUTPUT buffer, VATTR buffer, vertex count)
    void ResetVS() {
        // Note: do NOT clear vbodata_ here - it contains per-vertex VBO data set externally via SetVBO
        // Just reset counters and buffers that are internal
        vcount_ = 0;
        vobuf_.assign(64, 0.0f);
        vabuf_.assign(64, 0.0f);
        vcnt_ = curvtx_ = 0;
        after_word1_ = false;  // Critical: reset to prevent instruction skip on next vertex
    }

    // VATTR buffer accessors
    float GetVAttrFloat(int vi, int off) const {
        if (vi < 0 || off < 0) return 0.0f;
        size_t idx = static_cast<size_t>(vi) * 4 + static_cast<size_t>(off);
        return (idx < vabuf_.size()) ? vabuf_[idx] : 0.0f;
    }
    const float* GetVAttrBufData() const { return vabuf_.data(); }
    size_t GetVAttrBufSize() const { return vabuf_.size(); }

    // VBO data accessors
    const float* GetVBOData() const { return vbodata_.data(); }
    size_t GetVBOCount() const { return vbodata_.size(); }

private:
    void Fetch();
    void Decode();
    void Execute();
    void MemoryAccess() {}
    void WriteBack() {}
    void DrainDIVs();

    // Format-D
    void ExNOP() {}
    void ExHALT() { run_ = false; }  // Execute HALT case does `return;` (skips pc_ fall-through)
    void ExRET() { float r63 = rf_.Read(63); pc_ = reinterpret_cast<uint32_t&>(r63); st_.returns++; }
    void ExBAR() { /* scalar interpreter: threads run sequentially, no-op barrier */ }

    // Format-A R-type
    void ExADD() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, rf_.Read(ra) + rf_.Read(rb));
    }
    void ExSUB() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, rf_.Read(ra) - rf_.Read(rb));
    }
    void ExMUL() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, rf_.Read(ra) * rf_.Read(rb));
    }

    void ExDIV() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float b = rf_.Read(rb);
        float r = (b != 0.0f) ? (rf_.Read(ra) / b) : std::numeric_limits<float>::infinity();
        pd_.push_back({rd, r, st_.cycles + DIV_LATENCY});
    }
    void ExCMP() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, rf_.Read(ra) < rf_.Read(rb) ? 1.0f : 0.0f);
    }
    void ExMIN() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, fminf(rf_.Read(ra), rf_.Read(rb)));
    }
    void ExMAX() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, fmaxf(rf_.Read(ra), rf_.Read(rb)));
    }
    void ExAND() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float fa = rf_.Read(ra), fb = rf_.Read(rb);
        uint32_t ia = reinterpret_cast<uint32_t&>(fa);
        uint32_t ib = reinterpret_cast<uint32_t&>(fb);
        uint32_t r = ia & ib;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExOR() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float fa = rf_.Read(ra), fb = rf_.Read(rb);
        uint32_t ia = reinterpret_cast<uint32_t&>(fa);
        uint32_t ib = reinterpret_cast<uint32_t&>(fb);
        uint32_t r = ia | ib;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExXOR() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float fa = rf_.Read(ra), fb = rf_.Read(rb);
        uint32_t ia = reinterpret_cast<uint32_t&>(fa);
        uint32_t ib = reinterpret_cast<uint32_t&>(fb);
        uint32_t r = ia ^ ib;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExSHL() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float fa = rf_.Read(ra);
        uint32_t ia = reinterpret_cast<uint32_t&>(fa);
        int sh = static_cast<int>(rf_.Read(rb)) & 0x1F;
        uint32_t r = ia << sh;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExSHR() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float fa = rf_.Read(ra);
        uint32_t ia = reinterpret_cast<uint32_t&>(fa);
        int sh = static_cast<int>(rf_.Read(rb)) & 0x1F;
        uint32_t r = ia >> sh;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExSETP() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        rf_.Write(rd, rf_.Read(ra) != 0.0f ? 1.0f : 0.0f);
    }
    void ExPOW() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, powf(rf_.Read(ra), rf_.Read(rb)));
    }

    // Format-A R4-type
    void ExMAD() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        // MAD: Rd = Ra * Rb + Rc (accumulate via Rc)
        // v2.5 Format-A encoding: Rc overlaps with Rb bits [9:5] = Rb[6:2]
        // Due to encoding overlap, Rc reads as Rb[6:2] (e.g., Rb=2 → Rc=0)
        float a = rf_.Read(ra), b = rf_.Read(rb), c = rf_.Read(inst_.GetRc());
        float result = a * b + c;
        rf_.Write(rd, result);
    }
    void ExSEL() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        rf_.Write(rd, rf_.Read(rd) != 0.0f ? rf_.Read(ra) : rf_.Read(rb));
    }
    void ExSMOOTHSTEP() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        // ISA v2.5: R[rd] = smoothstep(R[Ra], R[Rb], R[Rd]) — edge0=Ra, edge1=Rb, x=Rd
        float e0 = rf_.Read(ra), e1 = rf_.Read(rb), x = rf_.Read(rd);
        float t = (e1 == e0) ? 0.0f : (x - e0) / (e1 - e0);
        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
        rf_.Write(rd, t * t * (3.0f - 2.0f * t));
    }

    // Format-A VS-special
    void ExDOT3() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float r = rf_.Read(ra) * rf_.Read(rb) + rf_.Read(ra+1) * rf_.Read(rb+1) + rf_.Read(ra+2) * rf_.Read(rb+2);
        rf_.Write(rd, r);
    }
    void ExDOT4() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float r = rf_.Read(ra) * rf_.Read(rb) + rf_.Read(ra+1) * rf_.Read(rb+1) + rf_.Read(ra+2) * rf_.Read(rb+2) + rf_.Read(ra+3) * rf_.Read(rb+3);
        rf_.Write(rd, r);
    }
    // Scale factor for converting normalized texture coordinates [0,1] to
    // texel indices in the fallback checkerboard pattern.
    static constexpr float TEXTURE_COORD_SCALE = 8.0f;

    void ExTEX() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(), rb = inst_.GetRb();
        float u = rf_.Read(ra);
        float v = rf_.Read(rb);

        // Cast void* back to TextureBuffer* and sample
        SoftGPU::TextureBuffer* tex = reinterpret_cast<SoftGPU::TextureBuffer*>(tex_[0]);
        if (tex && tex->valid()) {
            SoftGPU::float4 c = tex->sampleNearest(u, v);
            rf_.Write(rd, c.r);
            rf_.Write(rd+1, c.g);
            rf_.Write(rd+2, c.b);
            rf_.Write(rd+3, c.a);
        } else {
            // Fallback: checkerboard if no valid texture
            int cx = static_cast<int>(u * TEXTURE_COORD_SCALE);
            int cy = static_cast<int>(v * TEXTURE_COORD_SCALE);
            float c = ((cx + cy) % 2 == 0) ? 1.0f : 0.0f;
            rf_.Write(rd, c);
            rf_.Write(rd+1, c);
            rf_.Write(rd+2, c);
            rf_.Write(rd+3, 1.0f);
        }
    }
    void ExSAMPLE() { ExTEX(); }

    // Format-C U-type
    void ExRCP() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float a = rf_.Read(ra);
        rf_.Write(rd, (a != 0.0f) ? (1.0f / a) : std::numeric_limits<float>::infinity());
    }
    void ExSQRT() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float a = rf_.Read(ra);
        rf_.Write(rd, (a >= 0.0f) ? sqrtf(a) : std::nan(""));
    }
    void ExRSQ() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float a = rf_.Read(ra);
        if (a > 0.0f) rf_.Write(rd, 1.0f / sqrtf(a));
        else if (a == 0.0f) rf_.Write(rd, std::numeric_limits<float>::infinity());
        else rf_.Write(rd, std::nan(""));
    }
    void ExSIN() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, sinf(rf_.Read(ra))); }
    void ExCOS() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, cosf(rf_.Read(ra))); }
    void ExEXPD2() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, exp2f(rf_.Read(ra))); }
    void ExLOGD2() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float a = rf_.Read(ra);
        rf_.Write(rd, (a > 0.0f) ? log2f(a) : std::nan(""));
    }
    void ExABS() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, fabsf(rf_.Read(ra))); }
    void ExNEG() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, -rf_.Read(ra)); }
    void ExFLOOR() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, floorf(rf_.Read(ra))); }
    void ExCEIL() { uint8_t rd = inst_.GetRd(), ra = inst_.GetRa(); rf_.Write(rd, ceilf(rf_.Read(ra))); }
    void ExFRACT() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float a = rf_.Read(ra);
        rf_.Write(rd, a - floorf(a));
    }
    void ExF2I() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float fa = rf_.Read(ra);
        uint32_t b = reinterpret_cast<uint32_t&>(fa);
        rf_.Write(rd, reinterpret_cast<float&>(b));
    }
    void ExI2F() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float fa = rf_.Read(ra);
        int32_t v = reinterpret_cast<int32_t&>(fa);
        uint32_t r = static_cast<uint32_t>(v);
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExNOT() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        float fa = rf_.Read(ra);
        uint32_t b = reinterpret_cast<uint32_t&>(fa);
        uint32_t r = ~b;
        rf_.Write(rd, reinterpret_cast<float&>(r));
    }
    void ExMOV() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        rf_.Write(rd, rf_.Read(ra));
    }

    // Format-B dual-word
    // For dual-word: next instruction = pc_ + 8
    void ExBRA() {
        uint8_t ra = inst_.GetRa();
        int16_t off = inst_.GetSignedImm10();
        float c = rf_.Read(ra);
        uint32_t next = pc_ + 8;
        if (c != 0.0f) { pc_ = next + static_cast<uint32_t>(static_cast<int32_t>(off) * 4); st_.branches_taken++; }
        else { pc_ = next; }
    }
    void ExJMP() {
        int16_t off = inst_.GetSignedImm10();
        uint32_t next = pc_ + 8;
        if (off != 0) pc_ = next + static_cast<uint32_t>(static_cast<int32_t>(off) * 4);
        else pc_ = next;
    }
    void ExCALL() {
        // R63 is the dedicated link register (saves pc_ + 8 = address after CALL instruction).
        // R63 is chosen as it's unlikely to be used by compiled code.
        link_ = pc_ + 8;
        rf_.Write(63, reinterpret_cast<float&>(link_));
        int16_t off = inst_.GetSignedImm10();
        pc_ = pc_ + 8 + static_cast<uint32_t>(static_cast<int32_t>(off) * 4);
        st_.calls++;
    }
    void ExLD() {
        uint8_t rd = inst_.GetRd(), ra = inst_.GetRa();
        uint16_t imm = inst_.GetImm10();
        float base = rf_.Read(ra);
        if (!std::isnan(base) && !std::isinf(base) && base >= 0.0f) {
            rf_.Write(rd, mem_.Load32(static_cast<uint32_t>(base) + imm));
        } else { rf_.Write(rd, 0.0f); }
        st_.loads++;
    }
    void ExST() {
        uint8_t ra = inst_.GetRa(), rb = inst_.GetRb();
        uint16_t imm = inst_.GetImm10();
        float base = rf_.Read(ra), v = rf_.Read(rb);
        if (!std::isnan(base) && !std::isinf(base) && base >= 0.0f)
            mem_.Store32(static_cast<uint32_t>(base) + imm, v);
        st_.stores++;
    }
    void ExVLOAD() {
        // Format-B dual-word: Ra implicit=R0 (VBO base pointer), imm=byte_offset(0-1023)
        // Rd must be 4-aligned (4 consecutive registers, fills vec4)
        uint8_t rd = inst_.GetRd();
        uint16_t boff = inst_.GetImm10();  // byte_offset
        size_t fi = boff / 4;              // convert byte offset to float index
        for (int i = 0; i < 4; ++i)
            rf_.Write(rd + i, (fi + i < vcount_) ? vbodata_[fi + i] : 0.0f);
        st_.loads++;
    }
    void ExVSTORE() {
        // Format-E: rd=source register (4-aligned), word2=attr_byte_offset
        // Writes rf_[rd+i] → vabuf_[(attr_byte_offset/4)+i]
        uint8_t rd = inst_.GetRd();
        uint16_t attr_boff = inst_.GetWord2() & 0x3FF;  // byte offset
        for (int i = 0; i < 4; ++i) {
            size_t attr_idx = (attr_boff / 4) + i;
            if (attr_idx < vabuf_.size()) vabuf_[attr_idx] = rf_.Read(rd + i);
        }
        st_.stores++;
    }
    void ExOUTPUT_VS() {
        // Format-B dual-word: output clip-space coordinates to rasterizer
        // Rd = clip coordinates (4-aligned, x/y/z/w), offset = VOUTPUTBUF internal offset
        // Execution cycles: 2, terminating instruction
        uint8_t rd = inst_.GetRd();
        uint32_t base = curvtx_ * 4;
        float cx = rf_.Read(rd), cy = rf_.Read(rd+1), cz = rf_.Read(rd+2), cw = rf_.Read(rd+3);
        if (base + 3 < vobuf_.size()) for (int i = 0; i < 4; ++i) vobuf_[base + i] = rf_.Read(rd + i);
        curvtx_++; vcnt_++;
    }
    void ExLDC() {
        // Format-B dual-word: load from constant buffer
        // Ra = constant buffer base address, imm = byte offset
        uint8_t rd = inst_.GetRd();
        uint16_t off = inst_.GetImm10();
        uint32_t a = cbbase_ + off * 4;
        rf_.Write(rd, (a + 4 <= mem_.GetSize()) ? mem_.Load32(a) : 0.0f);
    }
    void ExATTR() {
        // Format-B dual-word: load vertex attribute from VBO
        // imm = VBO float index (NOT byte offset)
        uint8_t rd = inst_.GetRd();
        uint16_t fi = inst_.GetImm10();  // float index
        if (fi < vcount_)
            rf_.Write(rd, vbodata_[fi]);
        else
            rf_.Write(rd, 0.0f);
        st_.loads++;
    }
    void ExMOVIMM() {
        uint8_t rd = inst_.GetRd();
        uint16_t imm = inst_.GetImm10();
        rf_.Write(rd, static_cast<float>(imm));
    }
    void ExOUTPUT() { ExOUTPUT_VS(); }

    // State
    RegisterFile rf_;
    uint32_t pc_ = 0;
    uint32_t link_ = 0;
    Instruction inst_;
    bool iv_ = false;
    bool idw_ = false;      // current instruction is dual-word (Format_B)
    bool idf_ = false;      // instruction fetch done (word2 fetched)
    bool after_word1_ = false;     // just executed Format_B word1, don't advance pc_ to word2 yet
    bool after_format_e_ = false;  // just executed Format_E word1, skip word2 in next cycle
    bool skip_decode_ = false;      // skip next Decode (word2 is data, not opcode)
    Stats st_;
    std::vector<PendingDiv> pd_;
    Memory mem_;
    std::vector<uint32_t> prog_;
    std::vector<float> vbodata_;
    size_t vcount_ = 0;
    std::vector<size_t> attable_;
    std::vector<float> vobuf_;
    size_t vcnt_ = 0;
    std::vector<float> vabuf_;
    void* tex_[4] = {};
    uint32_t cbbase_ = 0;
    uint32_t vbobase_ = 0;
    uint32_t curvtx_ = 0;
    bool run_ = true;

    // Compatibility with old ISA: MVP matrices + viewport
    std::array<float, 16> m_model_matrix{};
    std::array<float, 16> m_view_matrix{};
    std::array<float, 16> m_projection_matrix{};
    float m_viewport_width = 640.0f;
    float m_viewport_height = 480.0f;

    // Compatibility: cached vertex program bytecode
    std::vector<uint32_t> cached_prog_;
};

// ============================================================================
// Constructor & Reset
// ============================================================================
inline Interpreter::Interpreter() : mem_(1048576), vobuf_(64, 0.0f), vabuf_(64, 0.0f) {
    attable_ = {0, 16, 16, 24};
    Reset();
}

inline void Interpreter::Reset() {
    rf_.Reset();
    pc_ = link_ = 0;
    st_.Reset();
    pd_.clear();
    iv_ = idw_ = idf_ = false;
    after_word1_ = false;
    run_ = true;
    vobuf_.assign(64, 0.0f);
    vabuf_.assign(64, 0.0f);
    vcnt_ = curvtx_ = 0;
}

inline void Interpreter::LoadProgram(const uint32_t* code, size_t n, uint32_t a) {
    prog_.assign(code, code + n);
    pc_ = a;
    iv_ = idw_ = idf_ = run_ = true;
    after_word1_ = false;
}

inline void Interpreter::SetVBO(const float* d, size_t n) {
    vbodata_.assign(d, d + n);
    vcount_ = n;
}

inline void Interpreter::SetAttrTable(const std::vector<size_t>& t) {
    attable_ = t;
}

// ============================================================================
// Pipeline: IF
// ============================================================================
inline void Interpreter::Fetch() {
    if (!run_) return;

    // Skip Format-E word2 (already consumed as data word)
    if (after_format_e_) {
        // Format-E word2 is data, not opcode - skip to next instruction
        pc_ += 8;  // skip past word1+word2 to next instruction
        after_format_e_ = false;
        // fall through to fetch next instruction at new pc_
    }

    // Fetch word2 of Format-B dual-word (second word is also opcode)
    // NOTE: idw_ may be true from previous cycle if we exited Execute early after
    // a dual-word instruction. Reset it here - if we need word2 of a NEW dual-word
    // instruction, idw_ will be set to true again after we fetch word1.
    if (idw_ && !idf_) {
        uint32_t a2 = pc_ + 4;
        if (a2 / 4 < prog_.size()) inst_.word2 = prog_[a2 / 4];
        idf_ = true;
        idw_ = false;  // reset - we've fetched word2 for the PREVIOUS dual-word instruction
        return;  // word2 fetched, will Execute in next cycle
    }

    // Fetch word1
    if (pc_ / 4 >= prog_.size()) { iv_ = false; run_ = false; return; }
    inst_.word1 = prog_[pc_ / 4];
    inst_.word2 = 0;
    iv_ = true;
    idf_ = false;
    Opcode op = inst_.GetOpcode();
    Format fmt = Instruction::GetFormat(op);
    idw_ = (fmt == Format::B || fmt == Format::E);
    if (!idw_) idf_ = true;
}

inline void Interpreter::Decode() {
    if (!iv_) return;
    if (inst_.GetOpcode() == Opcode::INVALID) { iv_ = false; /* do NOT stop pipeline */ }
}

inline void Interpreter::DrainDIVs() {
    uint64_t c = st_.cycles;
    for (size_t i = 0; i < pd_.size(); ) {
        if (pd_[i].completion_cycle <= c) {
            rf_.Write(pd_[i].rd, pd_[i].result);
            pd_.erase(pd_.begin() + i);
        } else ++i;
    }
}

// ============================================================================
// Pipeline: EX
// ============================================================================
inline void Interpreter::Execute() {
    if (!iv_) return;
    st_.instructions_executed++;
    DrainDIVs();

    Opcode op = inst_.GetOpcode();

    switch (op) {
        case Opcode::NOP:
            ExNOP();
            break;
        case Opcode::HALT:
            ExHALT();
            return;  // returns (no pc_ advance)
        case Opcode::RET:
            ExRET();
            return;
        case Opcode::BAR:
            ExBAR();
            break;

        case Opcode::ADD:
            ExADD();
            break;
        case Opcode::SUB:
            ExSUB();
            break;
        case Opcode::MUL:
            ExMUL();
            break;
        case Opcode::DIV:
            ExDIV();
            break;
        case Opcode::CMP:
            ExCMP();
            break;
        case Opcode::MIN:
            ExMIN();
            break;
        case Opcode::MAX:
            ExMAX();
            break;
        case Opcode::AND:
            ExAND();
            break;
        case Opcode::OR:
            ExOR();
            break;
        case Opcode::XOR:
            ExXOR();
            break;
        case Opcode::SHL:
            ExSHL();
            break;
        case Opcode::SHR:
            ExSHR();
            break;
        case Opcode::SETP:
            ExSETP();
            break;
        case Opcode::POW:
            ExPOW();
            break;

        case Opcode::MAD:
            ExMAD();
            break;
        case Opcode::SEL:
            ExSEL();
            break;
        case Opcode::SMOOTHSTEP:
            ExSMOOTHSTEP();
            break;

        case Opcode::DOT3:
            ExDOT3();
            break;
        case Opcode::DOT4:
            ExDOT4();
            break;
        case Opcode::TEX:
            ExTEX();
            break;
        case Opcode::SAMPLE:
            ExSAMPLE();
            break;

        case Opcode::RCP:
            ExRCP();
            break;
        case Opcode::SQRT:
            ExSQRT();
            break;
        case Opcode::RSQ:
            ExRSQ();
            break;
        case Opcode::SIN:
            ExSIN();
            break;
        case Opcode::COS:
            ExCOS();
            break;
        case Opcode::EXPD2:
            ExEXPD2();
            break;
        case Opcode::LOGD2:
            ExLOGD2();
            break;
        case Opcode::ABS:
            ExABS();
            break;
        case Opcode::NEG:
            ExNEG();
            break;
        case Opcode::FLOOR:
            ExFLOOR();
            break;
        case Opcode::CEIL:
            ExCEIL();
            break;
        case Opcode::FRACT:
            ExFRACT();
            break;
        case Opcode::F2I:
            ExF2I();
            break;
        case Opcode::I2F:
            ExI2F();
            break;
        case Opcode::NOT:
            ExNOT();
            break;
        case Opcode::MOV:
            ExMOV();
            break;

        // Format-B dual-word
        case Opcode::BRA:
            ExBRA();
            return;  // returns (no pc_ advance)
        case Opcode::JMP:
            ExJMP();
            return;
        case Opcode::CALL:
            ExCALL();
            idw_ = false;  // clear dual-word flag after Execute returns early
            return;
        case Opcode::LD:
            ExLD();
            break;  // fall through to pc_ += 8 (dual-word)
        case Opcode::ST:
            ExST();
            break;  // fall through to pc_ += 8 (dual-word)
        case Opcode::VLOAD:
            ExVLOAD();
            break;
        case Opcode::VSTORE:
            ExVSTORE();
            after_format_e_ = true;  // signal: next word is Format-E data, skip its Execute
            return;  // terminate (Format-E word2 is data, not opcode)
        case Opcode::OUTPUT_VS:
            ExOUTPUT_VS();
            break;
        case Opcode::LDC:
            ExLDC();
            break;
        case Opcode::ATTR:
            ExATTR();
            break;
        case Opcode::MOV_IMM:
            ExMOVIMM();
            break;
        case Opcode::OUTPUT:
            ExOUTPUT();
            break;

        default:
            break;
    }

    // Fall-through: advance pc_ to next instruction
    if (run_) {
        if (after_format_e_) {
            // Format-E word2 (data word): advance past word1+word2 to next instruction
            pc_ += 8;
            after_word1_ = false;
            after_format_e_ = false;
        } else {
            // Normal instruction advancement
            if (idw_ && !after_word1_) {
                // Format_B word1: don't advance pc_, set flag for word2
                after_word1_ = true;
            } else {
                // Format_B word2 or single-word: advance by 4 (one uint32_t)
                pc_ += 4;
                after_word1_ = false;
            }
        }
    }
}

inline bool Interpreter::Step() {
    st_.cycles++;
    Fetch();
    Decode();
    Execute();
    return run_;
}

inline void Interpreter::Run(uint64_t max_cycles) {
    while (run_ && st_.cycles < max_cycles) {
        if (!Step()) break;
    }
}

inline float Interpreter::GetVOutputFloat(int vi, int off) const {
    if (vi < 0 || off < 0) return 0.0f;
    size_t idx = static_cast<size_t>(vi) * 4 + static_cast<size_t>(off);
    return (idx < vobuf_.size()) ? vobuf_[idx] : 0.0f;
}

inline std::string Interpreter::DumpState() const {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "PC=0x%08X Cycles=%llu Insts=%llu Loads=%llu Stores=%llu\n"
        "R00=%.4f R01=%.4f R02=%.4f R03=%.4f\n"
        "R04=%.4f R05=%.4f R06=%.4f R07=%.4f",
        pc_,
        (unsigned long long)st_.cycles,
        (unsigned long long)st_.instructions_executed,
        (unsigned long long)st_.loads,
        (unsigned long long)st_.stores,
        rf_.Read(0), rf_.Read(1), rf_.Read(2), rf_.Read(3),
        rf_.Read(4), rf_.Read(5), rf_.Read(6), rf_.Read(7));
    return std::string(buf);
}

}}} // namespaces
