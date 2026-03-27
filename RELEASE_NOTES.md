# v0.5 (2026-03-27)

## Bug 修复

- fix: RenderPipeline heap allocation - 修复 ~8MB 栈溢出导致的 TBR crash
- fix: TestScene vertex format - 添加缺失的 w component
- fix: TBR loadTileFromGMEM - 修复错误的 GMEM 加载逻辑

## 改进

- Add: Comprehensive test suite - 11 个管线验证测试
- Add: Command-line parameter support - --headless, --output, --scene
- Add: Test framework improvements - PPM 像素级验证

## 测试

- 测试通过: 22 tests PASSED
- 新增测试: test_PipelineVerification

## 文档

- Update: README with v1.0 roadmap
- Update: .gitignore - 规范化推送规则
