# imgui CMakeLists.txt
add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_glfw.cpp
    backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# Link GLFW if available and propagate include dirs
# Note: glfw3 package provides 'glfw' target
if(glfw3_FOUND)
    target_link_libraries(imgui PUBLIC glfw)
elseif(glfw_FOUND)
    target_link_libraries(imgui PUBLIC glfw)
endif()
