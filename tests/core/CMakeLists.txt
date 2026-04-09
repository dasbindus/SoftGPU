# ============================================================================
# SoftGPU - tests/core/CMakeLists.txt
# G1 Phase - P0-2/3/4 Test Framework
# ============================================================================

# Existing math test
add_executable(math_test math_test.cpp)

if(GTest_FOUND)
    target_link_libraries(math_test PRIVATE core GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(math_test PRIVATE core gtest gtest_main)
endif()

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    target_link_libraries(math_test PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(math_test)

# ---------------------------------------------------------------------------
# P0-2: ISA Tests (DIV Cycle Latency)
# ---------------------------------------------------------------------------
add_executable(test_isa test_isa.cpp)

if(GTest_FOUND)
    target_link_libraries(test_isa PRIVATE core isa pipeline GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_isa PRIVATE core isa pipeline gtest gtest_main)
endif()

if(APPLE)
    target_link_libraries(test_isa PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(test_isa)

# ---------------------------------------------------------------------------
# P0-3/4: Memory Tests (TokenBucket, L2 Cache)
# ---------------------------------------------------------------------------
add_executable(test_memory test_memory.cpp)

if(GTest_FOUND)
    target_link_libraries(test_memory PRIVATE core stages GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_memory PRIVATE core stages gtest gtest_main)
endif()

if(APPLE)
    target_link_libraries(test_memory PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(test_memory)

# ---------------------------------------------------------------------------
# P0-1: WarpScheduler Tests
# ---------------------------------------------------------------------------
add_executable(test_warp test_warp.cpp)

if(GTest_FOUND)
    target_link_libraries(test_warp PRIVATE core pipeline GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_warp PRIVATE core pipeline gtest gtest_main)
endif()

if(APPLE)
    target_link_libraries(test_warp PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(test_warp)

# ---------------------------------------------------------------------------
# P1: VS ISA Phase 1 Tests (NOP, HALT, JUMP, ADD, SUB, MUL, DIV,
#                           DOT3, NORMALIZE, MAT_MUL, VLOAD, VOUTPUT)
# ---------------------------------------------------------------------------
add_executable(test_vsisa_phase1 test_vsisa_phase1.cpp)

if(GTest_FOUND)
    target_link_libraries(test_vsisa_phase1 PRIVATE core isa pipeline GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_vsisa_phase1 PRIVATE core isa pipeline gtest gtest_main)
endif()

if(APPLE)
    target_link_libraries(test_vsisa_phase1 PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(test_vsisa_phase1)
