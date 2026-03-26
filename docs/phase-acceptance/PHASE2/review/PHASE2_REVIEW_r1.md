# PHASE2 Review Report r1

**Reviewer:** 王刚（@wanggang）  
**Author:** 白小西（@xiaoxi）  
**Date:** 2026-03-26  
**Phase:** PHASE2 - Tile-Based Rendering Core  
**Status:** ⚠️ **CONDITIONAL PASS** — Implementation structurally sound, tests pass, but 3 critical functional gaps require PHASE3 fixes  

---

## 1. Executive Summary

PHASE2 implements the TBR (Tile-Based Rendering) core with 5 new/rebuilt modules. The architecture is well-structured and PHASE1 backward compatibility is preserved. **All 29 active tests pass** (1 disabled). However, 3 critical functional gaps were identified that will impact bandwidth modeling and frame continuity in PHASE3.

**Test Results:** 29 PASSED / 1 DISABLED (Performance_SingleTriangle)

---

## 2. Module Implementation Review

### 2.1 TilingStage ✅

**File:** `src/stages/TilingStage.hpp`, `src/stages/TilingStage.cpp`

| Aspect | Status | Notes |
|--------|--------|-------|
| Binning algorithm | ✅ | bbox-based per-triangle tile assignment |
| NDC → tile coordinate transform | ✅ | Correct: NDC → screen → tile grid |
| TileBin per-tile storage | ✅ | `std::array<TileBin, 300>` fixed array |
| Performance counters | ✅ | `triangles_binned`, `tiles_affected` |
| `tri.culled` skip | ✅ | Skips culled triangles correctly |

**Issue (Minor):** `tiles_affected=99` for a small center triangle. This is a side effect of bbox-based binning — tiles partially covered by triangle edges are included. This over-counts but does not affect correctness. The dev report acknowledges this.

**Code Quality:** Clean, well-commented. The `computeBbox()` clamping logic is correct.

---

### 2.2 TileBuffer ✅

**File:** `src/stages/TileBuffer.hpp`, `src/stages/TileBuffer.cpp`

| Aspect | Status | Notes |
|--------|--------|-------|
| Per-tile LMEM layout | ✅ | 32×32 RGBA (16KB) + depth (4KB) per tile |
| `depthTestAndWrite` | ⚠️ | Logic correct but NDC vs window-space mismatch (see §3.2) |
| `loadFromGMEM` / `storeToGMEM` | ✅ | Proper float-wise copy |
| Statistics counters | ✅ | `tileWriteCount`, `depthTestCount`, `depthRejectCount` |

**Issue (Minor):** `tileWriteCount` is incremented by `RenderPipeline` externally (`getTileWriteCount()`) but never inside `TileBuffer.cpp` itself. This is a documentation inconsistency — the counter is never set internally.

---

### 2.3 MemorySubsystem ⚠️

**File:** `src/core/MemorySubsystem.hpp`, `src/core/MemorySubsystem.cpp`

| Aspect | Status | Notes |
|--------|--------|-------|
| Token bucket model | ✅ | 100 GB/s, correct `tryConsume` / `refill` |
| L2 Cache simulation | ✅ | 8-way set associative, 256 sets, LRU |
| `addAccess` bandwidth tracking | ✅ | Read/write bytes recorded |
| Cache hit/miss tracking | ✅ | `getHits()`, `getMisses()`, `getHitRate()` |
| **GMEM read/write simulation** | ❌ | `readGMEM`/`writeGMEM` only call cache, no actual data copy |

**Critical Issue:** `readGMEM()` and `writeGMEM()` only invoke `m_l2Cache.access()` for modeling purposes but do **not** perform any actual memory copy. The `dst`/`src` pointers passed to these functions are never used. This is a stub implementation — PHASE3 must implement actual data movement. The function signatures suggest a copy should happen but the body only does cache simulation.

**Note on `refill()`:** The implementation always refills to `maxTokens` regardless of elapsed time. This means the token bucket does not correctly model time-based bandwidth limiting across multiple accesses. Tokens never "run out" realistically. This should be fixed in PHASE3 with proper wall-clock-based refill.

---

### 2.4 TileWriteBack ✅ (with gaps)

**File:** `src/stages/TileWriteBack.hpp`, `src/stages/TileWriteBack.cpp`

| Aspect | Status | Notes |
|--------|--------|-------|
| Per-tile `loadTileFromGMEM` | ✅ | Correctly calls `memory->addAccess` |
| Per-tile `storeTileToGMEM` | ✅ | Correctly calls `memory->addAccess` |
| `loadAllTilesToBuffer` | ✅ | Correct data copy |
| `storeAllTilesFromBuffer` | ⚠️ | **Does NOT call `memory->addAccess`** — no bandwidth tracking |
| GMEM simulation storage | ✅ | `m_gmemColor` / `m_gmemDepth` vectors |
| GMEM offset calculation | ✅ | `getTileColorOffset`, `getTileDepthOffset` correct |

**Critical Gap:** `storeAllTilesFromBuffer()` and `loadAllTilesToBuffer()` do not invoke `m_memory->addAccess()`. As a result, `MemorySubsystem::getReadBytes()` and `getWriteBytes()` show **0** for the entire frame in the performance report. This must be fixed in PHASE3 before bandwidth modeling can be validated.

---

### 2.5 RenderPipeline ✅ (with gaps)

**File:** `src/pipeline/RenderPipeline.hpp`, `src/pipeline/RenderPipeline.cpp`

| Aspect | Status | Notes |
|--------|--------|-------|
| TBR main loop structure | ✅ | TilingStage → per-tile loop → GMEM store |
| `executeTile()` | ⚠️ | Per-tile load/store is commented out (see §3.1) |
| PHASE1 compat mode | ✅ | `setTBREnabled(false)` path works |
| `syncGMEMToFramebuffer` | ✅ | Copies GMEM → FB for test compatibility |
| Performance counter wiring | ⚠️ | `FragmentShader::extra_count1` is TileBuffer cumulative (see §3.4) |

---

## 3. Critical Functional Gaps

### 3.1 `executeTile()` — GMEM Load/Store Not Wired

**Location:** `RenderPipeline.cpp`, `executeTile()` function

```cpp
// Step 1: Load tile from GMEM (initialize LMEM with GMEM state)
// PHASE2: For now, we clear to default (no history load)
(void)m_tileWriteBack;
(void)tileMem;
```

The per-tile GMEM load and store are **not connected**. `loadTileFromGMEM()` and `storeTileToGMEM()` are never called during the tile loop. The tile data from the fragment shader write goes to `m_tileBuffer` (LMEM), but no actual GMEM traffic occurs per-tile.

**Impact:** PHASE3 per-tile bandwidth modeling cannot build on this foundation without significant rework. The current code only stores all tiles at frame end via `storeAllTilesFromBuffer()`.

**Recommended Fix:** Uncomment and wire `loadTileFromGMEM()` and `storeTileToGMEM()` in `executeTile()`, guarded by a `PHASE3_LOADSTORE` flag or similar.

---

### 3.2 Depth Coordinate Space Mismatch (Potential)

**Location:** `TileBufferManager::depthTestAndWrite()` vs `TileBuffer::clear()`

- `clear()` initializes depth to `CLEAR_DEPTH = 1.0f`
- `depthTestAndWrite` compares fragment `z` against `tile.depth[localIndex]` with `z < tile.depth[...]`
- The fragment `z` comes from `Rasterizer::interpolateAttributes()` using NDC Z (`tri.v[0].ndcZ`)
- But `CLEAR_DEPTH = 1.0f` is the far plane in **window space** [0, 1]

**Hypothesis:** The depth comparison is mixing NDC and window coordinate spaces. Yet tests pass. This may be because the NDC Z of 0 (near plane) is always `< CLEAR_DEPTH = 1.0`, making the depth test always pass for NDC-Z fragments — meaning the depth buffer effectively doesn't reject any valid fragment. The test suite only checks color correctness, not depth correctness.

**Impact:** If a fragment with NDC Z > 1.0 (beyond far plane) were generated, it would incorrectly pass the depth test. For PHASE2 test cases (simple triangles), this is not triggered.

**Recommended Fix:** Clarify the depth coordinate space convention. Either (a) convert fragment NDC Z to window Z before depth test, or (b) use CLEAR_DEPTH = 1.0 (NDC far plane) consistently. Document the convention.

---

### 3.3 Bandwidth Counters Always Zero

**Location:** `RenderPipeline.cpp` performance report

```
MemorySubsystem:
  read_bytes=0  write_bytes=0  total_accesses=0
```

This is caused by two factors:
1. `executeTile()` doesn't call GMEM load/store (gap 3.1)
2. `storeAllTilesFromBuffer()` doesn't call `memory->addAccess()` (gap 2.4)

**Impact:** Bandwidth utilization cannot be measured or validated in PHASE2. The `MemorySubsystem` model is structurally present but never exercised.

---

### 3.4 Performance Counter - `extra_count1` Semantic Drift

**Location:** `FragmentShader::setInputAndExecuteTile()`

```cpp
m_counters.extra_count1 = m_tileBuffer->getDepthTestCount();  // cumulative!
```

`extra_count1` is set to `TileBuffer::getDepthTestCount()`, which is a **cumulative** counter across all invocations. This means the value doesn't represent per-invocation depth tests but an ever-increasing total. The semantic of `extra_count1` differs from other stages where it represents a per-call quantity.

---

## 4. TBR Data Flow Review

### 4.1 Data Flow (Correct Structure)

```
PrimitiveAssembly.getOutput()  [triangles in NDC space]
    ↓
TilingStage.execute()          [bins triangles to 300 tile bins]
    ↓
Per-tile loop (tileIndex 0..299):
    ↓
    Rasterizer.setInputPerTile() → executePerTile()
        [emits fragments with absolute screen coords + NDC Z]
    ↓
    FragmentShader.setInputAndExecuteTile()
        [converts to local tile coords, writes to TileBuffer with depth test]
    ↓
    TileBuffer: depthTestAndWrite()  [LMEM write]
    ↓
storeAllTilesFromBuffer()      [LMEM → GMEM copy at frame end]
    ↓
syncGMEMToFramebuffer()        [GMEM → FB for test compat]
```

**Verdict:** The data flow structure is correct. All stages are wired appropriately.

### 4.2 Per-Tile Rasterization — Clamping Logic

**Location:** `Rasterizer::rasterizeTrianglePerTile()`

- For `tileX==0 && tileY==0`: clamps to viewport (PHASE1 compat)
- For other tiles: clamps to tile region `[tileScreenX, tileScreenX2)` × `[tileScreenY, tileScreenY2)`

This is correct. The tile (0,0) special case ensures PHASE1 mode (which effectively uses tile 0,0 as the "viewport") continues to work.

### 4.3 Fragment Shader — Tile Buffer Mode

**Location:** `FragmentShader::setInputAndExecuteTile()`

The flow is correct:
1. Computes local coordinates: `localX = frag.x - tileX * TILE_WIDTH`
2. Calls `m_tileBuffer->depthTestAndWrite(tileIndex, localX, localY, z, color)`
3. The depth test happens inside TileBuffer with local coordinates

This correctly isolates per-tile memory.

---

## 5. PHASE1 Backward Compatibility

### 5.1 Test Compatibility ✅

All PHASE1 tests pass:
- `RasterizerTest` — 5/5 passed
- `FramebufferTest` — 7/7 passed
- `PrimitiveAssemblyTest` — 5/5 passed
- `VertexShaderTest` — 3/3 passed
- `IntegrationTest` — 4/4 passed (1 disabled)

### 5.2 API Compatibility

| API | PHASE1 Behavior | PHASE2 Behavior |
|-----|-----------------|-----------------|
| `RenderPipeline::render()` | Full immediate-mode pipeline | TBR pipeline (default) |
| `setTBREnabled(false)` | — | Switches to PHASE1 mode |
| `getFramebuffer()` | Returns framebuffer | Returns framebuffer (GMEM synced) |
| `IStage` interface | Unchanged | Unchanged |
| `Rasterizer::execute()` | Full rasterization | Delegates to `rasterizeTrianglePerTile(tri, 0, 0)` |

**Issue (Minor):** `Rasterizer::execute()` calls the per-tile rasterizer with `tileX=0, tileY=0`, relying on the viewport-clamping special case. This is fragile — if the special case logic ever changes, PHASE1 mode breaks silently. Consider adding an explicit flag.

---

## 6. Performance Counter Audit

| Stage | Counter |正确埋点? | Notes |
|-------|---------|---------|-------|
| TilingStage | `invocation_count` | ✅ | triangles input |
| TilingStage | `extra_count0` | ✅ | triangles_binned |
| TilingStage | `extra_count1` | ✅ | tiles_affected |
| Rasterizer | `extra_count1` | ✅ | tile_fragments |
| FragmentShader | `extra_count0` | ✅ | fragments_shade |
| FragmentShader | `extra_count1` | ⚠️ | cumulative TileBuffer depth test count (not per-invocation) |
| MemorySubsystem | `read_bytes` | ❌ | Always 0 (batch functions don't track) |
| MemorySubsystem | `write_bytes` | ❌ | Always 0 |
| MemorySubsystem | `bandwidth_util` | ⚠️ | Shows 0% (uses the zeroed counters) |
| MemorySubsystem | `l2_hit_rate` | ⚠️ | Shows 0% (L2 never accessed due to gap 3.1) |

---

## 7. Integration Test Observations

From running `test_Integration`:

```
TilingStage: inv=1 triangles_binned=1 tiles_affected=99
Rasterizer: inv=0 tile_fragments=0  (Rasterizer invoked via per-tile, not execute())
FragmentShader: inv=0  (FragmentShader invoked via per-tile, not execute())
Framebuffer: inv=0 writes=0 depth_tests=0
TileWriteBack: inv=0
MemorySubsystem: read_bytes=0 write_bytes=0
```

**Interpretation:**
- The per-tile stages (`Rasterizer::executePerTile()`, `FragmentShader::setInputAndExecuteTile()`) are called but their counters are reported as `inv=0` because the main pipeline loop doesn't call `getCounters()` on these per-invocation calls — it only calls them on the non-per-tile `execute()` paths. The per-tile counter values are tracked internally but not accumulated into the main pipeline report.

- This is a **reporting gap** — the pipeline performance report doesn't include per-tile fragment counts in the main Rasterizer/FragmentShader lines. The per-tile data is tracked but only visible through `extra_count1` values.

---

## 8. Build Status

The full build fails due to pre-existing issues in `extern/glad/` and `src/platform/Window.cpp` (GLFW API version mismatch, glad header conflicts). These are **not PHASE2 regressions** — they existed before PHASE2.

However, the core libraries (`libpipeline.a`, `libcore.a`, `libstages.a`, `libutils.a`) compile successfully, and all test executables were already built. The tests run correctly.

**Recommendation:** Fix the glad/Window build issues as a separate pre-PHASE3 task.

---

## 9. Summary of Required Fixes (PHASE3)

| # | Severity | Issue | Location |
|---|----------|-------|----------|
| 1 | **Critical** | `executeTile()` GMEM load/store not wired | `RenderPipeline.cpp` |
| 2 | **Critical** | `storeAllTilesFromBuffer` doesn't track bandwidth | `TileWriteBack.cpp` |
| 3 | **Critical** | `MemorySubsystem::readGMEM`/`writeGMEM` are stubs (no actual memcpy) | `MemorySubsystem.cpp` |
| 4 | **Minor** | Depth coordinate space clarification needed | `TileBuffer` vs `Rasterizer` |
| 5 | **Minor** | `extra_count1` semantic drift in FragmentShader | `FragmentShader.cpp` |
| 6 | **Minor** | `tileWriteCount` never set inside TileBuffer | `TileBuffer.cpp` |
| 7 | **Minor** | `Rasterizer::execute()` relies on fragile tile(0,0) special case | `Rasterizer.cpp` |
| 8 | **Minor** | Per-tile counter values not included in pipeline report | `RenderPipeline.cpp` |
| 9 | **Build** | glad/Window.cpp fail to compile (pre-existing) | `extern/glad/`, `src/platform/` |

---

## 10. Verdict

**PHASE2 passes review with conditions.**

**Strengths:**
- Clean, well-documented code architecture
- Correct TBR data flow structure
- All 29 active tests pass
- PHASE1 backward compatibility maintained
- Performance counter infrastructure is largely in place

**Must Fix Before PHASE3:**
- Wire GMEM load/store in `executeTile()` — without this, PHASE3 per-tile bandwidth modeling has no foundation
- Add bandwidth tracking to `storeAllTilesFromBuffer()` / `loadAllTilesToBuffer()`
- Implement actual memory copy in `readGMEM()` / `writeGMEM()`

**Recommendation:** Proceed to PHASE3 after fixing items 1–3. Items 4–9 are refinements that should be addressed in PHASE3 as part of the bandwidth model completion.

---

*Reviewer: 王刚（@wanggang）*  
*Date: 2026-03-26*
