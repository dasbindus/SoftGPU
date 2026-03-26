# PHASE1_DEV_r2.md - FrameDumper 模块实现报告

**日期:** 2026-03-26
**实现者:** 白小西（@xiaoxi）
**版本:** PHASE1_DEV_r2

---

## 实现概述

根据 PHASE1_DESIGN.md 第 10.5 节 FrameDumper 设计，实现 PPM 格式帧导出功能。

### 完成的功能

| 功能 | 描述 | 状态 |
|------|------|------|
| PPM 格式输出 | 将 RGBA float buffer 导出为 P6 binary PPM | ✅ 已实现 |
| FrameDumper 模块 | 新增 `src/utils/FrameDumper.hpp/cpp` | ✅ 已实现 |
| RenderPipeline 集成 | 添加 `dump()`、`setDumpOutputPath()`、`dumpFrame()` 方法 | ✅ 已实现 |
| CMake 集成 | 新增 utils 子目录，pipeline 链接 utils | ✅ 已实现 |

---

## 文件变更清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/utils/FrameDumper.hpp` | FrameDumper 头文件 |
| `src/utils/FrameDumper.cpp` | FrameDumper 实现（PPM 无依赖单函数） |
| `src/utils/CMakeLists.txt` | utils 库 CMake 配置 |

### 修改文件

| 文件 | 说明 |
|------|------|
| `src/pipeline/RenderPipeline.hpp` | 添加 dump 方法声明和 FrameDumper 成员 |
| `src/pipeline/RenderPipeline.cpp` | 实现 dump、setDumpOutputPath、dumpFrame |
| `src/CMakeLists.txt` | 添加 `add_subdirectory(utils)` |
| `src/pipeline/CMakeLists.txt` | pipeline 链接 utils 库 |

---

## API 说明

### FrameDumper 类

```cpp
// 设置输出目录（默认为当前目录 "."）
void setOutputPath(const std::string& path);

// dump RGBA float buffer 为 PPM 文件
// filename: 可以是 "frame_0000.ppm" 或带路径的 "output/frame_0000.ppm"
void dumpPPM(const float* colorBuffer,
             uint32_t width, uint32_t height,
             const std::string& filename) const;

// 按序号 dump 到 frame_XXXX.ppm
void dumpFrame(const float* colorBuffer,
               uint32_t width, uint32_t height,
               uint32_t frameIndex) const;
```

### RenderPipeline 集成

```cpp
// dump 当前帧到指定文件
void dump(const std::string& filename) const;

// 设置 dump 输出目录
void setDumpOutputPath(const std::string& path);

// 便捷方法：dump 到 outputPath/frame_XXXX.ppm
void dumpFrame(uint32_t frameIndex);
```

---

## PPM 格式说明

```
P6              # Binary RGB 格式
640 480         # 宽 高
255             # 颜色最大值
<binary data>   # R G B R G B ... (每像素 3 字节)
```

**颜色范围转换:** `float [0,1]` → `uint8 [0,255]`

---

## 使用示例

```cpp
// 渲染完成后
RenderPipeline pipeline;
pipeline.render(cmd);

// 方式 1: 直接指定文件名
pipeline.dump("frame_0000.ppm");

// 方式 2: 设置输出目录后按序号 dump
pipeline.setDumpOutputPath("./output/");
pipeline.dumpFrame(0);  // 生成 ./output/frame_0000.ppm
pipeline.dumpFrame(1);  // 生成 ./output/frame_0001.ppm
```

---

## 编译验证

| 组件 | 状态 |
|------|------|
| utils 库 | ✅ 编译通过 |
| pipeline 库 | ✅ 编译通过 |
| test_Integration | ✅ 4/4 通过 |
| test_Framebuffer | ✅ 7/7 通过 |
| test_PrimitiveAssembly | ✅ 5/5 通过 |
| test_VertexShader | ✅ 3/3 通过 |

---

## Git Commit 记录

```
d162922 feat(FrameDumper): 实现 PPM 格式帧导出模块
```

---

## 备注

1. **无外部依赖:** PPM 格式输出仅使用标准库（`cstdio`, `cmath`），无需 libpng 等第三方库。
2. **文件可被标准工具打开:** 生成的 `.ppm` 文件可用 `feh`、`Photoshop`、`Preview` 等直接查看。
3. **输出目录:** `setDumpOutputPath` 支持相对路径和绝对路径，末尾会自动补充 `/`。

---

**报告生成时间:** 2026-03-26 04:35 GMT+8
