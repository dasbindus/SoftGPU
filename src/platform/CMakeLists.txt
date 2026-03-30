# ============================================
# platform 库 - 窗口和输入
# ============================================
add_library(platform STATIC
    Window.cpp
    Window.hpp
)

# Use glfw from FetchContent if available, otherwise find system package
if(NOT TARGET glfw)
    if(NOT glfw3_FOUND)
        find_package(glfw3 REQUIRED)
    endif()
endif()

target_link_libraries(platform PUBLIC
    core
    glfw
    OpenGL::GL
)

target_include_directories(platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
