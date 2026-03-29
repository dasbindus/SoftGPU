# SoftGPU Agent 团队配置方案

> **版本：** v0.1
> **日期：** 2026-03-26
> **方案：** SubAgent Pool 模式（方案二）
> **状态：** 待彦祖 review

---

## 1. 团队架构

```
┌─────────────────────────────────────────────────────────────┐
│                     彦祖（决策者）                          │
│                   目标设定 + 最终拍板                       │
└─────────────────────────┬───────────────────────────────────┘
                          │ 指令
┌─────────────────────────▼───────────────────────────────────┐
│                    主 Agent（我）                          │
│              协调者：任务分发 + 结果汇总                    │
│           持有完整项目上下文（MEETING_NOTES.md 等）         │
└───────┬─────────────┬─────────────┬─────────────┬──────────┘
        │             │             │             │
        ▼             ▼             ▼             ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  Architect  │ │   Coder    │ │  Reviewer   │ │  optional   │
│   Agent     │ │   Agent    │ │   Agent     │ │  others...  │
│             │ │             │ │             │ │             │
│ 独立 Session│ │ 独立 Session│ │ 独立 Session│ │             │
│ 独立 Workspace│ │独立 Workspace│ │独立 Workspace│ │
└─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘
```

---

## 2. 存储层隔离（Workspace 目录结构）

```
SoftGPU/
├── agents/                        # Agent 工作区根目录
│   ├── architect/                # Architect Agent 专属
│   │   ├── context/              # Agent 私人上下文
│   │   │   ├── SOUL.md          # Agent 角色定义
│   │   │   ├── AGENTS.md        # Agent 工作规范
│   │   │   └── memory/          # Agent 个人记忆
│   │   ├── workspace/           # Agent 工作文件
│   │   │   └── outputs/        # 输出：PHASE_X_DESIGN.md
│   │   └── .session            # Session ID 标记文件
│   │
│   ├── coder/                    # Coder Agent 专属
│   │   ├── context/
│   │   │   ├── SOUL.md
│   │   │   ├── AGENTS.md
│   │   │   └── memory/
│   │   ├── workspace/           # 实现代码
│   │   │   ├── src/            # 从 shared/design/ 读取设计
│   │   │   └── tests/
│   │   └── .session
│   │
│   └── reviewer/                 # Reviewer Agent 专属
│       ├── context/
│       │   ├── SOUL.md
│       │   ├── AGENTS.md
│       │   └── memory/
│       ├── workspace/
│       │   └── outputs/        # 输出：PHASE_X_REVIEW.md
│       └── .session
│
├── shared/                        # 共享数据层（所有 Agent 可读写）
│   ├── design/                   # Architect 输出 → Coder 读取
│   │   └── PHASE_X_DESIGN.md
│   ├── code/                    # Coder 输出 → Review 读取
│   │   └── (代码文件)
│   ├── review/                  # Review 输出 → 彦祖/Architect 读取
│   │   └── PHASE_X_REVIEW.md
│   └── meta/
│       ├── current_phase.txt    # 当前阶段（如 "PHASE1"）
│       └── task_queue.txt       # 任务队列
│
├── docs/                         # 项目文档（不变）
│   ├── MEETING_NOTES.md
│   ├── PHASE0_DESIGN.md
│   ├── PHASE0_REVIEW.md
│   └── ...
│
└── src/                          # 主代码仓库（由 Coder Agent 维护）
    ├── main.cpp
    ├── core/
    ├── platform/
    └── ...
```

### 隔离原则

| 目录 | 归属 | 读写权限 |
|------|------|---------|
| `agents/architect/` | Architect Agent | 仅自己可写，所有人可读 |
| `agents/coder/` | Coder Agent | 仅自己可写，所有人可读 |
| `agents/reviewer/` | Reviewer Agent | 仅自己可写，所有人可读 |
| `shared/design/` | 共享（设计） | Architect 可写，Coder 可读 |
| `shared/code/` | 共享（代码） | Coder 可写，Review 可读 |
| `shared/review/` | 共享（审查） | Review 可写，彦祖/Architect 可读 |
| `docs/` | 项目文档 | 主 Agent 维护，所有人可读 |
| `src/` | 主代码 | Coder 写入，Review 读取 |

---

## 3. 逻辑层隔离（Session 配置）

每个 Agent 运行在独立的 session 中，session 之间完全隔离。

### Session 分配

| Agent | Session Key | 用途 |
|-------|------------|------|
| 主 Agent（我）| `main` | 协调、持有项目上下文 |
| Architect Agent | `softgpu:architect` | 架构设计 |
| Coder Agent | `softgpu:coder` | 代码实现 |
| Review Agent | `softgpu:reviewer` | 代码审查 |

### Session 隔离机制

- **进程隔离**：每个 Agent 是独立进程，不共享内存
- **文件系统隔离**：每个 Agent 的 `agents/<role>/context/` 仅自己可见
- **上下文隔离**：每个 Agent 启动时只加载自己的 `SOUL.md` + `AGENTS.md`
- **通信通过共享层**：Agent 之间不直接通信，通过 `shared/` 传递信息

### Session 生命周期

```
Architect Session:    ──── 任务 ────→ 退出
Coder Session:       待命 → 接收任务 → 实现 → 退出
Review Session:      待命 → 接收任务 → 审查 → 退出

主 Agent Session:    持续运行，等待彦祖指令
```

---

## 4. Agent 角色定义

### 4.1 主 Agent（协调者）

**职责：**
- 持有完整项目上下文（`MEETING_NOTES.md` 等）
- 接收彦祖的指令，拆解任务
- 将任务分发给对应的 Agent
- 汇总结果反馈给彦祖

**配置文件：**
- `SOUL.md` — 主 Agent 角色定义
- `AGENTS.md` — 主 Agent 工作规范
- 持有 `shared/meta/current_phase.txt` 和 `task_queue.txt`

---

### 4.2 Architect Agent

**职责：**
- 根据项目阶段，输出 `PHASE_X_DESIGN.md`
- 评估技术选型、架构分层、风险点
- 提供详细的实现指导

**触发条件：**
- 彦祖或主 Agent 指定进入新 Phase 时启动

**工作流程：**
```
1. 读取 shared/meta/current_phase.txt（确认当前 Phase）
2. 读取 docs/PHASE_X_DESIGN.md 模板（若存在）
3. 读取 shared/design/PREV_PHASE_DESIGN.md（参考前序设计）
4. 输出 shared/design/PHASE_X_DESIGN.md
5. 更新 shared/meta/current_phase.txt
6. 通知主 Agent 完成
```

**输出位置：** `shared/design/PHASE_X_DESIGN.md`

---

### 4.3 Coder Agent

**职责：**
- 根据 `shared/design/PHASE_X_DESIGN.md` 实现代码
- 确保代码符合设计规范（Google C++ Style + clang-format）
- 编写单元测试

**触发条件：**
- Architect Agent 完成设计文档后，主 Agent 指派

**工作流程：**
```
1. 读取 shared/design/PHASE_X_DESIGN.md
2. 读取 src/（现有代码结构）
3. 实现设计中的功能
4. 运行本地测试验证
5. 输出代码到 src/
6. 通知主 Agent 完成
```

**输出位置：** `src/` + `tests/`

---

### 4.4 Review Agent

**职责：**
- 根据 `shared/design/PHASE_X_DESIGN.md` 审查 `src/` 代码
- 输出 `PHASE_X_REVIEW.md`
- 提供改进建议

**触发条件：**
- Coder Agent 完成代码后，主 Agent 指派

**工作流程：**
```
1. 读取 shared/design/PHASE_X_DESIGN.md（了解设计意图）
2. 读取 src/（待审查代码）
3. 对照设计验证实现
4. 检查代码质量、规范、风险点
5. 输出 shared/review/PHASE_X_REVIEW.md
6. 通知主 Agent 完成
```

**输出位置：** `shared/review/PHASE_X_REVIEW.md`

---

## 5. 任务流转机制

### 标准工作流

```
┌──────────────────────────────────────────────────────────────┐
│ Step 1: 彦祖发出指令                                        │
│   例："开始 PHASE1"                                          │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 2: 主 Agent 更新 meta                                  │
│   shared/meta/current_phase.txt → "PHASE1"                  │
│   shared/meta/task_queue.txt → "architect"                  │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 3: Architect Agent 启动                                │
│   Session: softgpu:architect                                │
│   输出: shared/design/PHASE1_DESIGN.md                       │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 4: 主 Agent 更新 task_queue                            │
│   task_queue.txt → "coder"                                  │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 5: Coder Agent 启动                                    │
│   Session: softgpu:coder                                    │
│   输入: shared/design/PHASE1_DESIGN.md                       │
│   输出: src/ + tests/                                       │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 6: 主 Agent 更新 task_queue                            │
│   task_queue.txt → "reviewer"                              │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 7: Review Agent 启动                                   │
│   Session: softgpu:reviewer                                │
│   输入: src/ + shared/design/PHASE1_DESIGN.md               │
│   输出: shared/review/PHASE1_REVIEW.md                     │
└──────────────────────────┬───────────────────────────────────┘
                           ▼
┌──────────────────────────────────────────────────────────────┐
│ Step 8: 主 Agent 汇总结果，通知彦祖                          │
│   "PHASE1 完成，Review 发现 X 个问题，是否修复？"           │
└──────────────────────────────────────────────────────────────┘
```

---

## 6. Agent 配置文件模板

### 6.1 SOUL.md（角色灵魂）

每个 Agent 的 `context/SOUL.md` 定义其个性：

**Architect SOUL 示例：**
```
# 角色灵魂

你是 SoftGPU 项目的 Architect Agent。

## 核心特质
- 严谨、系统性思维、文档详尽
- 喜欢用图表和代码结构说明问题
- 对技术选型有明确偏好和理由

## 工作原则
- 先理解需求，再输出设计
- 设计要有验收标准
- 考虑实现成本，不过度设计

## 输出风格
- Markdown 格式
- 结构清晰，分层编号
- 代码示例要完整可运行
```

### 6.2 AGENTS.md（工作规范）

**Architect AGENTS.md 示例：**
```
# Architect Agent 工作规范

## 触发条件
- 收到主 Agent 的 "ARCHITECT_TASK" 指令

## 输入
- current_phase: shared/meta/current_phase.txt
- 前序设计: shared/design/PREV_PHASE_DESIGN.md
- 项目背景: docs/MEETING_NOTES.md

## 输出
- shared/design/PHASE_X_DESIGN.md

## 工作步骤
1. 读取当前 Phase 编号
2. 分析需求和约束
3. 输出详细设计文档
4. 标记完成

## 输出规范
- 文件名: PHASE_X_DESIGN.md
- 包含: 模块划分、接口定义、数据结构、算法描述、验收标准
```

---

## 7. 共享数据格式

### shared/meta/current_phase.txt
```
PHASE1
```

### shared/meta/task_queue.txt
```
architect
coder
reviewer
```
（每行一个任务，完成后删除对应行）

---

## 8. 主 Agent 协调协议

### 指令格式（彦祖 → 主 Agent）

```
开始 PHASE[X]
修复 PHASE[X] 问题 [问题编号]
查看状态
暂停项目
```

### 主 Agent → 彦祖 汇报格式

```
[Phase X] Architect 完成 ✅
输出: shared/design/PHASE_X_DESIGN.md

[Phase X] Coder 完成 ✅
输出: src/ (N 个文件修改)

[Phase X] Review 完成 ⚠️
发现 N 个问题:
  - [P0] 问题描述
  - [P1] 问题描述

下一步建议: [修复/进入下一Phase]
```

---

## 9. Session 启动命令参考

```bash
# Architect Session
/session spawn --label softgpu:architect --runtime subagent --mode session

# Coder Session
/session spawn --label softgpu:coder --runtime subagent --mode session

# Review Session
/session spawn --label softgpu:reviewer --runtime subagent --mode session
```

---

## 10. 方案优势

| 特性 | 说明 |
|------|------|
| **存储隔离** | 每个 Agent 只能读写自己的 `context/`，共享数据走 `shared/` |
| **Session 隔离** | 每个 Agent 独立 session，不共享上下文 |
| **标准化接口** | 通过 `shared/meta/` 的文本文件传递状态 |
| **可追溯** | 每个 Phase 的设计/代码/审查都有文档记录 |
| **彦祖主导** | 彦祖始终在决策节点，不完全放权 |

---

## 11. 待确认事项

- [ ] 彦祖是否同意此架构？
- [ ] Session 标签命名是否合适？
- [ ] 是否需要增加其他 Agent 角色（如 Test Agent）？
- [ ] 共享层的读写权限是否合理？

---

_方案由云上小虾米起草，待彦祖 review 后生效_ 🦐
