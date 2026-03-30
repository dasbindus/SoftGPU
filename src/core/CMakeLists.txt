# ============================================
# core 库 - 通用类型和数学
# ============================================
add_library(core STATIC
    Common.hpp
    Math.hpp
    PipelineTypes.hpp
    RenderCommand.hpp
    MemorySubsystem.hpp
    MemorySubsystem.cpp
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(core PUBLIC glm)
