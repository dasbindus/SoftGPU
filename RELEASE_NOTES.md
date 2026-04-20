# v1.5 (2026-04-20)

## Bug 修复

- fix: VS VIEW Transform 寄存器重叠导致 u.w 计算错误
- fix: VS vcount_ clearing bug - vbodata_ 被过早清零
- fix: VS ISA shader 多顶点处理 bug

## 新功能

- feat: OBJ 模型加载 - OBJLoader 解析器支持顶点、面、索引解析
- feat: Utah Teapot 模型 - 6320 三角形的经典测试模型
- feat: --obj 命令行选项 - 主程序支持加载 OBJ 模型文件
- feat: OBJ-Model 场景注册 - cube.obj 和 teapot.obj 已注册到 TestSceneRegistry

## 测试

- test_e2e: 99 tests PASSED (+9 新增 Scene015/016)
- 新增: scene015_utah_teapot.cpp, scene016_obj_model.cpp

---

# v1.4.2 (2026-04-16)

## Bug 修复

- fix: CALL/RET link register 问题 - ISA 解释器返回地址错误
- fix: DOT3/DOT4 测试修复
- fix: VSTORE 双字处理

## 新功能

- feat: ISA v2.5 指令集升级 - 50+ 指令支持
- feat: 103 ISA golden 测试用例

## 文档

- docs: ISA_DESIGN.md 更新 Known Issues

---

# v1.4.1 (2026-04-07)

## 改进

- feat: PNG 纹理加载增强 - 使用 `--texture` 参数时自动启用纹理采样 shader
- feat: 新增 Triangle-1Tri-Textured 场景 - 支持 UV 坐标的纹理三角形
- feat: 新增 scene_014 E2E 测试 - PNG 纹理 golden reference 对比测试

## 文档

- docs: 清理过时文档 - 移除 phase-acceptance、releases、agent 相关文档
- docs: 更新 README - 测试数量 78→90 E2E，177→189 total
- docs: 添加 .claude/ 到 .gitignore

## 测试

- test_e2e: 90 tests PASSED
- 新增: scene014_textured_triangle.cpp

---

# v1.3.2 (2026-04-03)

## Bug 修复

- fix: Framebuffer depth test 逻辑 - 改回 z < oldZ (OpenGL 惯例)

## 改进

- feat: GUI 模式支持场景渲染和纹理显示
- feat: GUI 模式支持 --scene 参数
- feat: macOS OpenGL 4.1 + GLSL 410 兼容性

## 构建

- fix: 添加 Linux -ldl 库链接 (dlopen/dlclose 符号)

## 测试

- test_Framebuffer: 7 tests PASSED
- test_Integration: 6 tests PASSED
- test_test_scenarios: 18 tests PASSED
- test_benchmark_runner: 14 tests PASSED
- test_Rasterizer: 5 tests PASSED
- test_PrimitiveAssembly: 5 tests PASSED
- test_VertexShader: 3 tests PASSED
- test_e2e: 77 tests PASSED

---

# v1.3.1 (2026-03-30)

## Bug 修复

- fix: test_Integration PPM filename mismatch - 测试用例文件名不一致问题
- fix: CI test_e2e XML output path - 使用绝对路径修复 artifact 上传
- fix: headless render output filename collision - 使用唯一文件名避免覆盖

## 改进

- feat: 支持编译器自适应 golden 文件 - GCC/Clang 使用不同 golden reference
- feat: CI 完善 - Ubuntu/macOS 双平台测试、headless 渲染验证
- feat: README 完善 - 中文版、依赖说明、渲染效果展示

## 文档

- docs: README 翻译为中文
- docs: 添加 LICENSE 文件 (MIT)
- docs: 添加渲染效果截图 (3 个场景)
- docs: 添加完整版本计划和微架构改造路线图
- docs: 修正历史版本描述

## 测试

- test_e2e: 77 tests PASSED
- test_Integration: 6 tests PASSED
- test_test_scenarios: 18 tests PASSED

---

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
