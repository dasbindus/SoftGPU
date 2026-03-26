# ============================================
# app 库 - 应用框架
# ============================================
add_library(app INTERFACE)
add_library(SoftGPU::app ALIAS app)

target_link_libraries(app INTERFACE
    core
    platform
    renderer
    stages
    pipeline
)

target_include_directories(app INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
