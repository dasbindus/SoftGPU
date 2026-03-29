# ============================================
# app 库 - 应用框架
# ============================================
add_library(app STATIC
    Application.hpp
)

target_link_libraries(app PUBLIC
    core
    platform
    renderer
    stages
    pipeline
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
