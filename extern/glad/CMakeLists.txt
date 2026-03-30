# glad CMakeLists.txt
# This CMakeLists.txt builds glad from source

# Check if glad is already installed system-wide
find_package(glad QUIET)

if(glad_FOUND)
    message(STATUS "Using system-installed glad")
    add_library(glad INTERFACE)
    target_include_directories(glad INTERFACE ${glad_INCLUDE_DIRS})
else()
    message(STATUS "Building glad from source")

    # Enable C language for glad.c compilation
    enable_language(C)

    # Create library from source
    add_library(glad STATIC
        src/glad.c
    )

    target_include_directories(glad PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Link against GLFW if available
    # Note: glfw3 package provides 'glfw' target
    if(glfw3_FOUND)
        target_link_libraries(glad PUBLIC glfw)
    elseif(glfw_FOUND)
        target_link_libraries(glad PUBLIC glfw)
    endif()

    # Use system OpenGL if available
    if(OpenGL_FOUND)
        target_link_libraries(glad PUBLIC
            ${OPENGL_gl_LIBRARY}
        )
    endif()
endif()

# Set version info
set_target_properties(glad PROPERTIES
    VERSION 2.0.0
    SOVERSION 2
)
