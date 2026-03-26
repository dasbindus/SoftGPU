cmake_minimum_required(VERSION 3.16)
project(SoftGPU VERSION 0.1.0 LANGUAGES CXX)

# ============================================
# C++ 标准
# ============================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ============================================
# FetchContent - only for packages not on system
# ============================================
include(FetchContent)

# GLFW - use system if available
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM - use system if available (check for header)
set(_GLM_SYSTEM_INCLUDE "/usr/include")
if(EXISTS "${_GLM_SYSTEM_INCLUDE}/glm/glm.hpp")
    set(GLM_FOUND TRUE)
    message(STATUS "Using system GLM: ${_GLM_SYSTEM_INCLUDE}")
else()
    set(GLM_FOUND FALSE)
endif()

if(NOT GLM_FOUND)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

# imgui - use extern/ directory
add_subdirectory(extern/imgui)

# GoogleTest - use system if available
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ============================================
# 查找 OpenGL 依赖
# ============================================
find_package(OpenGL REQUIRED)

# glad - OpenGL loader (本地源码)
add_subdirectory(extern/glad)

# ============================================
# 头文件路径
# ============================================
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/glad/include
)

# Add GLM include path if using system GLM
if(GLM_FOUND)
    include_directories(${_GLM_SYSTEM_INCLUDE})
endif()

# ============================================
# 子目录
# ============================================
add_subdirectory(src)
add_subdirectory(tests)

# ============================================
# 编译选项
# ============================================
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================
# 主程序
# ============================================
add_executable(SoftGPU
    src/main.cpp
)

target_link_libraries(SoftGPU PRIVATE
    app
    platform
    renderer
    stages
    pipeline
    test
    OpenGL::GL
    glfw
    glad
    dl
    imgui
)

# Windows: 链接 Windows 库
if(WIN32)
    target_link_libraries(SoftGPU PRIVATE
        dwmapi.lib
        imm32.lib
    )
endif()
