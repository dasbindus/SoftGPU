# ============================================================================
# SoftGPU - tests/isa/CMakeLists.txt
# Phase 1 ISA Golden Test Framework
# ============================================================================

# Golden ISA Test Suite (Phase 1)
add_executable(test_golden_isa test_golden_isa.cpp)

if(GTest_FOUND)
    target_link_libraries(test_golden_isa PRIVATE core isa pipeline GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_golden_isa PRIVATE core isa pipeline gtest gtest_main)
endif()

if(APPLE)
    target_link_libraries(test_golden_isa PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(test_golden_isa)
