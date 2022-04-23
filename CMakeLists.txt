

cmake_minimum_required(VERSION 2.7)

project(9_20ve482p1_pipe)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic -Wno-unused-result")
file(GLOB SOURCE_FILES "*.c")

add_executable(mumsh  mumsh.c)
add_executable(mumsh_memory_check  mumsh.c)

target_compile_options(mumsh_memory_check PUBLIC -fsanitize=address,undefined,integer, -fno-omit-frame-pointer)
target_link_libraries(mumsh_memory_check -fsanitize=address,undefined,integer)
