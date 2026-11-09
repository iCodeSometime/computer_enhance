cmake_minimum_required(VERSION 3.20)
project(instruction_decoding C)

# The actual program
add_executable(instruction_decoding
        decoder.c          # or whatever your real main file is called
)
target_link_libraries(instruction_decoding PRIVATE
        util
)

# The test
add_executable(instruction_decoding_tests
        _tests/instruction_decoding_tests.c
        instruction_set/parse_instructions.c
)
target_link_libraries(instruction_decoding_tests PRIVATE
        util
)
