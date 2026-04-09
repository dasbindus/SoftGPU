# ============================================
# stages 库 - 渲染管线各阶段
# ============================================
add_library(stages STATIC
    IStage.hpp
    CommandProcessor.hpp
    CommandProcessor.cpp
    VertexShader.hpp
    VertexShader.cpp
    VSOutputAssembler.hpp
    VSOutputAssembler.cpp
    PrimitiveAssembly.hpp
    PrimitiveAssembly.cpp
    Rasterizer.hpp
    Rasterizer.cpp
    FragmentShader.hpp
    FragmentShader.cpp
    Framebuffer.hpp
    Framebuffer.cpp
    TileWriteBack.hpp
    TileWriteBack.cpp
    TilingStage.hpp
    TilingStage.cpp
    TileBuffer.hpp
    TileBuffer.cpp
    EarlyZ.hpp
    EarlyZ.cpp
)

target_link_libraries(stages PUBLIC
    core
    pipeline
)

target_include_directories(stages PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
