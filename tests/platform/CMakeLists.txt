add_executable(window_test window_test.cpp)

if(GTest_FOUND)
    set(GTEST_LIBS GTest::gtest GTest::gtest_main)
else()
    set(GTEST_LIBS gtest gtest_main)
endif()

target_link_libraries(window_test PRIVATE
    platform
    glfw
    OpenGL::GL
    ${GTEST_LIBS}
)

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    target_link_libraries(window_test PRIVATE c++)
endif()

include(GoogleTest)
gtest_discover_tests(window_test)
