# SoftGPU 项目启动会纪要

> **日期：** 2026-03-25
> **参会：** 彦祖、云上小虾米
> **议题：** SoftGPU 项目规划与技术方案评审

---

## 1. 项目背景

**项目名称：** SoftGPU
**项目目标：** 实现一个软件 Tile-Based GPU 模拟器，支持渲染任务 + 高性能分析 + 可视化
**定位：** 学院派路线，循序渐进，12 周计划，扎实践行

**彦祖背景：**
- 职业：程序员
- 方向：图形学 / GPU 架构与优化
- 状态：很久没写代码，需要 Agent 辅助
- 核心诉求：保证质量 + 效率，多 Agent 协同

---

## 2. 团队协作方案

**采用方案：A — 3 Agent 协同**

```
彦祖（决策 + 拍板）
├── 🧠 Architect Agent
│   → 输出：ARCHITECTURE.md、PHASE_X_DESIGN.md
│   → 时机：每个 Phase 开始前启动
│
├── 💻 Code Agent
│   → 输入：Architect 的设计文档
│   → 输出：代码实现 + 单元测试
│   → 规范：Google C++ Style + clang-format
│
└── 🔍 Review Agent
    → 输入：Code Agent 的 PR
    → 输出：review 报告 + 优化建议
    → 时机：每个 Phase 完成后启动
```

**工作流程：**
```
Architect 设计 → Code 实现 → Review 审查 → 彦祖拍板 → 下一 Phase
```

---

## 3. 技术选型决策

| 组件 | 决策 | 备注 |
|------|------|------|
| 构建系统 | **CMake** | 生态成熟，C++ 标准 |
| 语言 | **C++17** | 覆盖主流编译器 |
| UI 框架 | **ImGui (docking)** | 快速出活，指标面板顺手 |
| 窗口+渲染 | **GLFW + OpenGL** | 更轻量，画架构图方便 |
| 数学库 | **glm** | header-only |
| 纹理加载 | **stb_image** | header-only，无依赖 |
| 测试框架 | **GoogleTest** | 工业标准 |

---

## 4. 架构分层决策

**接口风格：** 纯虚类（Abstract Base Class）做接口层，解耦彻底

**模块划分：**

| 模块 | 职责 |
|------|------|
| `CommandProcessor` | 解析 vertex buffer / index buffer / drawcall 参数 |
| `IShaderCore` | 执行 vertex/fragment shader，暴露 `execute(warp)` 接口 |
| `IMemorySubsystem` | GMEM 带宽模型 + L2 Cache 模拟，用令牌桶模拟带宽 |
| `ITiler` | Binning 算法，按 tile 收集图元 |
| `IRasterizer` | DDA 光栅化，输出 fragment list |
| `IProfiler` | 统一插桩，AOP 风格，不侵入业务代码 |

**内存模型：** `IMemorySubsystem` 不真实 allocate，用"令牌桶"模拟：
```
每次访问：addBandwidthAccess(bytes) → 自动计算当前带宽占用
带宽上限：可配置（如 100 GB/s），自动计算利用率
```

---

## 5. 风险点与应对

| 风险点 | 严重程度 | 应对方案 |
|--------|---------|---------|
| Tiling Binning 写错难以调试 | 🔴 高 | 第一周单独写 `test_tiler.cpp`，穷举验证，覆盖率 > 90% 再继续 |
| Fragment shader 多核调度死锁/竞态 | 🔴 高 | Phase 1 单线程顺序跑通；Phase 2 加 thread pool 时严格 review 同步逻辑 |
| 带宽模型和真实 GPU 差太远 | 🟡 中 | 先简化模型，后验阶段用 Mali/Adreno 白皮书数据校准 |
| ImGui 渲染和 GPU 模拟抢资源 | 🟡 中 | ImGui 单独一个线程，结果通过 mutex 传递 |
| 代码风格不一致（多 agent 协作） | 🟡 中 | 强制 `.clang-format` + `Google C++ Style Guide`，CI 检查 |
| Phase 1 耗时超出预期 | 🟡 中 | 砍掉 P1 的 multi-core 支持，单核版先跑通 |

---

## 6. 验证方式决策

**验证分层：**

```
单元测试 (GoogleTest)
  └─ 每个模块独立可测
     例：TilerTest.BinsTriangleToCorrectTile
         RasterizerTest.InterpolatesDepth
         BandwidthTracker.MeasuresAccurately

集成测试 (手动运行 + golden image)
  └─ 每个 Phase 交付标准
     例：Phase1 → 三角形渲染像素级正确
         Phase2 → Binning 结果可 dump 验证

Benchmark 自动化
  └─ --benchmark 跑完输出 CSV
     跟踪每个 Phase 的性能 baseline
```

**每阶段交付标准：**

| Phase | 交付标准 |
|-------|---------|
| P0 | 帧缓冲正确显示，无内存泄漏（valgrind 通过）|
| P1 | golden image 对比，像素级一致 |
| P2 | Binning 测试覆盖率 > 90% |
| P3 | 瓶颈判定结果和人工分析一致（抽3个场景验证）|
| P4 | 所有测试通过，benchmark 可复现 |

---

## 7. 项目总体时间表

| Phase | 内容 | 周期 |
|-------|------|------|
| P0 | 环境搭建（CMake + ImGui + GLFW + glm）| Week 1 |
| P1 | 最小可运行 GPU（能画三角形）| Week 2-3 |
| P2 | TBR 核心（Tiling + Bandwidth 模型）| Week 4-6 |
| P3 | 性能分析系统（Profiler + 瓶颈判定）| Week 7-8 |
| P4 | 测试场景集 + 调优 | Week 9-10 |
| P5 | 可选：Vulkan 接入 | Week 11-12 |

**总工期：** 约 10-12 周（每天 3-4 小时投入）

---

## 8. 代码规范约束

- `.clang-format` 强制格式化
- `Google C++ Style Guide` 规范命名
- 每个 commit 必须有 test 更新
- PR 需要 Review Agent 通过才能合并

---

## 9. 下一步行动

| 序号 | 行动 | 负责 | 状态 |
|------|------|------|------|
| 1 | Architect Agent 输出 PHASE0_DESIGN.md | Architect Agent | 待启动 |
| 2 | 彦祖审阅 PHASE0_DESIGN.md | 彦祖 | 待进行 |
| 3 | Code Agent 实现 P0 环境搭建 | Code Agent | 待启动 |
| 4 | Review Agent 审查 P0 代码 | Review Agent | 待启动 |

---

## 10. 待讨论事项

- [ ] 彦祖的开发环境（操作系统、编辑器偏好）
- [ ] 代码仓库位置（GitHub / GitLab / 其他）
- [ ] CI/CD 方案（GitHub Actions / 本地验证）
- [ ] 后续 Phases 的详细设计待每个 Phase 开始前评审

---

_纪要生成：云上小虾米 🦐_  
_最后更新：2026-03-25_
