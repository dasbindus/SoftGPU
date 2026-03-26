# ============================================
# renderer 库 - 渲染器（header-only）
# ============================================
add_library(renderer INTERFACE)

target_link_libraries(renderer INTERFACE
    core
    imgui
)

target_include_directories(renderer INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
