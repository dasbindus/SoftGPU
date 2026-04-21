# ============================================
# E2E Tests - End-to-End Scene Tests
# ============================================
if(GTest_FOUND)
    set(GTEST_LIBS GTest::gtest GTest::gtest_main)
else()
    set(GTEST_LIBS gtest gtest_main)
endif()

add_executable(test_e2e
    E2E_Framework.cpp
    scene_001_green_triangle.cpp
    scene_002_rgb_interpolation.cpp
    scene_003_depth_test.cpp
    scene_004_mad_verification.cpp
    scene_005_multi_triangle.cpp
    scene_006_warp_scheduling.cpp
    scene_007_triangle_1tri.cpp
    scene_008_triangle_cube.cpp
    scene_009_triangle_cubes100.cpp
    scene_010_triangle_sponzastyle.cpp
    scene_011_pbr_material.cpp
    scene_012_depth_complexity.cpp
    scene_013_texture_sampling.cpp
    scene_014_textured_triangle.cpp
    scene_015_textured_teapot.cpp
    scene_016_obj_model.cpp
    scene_017_triangle_strip.cpp
    scene_018_primitive_restart.cpp
)
target_link_libraries(test_e2e PRIVATE stages pipeline core test ${GTEST_LIBS})

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    target_link_libraries(test_e2e PRIVATE c++)
endif()
include(GoogleTest)
gtest_discover_tests(test_e2e)

# Copy golden reference files to build directory for tests
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/golden/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/golden)

# Copy OBJ model files to build directory for tests
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/models/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/models)
