cmake_minimum_required(VERSION 3.12)
project(MySocketAPP C)
set(CMAKE_C_STANDARD 17)

add_compile_options(
        -pg
        -O
        #        -g
)

add_subdirectory(src/client)
add_subdirectory(src/server)
add_subdirectory(src/test)
