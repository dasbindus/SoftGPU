# ============================================
# pipeline 库 - 渲染管线组装
# ============================================
add_library(pipeline STATIC
    RenderPipeline.hpp
    RenderPipeline.cpp
    ShaderCore.hpp
    ShaderCore.cpp
)

target_link_libraries(pipeline PUBLIC
    stages
    core
    utils
    profiler
)

target_include_directories(pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
