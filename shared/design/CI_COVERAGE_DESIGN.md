# CI Coverage Design — P1-5

## 目标

- **行覆盖率：** ≥ 80%
- **分支覆盖率：** ≥ 70%（参考目标）

## gcov 集成

### CMake 配置

```cmake
# 启用覆盖率（Debug 或 Coverage build type）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
# 注意：--coverage 与 -O2 冲突，CI 必须用 -O0 或 -Og
```

### 覆盖率编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## 覆盖率报告

### lcov HTML（人类可读）

```bash
lcov --capture --directory build --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
# GitHub Actions Artifact: coverage_html/
```

### gcovr JSON（机器判定）

```bash
gcovr --json --output coverage.json --filter 'src/.*'
gcovr --json-summary coverage_summary.json
```

## CI 阈值门禁

```bash
# 从 JSON 中提取行覆盖率
LINE_RATE=$(jq '.line coverage' coverage_summary.json)
# bc 浮点比较
echo "$LINE_RATE >= 0.80" | bc --quiet
if [ $? -ne 0 ]; then
  echo "ERROR: Line coverage ${LINE_RATE} < 80%"
  exit 1
fi
```

## 覆盖组件

| 组件 | 路径 | 优先级 |
|------|------|--------|
| core | src/core/ | 高 |
| pipeline | src/pipeline/ | 高 |
| stages | src/stages/ | 高 |
| isa | src/isa/ | 中 |
| utils | src/utils/ | 低 |

## GitHub Actions 示例

```yaml
- name: Coverage Report
  run: |
    lcov --capture --directory build --output-file coverage.info
    genhtml coverage.info --output-directory coverage_html
    gcovr --json --output coverage.json --filter 'src/.*'
    gcovr --json-summary coverage_summary.json

- name: Coverage Gate
  run: |
    LINE_RATE=$(jq '.line coverage' coverage_summary.json)
    if ! echo "$LINE_RATE >= 0.80" | bc --quiet; then
      echo "ERROR: Line coverage ${LINE_RATE} < 80%"
      exit 1
    fi

- uses: actions/upload-artifact@v4
  with:
    name: coverage-html
    path: coverage_html/
```

## 注意事项

1. **测试必须先通过** — coverage 报告在测试通过后生成
2. **与 -O2 冲突** — CI 需用 -O0/-Og
3. **source 文件需包含调试信息** — 用 -g
4. **排除第三方代码** — 用 `--filter` 排除 external/、deps/、catch2/
5. **覆盖率是辅助指标** — 不是目的，不能替代代码审查
