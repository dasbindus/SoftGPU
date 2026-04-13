# ISA Module - SoftGPU Instruction Set Architecture
#
# v2.5 ISA - Header-only implementation (50+ instructions)

add_library(isa INTERFACE)

target_include_directories(isa INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ==============================================================================
# v2.5 ISA - Header-only implementation
# Files: isa_v2_5.hpp, register_file_v2_5.hpp, interpreter_v2_5.hpp
# ==============================================================================
