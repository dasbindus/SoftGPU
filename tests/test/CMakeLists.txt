# ============================================
# tests/test - TestScene 单元测试
# ============================================

add_executable(test_test_scenarios
    TestScene_test.cpp
)

target_link_libraries(test_test_scenarios
    PRIVATE
    test
    benchmark
    core
    stages
    pipeline
    gtest
)

gtest_discover_tests(test_test_scenarios)
