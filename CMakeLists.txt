cmake_minimum_required(VERSION 3.20)
project(computer_enhance_lessons C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(
        -Wall
        -Wextra
        -g
)

add_subdirectory(util)
add_subdirectory(part-1/1-instruction-decoding)

add_executable(tests
    tests.c
    part-1/1-instruction-decoding/instruction_set/parse_instructions.c
)
target_link_libraries(tests PRIVATE
    util
)

target_compile_definitions(tests PRIVATE AGGREGATE_TESTS)
add_custom_target(test_all
    COMMAND tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Building and running all tests"
)
