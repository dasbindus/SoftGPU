# ============================================
# test - 测试场景模块
# ============================================

set(TEST_SOURCES
    TestScene.cpp
    TestSceneBuilder.cpp
    OBJModelScene.hpp
    OBJModelScene.cpp
)

add_library(test STATIC ${TEST_SOURCES})

target_include_directories(test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test
    PUBLIC
    core
    stages
    utils
    glm::glm
)
