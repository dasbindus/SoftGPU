# ISA Module - SoftGPU Instruction Set Architecture
#
# v1.0 ISA - 28 Instructions (legacy)
# v2.5 ISA - 50+ Instructions (new unified ISA)

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

# ==============================================================================
# v2.5 ISA - Header-only implementation
# Files: isa_v2_5.hpp, register_file_v2_5.hpp, interpreter_v2_5.hpp
# Automatically included via target_include_directories above
# ==============================================================================
