# ============================================
# stages tests
# ============================================
if(GTest_FOUND)
    set(GTEST_LIBS GTest::gtest GTest::gtest_main)
else()
    set(GTEST_LIBS gtest gtest_main)
endif()

# Fix macOS linker issue with GTest requiring explicit C++ stdlib
if(APPLE)
    set(APPLE_CXX_LIBS c++)
else()
    set(APPLE_CXX_LIBS "")
endif()

add_executable(test_VertexShader test_VertexShader.cpp)
target_link_libraries(test_VertexShader PRIVATE stages core ${GTEST_LIBS} ${APPLE_CXX_LIBS})
include(GoogleTest)
gtest_discover_tests(test_VertexShader)

add_executable(test_PrimitiveAssembly test_PrimitiveAssembly.cpp)
target_link_libraries(test_PrimitiveAssembly PRIVATE stages core ${GTEST_LIBS} ${APPLE_CXX_LIBS})
include(GoogleTest)
gtest_discover_tests(test_PrimitiveAssembly)

add_executable(test_Rasterizer test_Rasterizer.cpp)
target_link_libraries(test_Rasterizer PRIVATE stages core ${GTEST_LIBS} ${APPLE_CXX_LIBS})
include(GoogleTest)
gtest_discover_tests(test_Rasterizer)

add_executable(test_Framebuffer test_Framebuffer.cpp)
target_link_libraries(test_Framebuffer PRIVATE stages core ${GTEST_LIBS} ${APPLE_CXX_LIBS})
include(GoogleTest)
gtest_discover_tests(test_Framebuffer)

add_executable(test_Integration test_Integration.cpp)
target_link_libraries(test_Integration PRIVATE stages pipeline core ${GTEST_LIBS} ${APPLE_CXX_LIBS})
include(GoogleTest)
gtest_discover_tests(test_Integration)
