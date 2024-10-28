cmake_minimum_required(VERSION 3.20)

#file(GLOB_RECURSE COMPONENT_SOURCES src/*.c)

file(GLOB COMPONENT_SOURCES
    src/*.c
    src/common/*.c
    src/device/*.c
    src/portable/st/stm32_fsdev/*.c
    src/class/cdc/*.c
    src/class/hid/*.c
)

add_library(tinyusb ${COMPONENT_SOURCES})

target_include_directories(tinyusb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tinyusb PRIVATE
    cmsis
    freertos
)
