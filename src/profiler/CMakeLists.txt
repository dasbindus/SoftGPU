# ============================================
# profiler 库 - 性能分析和瓶颈检测
# PHASE5
# ============================================
add_library(profiler STATIC
    IProfiler.hpp
    FrameProfiler.hpp
    FrameProfiler.cpp
    BottleneckDetector.hpp
    BottleneckDetector.cpp
    ProfilerUI.hpp
    ProfilerUI.cpp
)

target_link_libraries(profiler PUBLIC
    core
    imgui
)

target_include_directories(profiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern
)
