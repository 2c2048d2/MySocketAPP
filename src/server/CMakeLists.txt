cmake_minimum_required(VERSION 3.12)
project(MySocketAPP_Server C)

include_directories(${CMAKE_SOURCE_DIR}/src/common/include)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/common COMMON_SRC)

add_executable(server
        main.c
        ${COMMON_SRC}
)