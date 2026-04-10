# E2E Test Cases — v1.4 Regression Baseline

**Date:** 2026-04-11
**Commit:** v1.4 (master @ 0f42820)
**Test binary:** `SoftGPU/build/bin/test_e2e`
**Total test cases:** 90 (all PASSED)
**Disabled:** 2 (`DISABLED_Scene005_MultiTriangle_GenerateGolden`, `DISABLED_Scene012_DepthComplexity_GoldenReference`)

## Pipeline Path
All E2E tests go through `RenderPipeline` → `VertexShader` → `FragmentShader` → `Rasterizer` → `TilingStage` → `TileWriteBack`

## Test Case List

### Scene 001 — Green Triangle (8 sub-tests)
- `Scene001_GreenTriangle_RendersGreenPixels`
- `Scene001_GreenTriangle_BoundingBoxCentered`
- `Scene001_GreenTriangle_PPMDumpCorrect`
- `Scene001_GreenTriangle_TopCenteredPosition`
- `Scene001_GreenTriangle_NoLargeHoles`
- `Scene001_GreenTriangle_BoundingBoxExact`
- `Scene001_GreenTriangle_SlantedEdgeLinearity`
- `Scene001_GreenTriangle_GoldenReference`

### Scene 002 — RGB Interpolation (8 sub-tests)
- `Scene002_RGBInterpolation_HasMixedColors`
- `Scene002_RGBInterpolation_CenterIsGrayish`
- `Scene002_RGBInterpolation_VertexColorsPreserved`
- `Scene002_RGBInterpolation_PPMDumpHasGradient`
- `Scene002_RGBInterpolation_NoLargeHoles`
- `Scene002_RGBInterpolation_BoundingBoxExact`
- `Scene002_RGBInterpolation_SlantedEdgeLinearity`
- `Scene002_RGBInterpolation_GoldenReference`

### Scene 003 — Depth Test (10 sub-tests)
- `Scene003_DepthTest_FrontOccludesBack`
- `Scene003_DepthTest_NonOverlappingTriangles`
- `Scene003_DepthTest_ZBufferCorrectValues`
- `Scene003_DepthTest_CanBeDisabled`
- `Scene003_DepthTest_PPMDumpShowsOcclusion`
- `Scene003_DepthTest_GreenBBoxExact`
- `Scene003_DepthTest_RedBBoxExact`
- `Scene003_DepthTest_SlantedEdgeLinearity`
- `Scene003_DepthTest_GoldenReference`

### Scene 004 — MAD Verification (10 sub-tests)
- `Scene004_MAD_CorrectInterpolation`
- `Scene004_MAD_ChainAccuracy`
- `Scene004_MAD_ColorInterpolation`
- `Scene004_MAD_ZeroOperand`
- `Scene004_MAD_Precision`
- `Scene004_MAD_NegativeValues`
- `Scene004_MAD_TextureCoordInterpolation`
- `Scene004_MAD_BoundingBoxExact`
- `Scene004_MAD_SlantedEdgeLinearity`
- `Scene004_MAD_GoldenReference`

### Scene 005 — Multi Triangle (8 sub-tests)
- `Scene005_MultiTriangle_FrontmostVisible`
- `Scene005_MultiTriangle_BlueOccludesOthers`
- `Scene005_MultiTriangle_DepthValuesUpdated`
- `Scene005_MultiTriangle_AllRender`
- `Scene005_MultiTriangle_PPMDumpCorrect`
- `Scene005_MultiTriangle_BlueBBoxExact`
- `Scene005_MultiTriangle_SlantedEdgeLinearity`
- `Scene005_MultiTriangle_GoldenReference`

### Scene 006 — Warp Scheduling (12 sub-tests)
- `Scene006_Warp_LargeTriangleGeneratesManyFragments`
- `Scene006_Warp_MultipleTrianglesScheduled`
- `Scene006_Warp_SameRowFragmentsConsistent`
- `Scene006_Warp_TileBoundaryNoArtifacts`
- `Scene006_Warp_ManySmallTriangles`
- `Scene006_Warp_PPMDumpCorrect`
- `Scene006_Warp_InterleavedTriangles`
- `Scene006_Warp_FullScreenQuad`
- `Scene006_Warp_LargeTriangleBBoxExact`
- `Scene006_Warp_SlantedEdgeLinearity`
- `Scene006_Warp_GoldenReference`

### Scene 007 — Triangle 1Tri (3 sub-tests)
- `Scene007_Triangle1Tri_GoldenReference`
- `Scene007_Triangle1Tri_GreenPixelCount`
- `Scene007_Triangle1Tri_BoundingBox`

### Scene 008 — Triangle Cube (4 sub-tests)
- `Scene008_TriangleCube_GoldenReference`
- `Scene008_TriangleCube_NonBlackPixelCount`
- `Scene008_TriangleCube_HasMultipleColors`

### Scene 009 — Triangle Cubes100 (6 sub-tests)
- `Scene009_TriangleCubes100_GoldenReference`
- `Scene009_TriangleCubes100_NonBlackPixelCount`
- `Scene009_TriangleCubes100_HasMultipleFaceColors`
- `Scene009_TriangleCubes100_CentralRegionHasCubes`
- `Scene009_TriangleCubes100_VertexCount`

### Scene 010 — Triangle SponzaStyle (7 sub-tests)
- `Scene010_TriangleSponzaStyle_RendersWithoutError`
- `Scene010_TriangleSponzaStyle_NonBlackPixelCount`
- `Scene010_TriangleSponzaStyle_FloorRegionHasColor`
- `Scene010_TriangleSponzaStyle_WallRegionsHaveColor`
- `Scene010_TriangleSponzaStyle_TriangleCount`
- `Scene010_TriangleSponzaStyle_UpperRegionHasCeiling`

### Scene 011 — PBR Material (6 sub-tests)
- `Scene011_PBRMaterial_RendersWithoutError`
- `Scene011_PBRMaterial_NonBlackPixelCount`
- `Scene011_PBRMaterial_HasMultipleColors`
- `Scene011_PBRMaterial_SphereRegionsVisible`
- `Scene011_PBRMaterial_TriangleCount`
- `Scene011_PBRMaterial_ColorDistribution`

### Scene 012 — Depth Complexity (7 sub-tests)
- `Scene012_DepthComplexity_AllTrianglesRender`
- `Scene012_DepthComplexity_FrontTriangleVisible`
- `Scene012_DepthComplexity_BackTriangleVisibleInEdges`
- `Scene012_DepthComplexity_PPMDump`
- `Scene012_DepthComplexity_ZBufferUpdated`
- `Scene012_DepthComplexity_SameDepthNoArtifacts`

### Scene 013 — Texture Sampling (6 sub-tests)
- `Scene013_TextureSampling_RendersWithoutCrash`
- `Scene013_TextureSampling_UVInterpolation`
- `Scene013_TextureSampling_PPMDump`
- `Scene013_TextureSampling_TextureBufferValid`
- `Scene013_TextureSampling_GoldenReference`

### Scene 014 — Textured Triangle (3 sub-tests)
- `Scene014_TexturedTriangle_GoldenReference`
- `Scene014_TexturedTriangle_NonBlackPixels`

## v1.4 → v2.0 Migration Notes

- **Old path:** `RenderPipeline` → `VertexShader` (C++ ISA emulation) → `FragmentShader` → `Rasterizer`
- **New path (v2.0):** `RenderPipeline` → `Interpreter_v2_5` (ISA interpreter) → `FragmentShader` → `Rasterizer`
- **Impact:** All 90 E2E tests will need regression after VertexShader migration is complete
- **Key risk areas:** Scene004 (MAD precision), Scene006 (warp scheduling), Scene013 (texture sampling)
