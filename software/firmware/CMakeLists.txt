#######################################################################
# Initial setup parameters
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
cmake_minimum_required(VERSION 3.20)

# Package includes
find_package(Git REQUIRED)
find_package(Perl REQUIRED)
find_package(Python REQUIRED)

# Build configuration options
option(USE_SEGGER_RTT "Enable logging via SEGGER RTT" OFF)

# Specify cross-compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# Skip link step during compiler validation
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Pass object files to the linker as a single file parameter
SET(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
SET(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
SET(CMAKE_C_RESPONSE_FILE_LINK_FLAG "@")
SET(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG "@")
SET(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")

# Specify compiler variables
set (RUNTIME_LIBRARY "--specs=nano.specs")
set (RUNTIME_LIBRARY_SYSCALLS "--specs=nosys.specs")

# Specify common compile flags and options
set(CXX_FLAGS
    -fno-rtti
    -fno-exceptions
    -fno-threadsafe-statics
)
add_compile_options(
    -fdata-sections
    -ffunction-sections
    ${RUNTIME_LIBRARY}
    "$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS}>"
)

#######################################################################
# Project settings
project(uvdensitometer)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

# MCU Information
set(MCU_FAMILY STM32L0xx)
set(MCU_MODEL STM32L072xx)
set(CPU_PARAMETERS
    -mcpu=cortex-m0plus
    -mthumb
    -mfloat-abi=soft
)

set(STARTUP_SCRIPT ${PROJECT_DIR}/system/startup_stm32l072kztx.s)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32L072KZTX_FLASH.ld)

add_compile_options(
    ${CPU_PARAMETERS}
    -fstack-protector-strong
    "$<$<CONFIG:Debug>:-Os;-g3;-ggdb>"
    "$<$<CONFIG:Release>:-Os;-g0>"
)

add_compile_definitions(
    ${MCU_MODEL}
)

#######################################################################
# Build information variables
execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --dirty --exclude '*'
        OUTPUT_VARIABLE BUILD_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M")

#######################################################################

set(EXECUTABLE ${CMAKE_PROJECT_NAME})
enable_language(C ASM)

# Set linker output suffixes
set (CMAKE_EXECUTABLE_SUFFIX ".elf")
set (CMAKE_STATIC_LIBRARY_SUFFIX ".a")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Headers
set(PROJECT_INCLUDE_DIRECTORIES
    ${PROJECT_DIR}
    ${PROJECT_DIR}/system
)

# Sources
file(GLOB_RECURSE PROJECT_SOURCES FOLLOW_SYMLINKS
    ${PROJECT_DIR}/*.c)

# Executable files
add_executable(${EXECUTABLE}
    ${PROJECT_SOURCES}
    ${STARTUP_SCRIPT}
)

if(USE_SEGGER_RTT)
    add_subdirectory(${EXTERNAL_DIR}/RTT)
    target_compile_definitions(${EXECUTABLE} PRIVATE
        USE_SEGGER_RTT
        BUFFER_SIZE_UP=512
    )
    target_link_libraries(${EXECUTABLE} PRIVATE RTT)
endif()

# Subdirectories to include
add_subdirectory(${EXTERNAL_DIR}/drivers)
target_include_directories(hal PRIVATE ${PROJECT_INCLUDE_DIRECTORIES})

add_subdirectory(${EXTERNAL_DIR}/freertos)
target_include_directories(freertos PRIVATE ${PROJECT_INCLUDE_DIRECTORIES})

add_subdirectory(${EXTERNAL_DIR}/easylogger)
target_include_directories(easylogger PRIVATE ${PROJECT_INCLUDE_DIRECTORIES})

add_subdirectory(${EXTERNAL_DIR}/u8g2)

add_subdirectory(${EXTERNAL_DIR}/tinyusb)
target_include_directories(tinyusb PRIVATE ${PROJECT_INCLUDE_DIRECTORIES})

add_subdirectory(${EXTERNAL_DIR}/printf)

# Embedded macros(defines)
target_compile_definitions(${EXECUTABLE} PRIVATE
    USE_HAL_DRIVER
    STM32_THREAD_SAFE_STRATEGY=4
    $<$<CONFIG:DEBUG>:DEBUG>
    APP_BUILD_DATE="${BUILD_DATE}"
    APP_BUILD_DESCRIBE="${BUILD_DESCRIBE}"
    EMC_TEST
)

# Add header directories (AFTER add_executable)
target_include_directories(${EXECUTABLE} PUBLIC
    ${PROJECT_INCLUDE_DIRECTORIES}
)

# Compiler and linker options
target_compile_options(${EXECUTABLE} PRIVATE
    -Wall
    -fstack-usage
    -fcyclomatic-complexity
    -Wformat-overflow
    -Wmaybe-uninitialized
    -Wno-unused-parameter
    $<$<COMPILE_LANGUAGE:CXX>:
        -Wno-volatile
        -Wold-style-cast
        -Wuseless-cast
        -Wsuggest-override>
)

target_link_libraries(${EXECUTABLE} PRIVATE
    cmsis
    hal
    freertos
    easylogger
    u8g2
    tinyusb
    printf

    -T"${MCU_LINKER_SCRIPT}"
    ${CPU_PARAMETERS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -Wl,--gc-sections
    -static
    ${RUNTIME_LIBRARY}
    ${RUNTIME_LIBRARY_SYSCALLS}
    -Wl,--start-group
    -lc
    -lm
    -Wl,--end-group
    -Wl,--print-memory-usage
)

add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/codesize.pl -s ${CMAKE_SIZE} $<TARGET_FILE:${EXECUTABLE}>
)

# Post-build commands to embed the checksum
add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    DEPENDS ${EXECUTABLE}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE}.elf ${EXECUTABLE}.bin
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/checksum.pl ${EXECUTABLE}.bin ${EXECUTABLE}-desc.dat
    COMMAND ${CMAKE_OBJCOPY} --update-section .app_descriptor=${EXECUTABLE}-desc.dat ${EXECUTABLE}.elf ${EXECUTABLE}-out.elf
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE}-out.elf ${EXECUTABLE}-out.bin
    BYPRODUCTS ${EXECUTABLE}-desc.dat
)

add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    DEPENDS ${EXECUTABLE}-out.bin
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/dfuse-pack.py -b 0x08008000:${EXECUTABLE}-out.bin -D 0x0483:0xdf11 -a 0 ${EXECUTABLE}.dfu
)

add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    DEPENDS ${EXECUTABLE}-out.bin
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/uf2conv.py -f 0x202E3A91 -b 0x08008000 -c -o ${EXECUTABLE}.uf2 ${EXECUTABLE}-out.bin
)
