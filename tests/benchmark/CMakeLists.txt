# ============================================
# tests/benchmark - BenchmarkRunner 单元测试
# ============================================

add_executable(test_benchmark_runner
    BenchmarkRunner_test.cpp
)

target_link_libraries(test_benchmark_runner
    PRIVATE
    test
    benchmark
    core
    stages
    pipeline
    gtest
)

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    target_link_libraries(test_benchmark_runner PRIVATE c++)
endif()

gtest_discover_tests(test_benchmark_runner)
