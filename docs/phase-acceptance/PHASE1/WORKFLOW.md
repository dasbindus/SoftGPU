# PHASE1 开发流程规范

## 迭代规则

1. **白小西修复 P0/P1 问题** → 提交 git commit
2. **王刚严格审查** → 输出审查报告到 `docs/phase-acceptance/PHASE1/review/`
3. **如有问题** → 返回步骤1，最多迭代 3 次
4. **通过标准** → 0 个 P0 问题 + 0 个 P1 问题

## 文件路径规范

```
docs/phase-acceptance/PHASE1/
├── review/
│   └── PHASE1_REVIEW_r1.md    ← 第1次审查
│   └── PHASE1_REVIEW_r2.md    ← 第2次审查
│   └── PHASE1_REVIEW_r3.md    ← 第3次审查
├── development/
│   └── PHASE1_DEV_r1.md      ← 第1次修复记录
│   └── PHASE1_DEV_r2.md      ← 第2次修复记录
│   └── PHASE1_DEV_r3.md      ← 第3次修复记录
└── acceptance/
    └── PHASE1_ACCEPTANCE.md  ← 最终验收报告
```

## 开发规范

1. 每次修复必须包含 git commit
2. 每次修复必须输出自测试报告
3. 每次审查必须输出审查报告
4. 所有报告记录到 `docs/phase-acceptance/PHASE1/` 对应目录

## 审查规则

1. 王刚必须严格审查，不允许放水
2. P0 问题：编译阻塞或功能错误，必须修复
3. P1 问题：重要问题，建议修复
4. 只有 0 P0 + 0 P1 才能通过
