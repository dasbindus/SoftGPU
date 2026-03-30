# GitHub CI Workflow Documentation

## Overview

SoftGPU uses GitHub Actions for continuous integration. The CI pipeline runs on every push to `master` and every pull request targeting `master`.

## Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Trigger Conditions                          │
├─────────────────────────────────────────────────────────────────┤
│  • Push to master branch                                         │
│  • Pull request targeting master branch                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Job Dependencies                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐                                              │
│  │    build     │  ──▶ (uploads artifacts)                     │
│  │   (matrix)   │                                              │
│  └──────────────┘                                              │
│        │                                                        │
│        ▼                                                        │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐   │
│  │     test     │ ──▶ │headless-test │     │   quality    │   │
│  │              │     │              │     │              │   │
│  └──────────────┘     └──────────────┘     └──────────────┘   │
│        │                   │                                      │
│        └─────────┬─────────┘                                      │
│                  ▼                                                │
│         ┌──────────────┐                                          │
│         │   summary    │                                          │
│         │              │                                          │
│         └──────────────┘                                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Jobs Detail

### 1. Build Job

**Purpose**: Compile the project on multiple platforms and compilers.

**Matrix Strategy**:
| OS           | Compiler | Status  |
|--------------|----------|---------|
| ubuntu-latest | gcc      | Enabled |
| ubuntu-latest | clang    | Enabled |
| macos-latest  | clang    | Enabled |

**Steps**:
1. Checkout code
2. Install system dependencies
3. Configure CMake with Release build type
4. Build with parallel compilation
5. Upload build artifacts (bin/, lib/)

**Artifacts Retention**: 7 days

---

### 2. Test Job

**Purpose**: Run all test suites to verify functionality.

**Dependencies**: Requires `build` job to complete first.

**Test Suites**:

| Test Executable | Description |
|-----------------|-------------|
| `test_test_scenarios` | Scene unit tests |
| `test_Integration` | Integration tests |
| `test_e2e` | End-to-end golden reference tests |
| `ctest` | CMake test runner |

**Steps**:
1. Install dependencies (same as build)
2. Configure and build project
3. Run ctest with output on failure
4. Run each test executable
5. Upload test results (XML format)
6. Upload E2E PPM outputs on failure

**Note**: E2E tests are allowed to fail (`continue-on-error: true`) to collect diagnostic artifacts.

---

### 3. Headless Test Job

**Purpose**: Verify rendering works in headless (no display) mode.

**Dependencies**: Requires `build` job to complete first.

**Scenes Tested**:
- `Triangle-1Tri` - Single green triangle
- `Triangle-Cube` - Cube with multiple triangles

**Steps**:
1. Install minimal dependencies (no GUI libraries needed)
2. Build project
3. Run each scene in headless mode
4. Verify PPM output files are generated

**Artifacts**: Generated PPM files (7 days retention)

---

### 4. Quality Job

**Purpose**: Quick code quality checks without full build.

**Checks Performed**:
- Scan for TODO/FIXME/XXX/HACK comments
- Detect oversized source files (>1MB)

**Note**: Runs independently, no build dependencies.

---

### 5. Summary Job

**Purpose**: Aggregate and report CI results.

**Dependencies**: Runs after all other jobs complete (`needs: [build, test, headless-test]`).

**Output**: Markdown summary showing:
- Build status (pass/fail)
- Test status
- Headless render status
- Links to artifacts

---

## Environment Variables

| Variable | Value | Description |
|----------|-------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | CMake build type |
| `CMAKEFLAGS` | `-DCMAKE_POLICY_VERSION_MINIMUM=3.10` | CMake compatibility |

## Dependencies

### Ubuntu
```bash
libgl1-mesa-dev    # OpenGL
libxkbcommon-dev   # Keyboard handling
libglfw3-dev       # GLFW window library
libgtest-dev       # Google Test
libgmock-dev       # Google Mock
```

### macOS
```bash
glfw3              # Via Homebrew
googletest         # Via Homebrew
```

## Artifacts

### Build Artifacts
- **Name**: `build-{os}-{compiler}`
- **Contents**: `bin/`, `lib/`
- **Retention**: 7 days

### Test Results
- **Name**: `test-results-{os}`
- **Contents**: JUnit XML, PPM files
- **Retention**: 7 days

### E2E Failure Outputs
- **Name**: `e2e-failure-ppm-{os}`
- **Contents**: Generated PPM files (only on failure)
- **Retention**: 7 days

### Headless Outputs
- **Name**: `headless-output-{os}`
- **Contents**: Rendered PPM files
- **Retention**: 7 days

## Troubleshooting

### Build Failures

1. **Ubuntu: wayland-scanner not found**
   - Install: `libglfw3-dev`

2. **macOS: glfw3 not found**
   - Install: `brew install glfw3`

3. **CMake version too old**
   - Minimum required: CMake 3.16

### Test Failures

1. **E2E golden tests fail**
   - Check if rendering output matches golden files in `tests/e2e/golden/`
   - Golden files can be regenerated by deleting them and re-running tests

2. **Headless mode fails**
   - Ensure scene name is valid (see available scenes in program output)
   - Check if PPM files are being generated

### Artifacts Not Available

- Artifacts expire after 7 days
- Check if workflow run completed successfully
- Verify artifact upload step ran

## Adding New Tests

To add a new test to CI:

1. **Unit Tests**: Add to CMakeLists.txt in appropriate `tests/` subdirectory
2. **E2E Tests**: Add to `tests/e2e/` directory
3. **New Scene**: Add to `src/test/TestScene.cpp`

After adding, update the `test` job in `.github/workflows/build.yml` to include the new test executable.

## Status Indicators

| Status | Emoji | Meaning |
|--------|-------|---------|
| success | ✅ | All checks passed |
| failure | ❌ | One or more checks failed |
| skipped | ⏭️ | Job was skipped |
| cancelled | 🛑 | Job was cancelled |

## Contact

For CI issues, check:
1. GitHub Actions logs
2. Build artifacts
3. Test result artifacts
