add_executable(window_test window_test.cpp)

target_link_libraries(window_test PRIVATE
    platform
    glfw
    OpenGL::GL
    gtest
)

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    target_link_libraries(window_test PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(window_test)
