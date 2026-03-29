# 遗留问题追踪

## 活跃问题

| ID | 问题描述 | 责任人 | 状态 | 进展 |
|----|---------|--------|------|------|
| P1 | 飞书文档传输能力未配置，无法通过 feishu_doc 工具上传/写入文档 | 白小东 | ⏳ 待处理 | 需配置飞书 bot 文件上传接口 |

---

## P1 详情

**问题：** 当前飞书 bot 只能读取文档，无法上传/写入文档文件。需要配置文件传输能力。

**影响：** 无法将设计文档（PHASE1_DESIGN.md 等）直接发送到彦祖的飞书文档。

**解决方案：**
1. 研究飞书开放平台的文件上传 API（file/upload）
2. 确认当前 feishu skill 的 feishu_doc 工具是否支持上传
3. 如不支持，需扩展 skill 或通过其他方式实现

**下一步行动：**
- [ ] 白小东调研 feishu_doc 工具能力上限
- [ ] 确认飞书 bot 是否已有文件上传权限
- [ ] 如需配置，指导彦祖完成 bot 权限配置

**创建日期：** 2026-03-26
**最后更新：** 2026-03-26

---

## P2 详情

**问题：** 服务器无 GUI 环境，需要 dump 渲染结果到文件以便离线查看。

**影响：** 开发者无法实时看到渲染结果，调试困难。

**实现路径（建议）：**

方案 A — PPM 格式（最简单）
```cpp
// 输出 640×480 RGBA 为 PPM 格式
void dumpPPM(const float* colorBuffer, int width, int height, const char* filename) {
    FILE* f = fopen(filename, "wb");
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int i = 0; i < width * height; i++) {
        uint8_t r = (uint8_t)(std::min(colorBuffer[i*4+0], 1.0f) * 255);
        uint8_t g = (uint8_t)(std::min(colorBuffer[i*4+1], 1.0f) * 255);
        uint8_t b = (uint8_t)(std::min(colorBuffer[i*4+2], 1.0f) * 255);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}
```

方案 B — PNG 格式（通用性强）
- 使用 stb_image_write.h
- 支持压缩，文件小
- 可用任意图片查看器打开

方案 C — Raw RGBA 二进制（最原始）
```bash
# 直接 dump 原始 float 数据
# 文件名：frame_0000.raw，大小 640×480×4×4 = 4.9MB
```

**建议：** Phase1 先实现方案 A（PPM），轻量无依赖；后续可扩展 PNG 支持。

**下一步行动：**
- [ ] 在 PHASE1 设计文档中补充 FrameDumper 模块
- [ ] 白小西实现 dump 功能

**创建日期：** 2026-03-26
**最后更新：** 2026-03-26

---

## 已关闭问题

（无）
