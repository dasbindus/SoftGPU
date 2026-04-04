# ============================================
# pipeline 库 - 渲染管线组装
# ============================================
add_library(pipeline STATIC
    RenderPipeline.hpp
    RenderPipeline.cpp
    ShaderCore.hpp
    ShaderCore.cpp
    WarpScheduler.hpp
    WarpScheduler.cpp
    TextureBuffer.hpp
    TextureBuffer.cpp
)

target_link_libraries(pipeline PUBLIC
    stages
    core
    utils
    profiler
)

# pthread for std::thread in WarpScheduler
find_package(Threads REQUIRED)
target_link_libraries(pipeline PUBLIC Threads::Threads)

target_include_directories(pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
