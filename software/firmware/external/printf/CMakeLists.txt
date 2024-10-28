cmake_minimum_required(VERSION 3.20)

add_library(printf
    printf.c
)

target_include_directories(printf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
