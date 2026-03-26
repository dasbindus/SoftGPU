add_executable(math_test math_test.cpp)

if(GTest_FOUND)
    target_link_libraries(math_test PRIVATE core GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(math_test PRIVATE core gtest gtest_main)
endif()

include(GoogleTest)
gtest_discover_tests(math_test)
