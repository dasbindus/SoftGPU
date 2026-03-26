# ============================================
# benchmark - Benchmark 自动化模块
# ============================================

set(BENCHMARK_SOURCES
    BenchmarkResult.cpp
    BenchmarkRunner.cpp
)

add_library(benchmark STATIC ${BENCHMARK_SOURCES})

target_include_directories(benchmark PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(benchmark PUBLIC
    test
    pipeline
    core
    stages
)
