cmake_minimum_required(VERSION 3.20)
project(utils C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(util STATIC
        virtual_arena/virtual_arena.c
        file_loader/file_loader.c
)
target_include_directories(util PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# The test
add_executable(util_tests
        _tests/util_tests.c
)
target_link_libraries(util_tests PRIVATE
        util
)
