cmake_minimum_required(VERSION 3.20)
project(instruction_decoding C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The actual program
add_executable(instruction_decoding
        decoder.c          # or whatever your real main file is called
)
target_link_libraries(instruction_decoding PRIVATE
        util
)

# The test
add_executable(instruction_decoding_tests
        tests/instruction_decoding_tests.c
)
target_link_libraries(instruction_decoding_tests PRIVATE
        util
)
