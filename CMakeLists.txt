cmake_minimum_required(VERSION 3.20)
project(computer_enhance_lessons C)

add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-gnu-pointer-arith
        -g
)

add_subdirectory(util)
add_subdirectory(part-1/1-instruction-decoding)

add_executable(tests
    tests.c
)
target_link_libraries(tests PRIVATE
    util
)