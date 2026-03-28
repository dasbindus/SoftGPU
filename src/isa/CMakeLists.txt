# ISA Module - SoftGPU Instruction Set Architecture
#
# v1.0 ISA - 28 Instructions
# Provides instruction decode, execute, and interpretation for fragment shaders

add_library(isa STATIC
    Opcode.cpp
    Instruction.cpp
    Decoder.cpp
    RegisterFile.cpp
    ExecutionUnits.cpp
    Interpreter.cpp
)

target_include_directories(isa PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(isa PRIVATE
    ${SOFTGPU_CXX_FLAGS}
)

# Set C++ standard
target_compile_features(isa PRIVATE
    cxx_std_17
)

# Export for use
set_target_properties(isa PROPERTIES
    OUTPUT_NAME "softgpu_isa"
    POSITION_INDEPENDENT_CODE ON
)
