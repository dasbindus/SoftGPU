# SoftGPU 演进路线图（修订版）

**更新日期**：2026-03-29
**核心理念**：单核优先，管线跑稳再并行

---

## 战略意图

经过 v1.0-v1.2 的快速迭代，我们选择**单核优先**作为主动战略，而非技术限制：

- **主动选择**：先确保单核管线完整、稳定、性能最优，再扩展多核
- **避免过早优化**：多核带来复杂性（同步、缓存一致性、负载均衡），在管线未成熟时引入会拖慢开发速度
- **务实路线**：参考 Mesa 的经验——单核优化做到极致后再并行化

---

## 修订后的路线图

### v1.3（近期）
**目标：管线完整跑通**
- **VertexShader NDC 计算**（MVP 矩阵变换 + Perspective divide）— P0
  - 当前 NDC 输出是 stub（全是 0），必须先修复才能支持真实模型
  - 完成后可启用 `DISABLED_NDCCoordinates_Correct` 测试
- 命令行参数增强（--model, --output, --benchmark）
- Profiler UI 优化

### v1.4（中期）
**目标：性能与功能完善**
- BenchmarkRunner 完善（自动化性能基准测试）
- 周期精确建模（真实 CPI vs 模拟累加）
- 纹理单元：双线性插值采样（支持基础纹理）
- Early-Z 深度测试

### v2.0（后期）
**目标：多核并行化**
- Shader Core × N（N=2-4）
- SIMD Lane 真正执行
- 命令处理器并行
- 负载均衡（不同 tile 分配给不同 core）

### v3.0（远期）
- RTL 转换（Verilog/SystemVerilog）
- 共享内存 + 原子操作
- Compute Shader 支持

---

## v1.3 详细任务

### P0 — VertexShader NDC 计算
**根因**：`VertexShader.cpp` 第 139-142 行 NDC 输出全是 0，MVP 矩阵变换未实现

**需要实现**：
1. 4×4 矩阵结构体（Model、View、Projection）
2. 矩阵乘法
3. Perspective divide（clip → NDC）
4. MVP = Projection × View × Model

**依赖**：这是 GLTF/PBR 材质、OBJ Loader 的基础

---

## 修订说明

- 原 v1.1/v1.2 规划中的多核并行化推至 v2.0
- 单核优先策略：先确保单核管线完整、稳定、性能最优
- 避免过早优化，保持代码可读性和可维护性
- 这是**主动的战略选择**，不是技术限制

---

## 参考：完整功能路线图

来自 `internal/discussions/2026-03-26.md`：

### 短期能力目标
- OBJ 文件加载（支持真实模型）
- 基础纹理采样（Diffuse Map）
- 命令行参数增强

### 中期能力目标
- GLTF 2.0 支持（PBR 材质）
- 多 Pass 渲染（Shadow Map、G-Buffer）
- Blinn-Phong 光照模型
- Timeline Profiler

### 长期能力目标
- Vulkan Compute 后端
- GPU 并行仿真
- 功耗模型
