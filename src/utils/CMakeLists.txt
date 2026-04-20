# ============================================
# utils 库 - 工具类（FrameDumper 等）
# ============================================
add_library(utils STATIC
    FrameDumper.hpp
    FrameDumper.cpp
    OBJLoader.hpp
    OBJLoader.cpp
)

target_include_directories(utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
