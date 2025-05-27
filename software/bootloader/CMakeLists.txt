#######################################################################
# Initial setup parameters
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)
cmake_minimum_required(VERSION 3.20)

# Package includes
find_package(Perl REQUIRED)
find_package(Python REQUIRED)

# Specify cross-compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(CMAKE_SIZE arm-none-eabi-size)

# Skip link step during compiler validation
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Pass object files to the linker as a single file parameter
SET(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
SET(CMAKE_C_RESPONSE_FILE_LINK_FLAG "@")
SET(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")

# Specify compiler variables
set (RUNTIME_LIBRARY "--specs=nano.specs")
set (RUNTIME_LIBRARY_SYSCALLS "--specs=nosys.specs")

# Specify common compile flags and options
add_compile_options(
    -fdata-sections
    -ffunction-sections
    ${RUNTIME_LIBRARY}
)

#######################################################################
# Project settings
project(uvdensitometer-bootloader)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

# MCU Information
set(MCU_FAMILY STM32L0xx)
set(MCU_MODEL STM32L072xx)
set(CPU_PARAMETERS
    -mcpu=cortex-m0plus
    -mthumb
    -mfloat-abi=soft)

set(STARTUP_SCRIPT ${PROJECT_DIR}/system/startup_stm32l072kztx.s)
set(MCU_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/STM32L072KZTX_FLASH.ld)

#######################################################################

set(EXECUTABLE ${CMAKE_PROJECT_NAME})
enable_language(C ASM)

# Set linker output suffixes
set (CMAKE_EXECUTABLE_SUFFIX ".elf")
set (CMAKE_STATIC_LIBRARY_SUFFIX ".a")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Headers
set(PROJECT_INCLUDE_DIRECTORIES
    ${PROJECT_DIR}
    ${PROJECT_DIR}/system)
set(EXTERNAL_INCLUDE_DIRECTORIES
    ${EXTERNAL_DIR}/drivers/stm32l0xx/include
    ${EXTERNAL_DIR}/drivers/stm32l0xx/include/Legacy
    ${EXTERNAL_DIR}/drivers/cmsis/include
    ${EXTERNAL_DIR}/tinyusb/src)

# Sources
file(GLOB_RECURSE PROJECT_SOURCES FOLLOW_SYMLINKS
    ${PROJECT_DIR}/*.c)
file(GLOB_RECURSE EXTERNAL_SOURCES
    ${EXTERNAL_DIR}/drivers/*.c
    ${EXTERNAL_DIR}/tinyusb/src/*.c)

# Executable files
add_executable(${EXECUTABLE}
    ${EXTERNAL_SOURCES}
    ${PROJECT_SOURCES}
    ${STARTUP_SCRIPT})

# Embedded macros(defines)
target_compile_definitions(${EXECUTABLE} PRIVATE
    ${MCU_MODEL}
    USE_HAL_DRIVER
    $<$<CONFIG:DEBUG>:DEBUG>
    APP_BUILD_DATE="${BUILD_DATE}"
    APP_BUILD_DESCRIBE="${BUILD_DESCRIBE}")

# Add header directories (AFTER add_executable)
target_include_directories(${EXECUTABLE} PRIVATE
    ${EXTERNAL_INCLUDE_DIRECTORIES}
    ${PROJECT_INCLUDE_DIRECTORIES})

# Compiler and linker options
target_compile_options(${EXECUTABLE} PRIVATE
    ${CPU_PARAMETERS}
    -Wall
    -fstack-usage
    -fcyclomatic-complexity
    -Wformat-overflow
    -Wmaybe-uninitialized
    -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Os -g3 -ggdb>
    $<$<CONFIG:Release>:-Os -g0>)

target_link_libraries(${EXECUTABLE} PRIVATE
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
    -Wl,--print-memory-usage)

add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/codesize.pl -s ${CMAKE_SIZE} $<TARGET_FILE:${EXECUTABLE}>)

# Post-build commands
add_custom_command(TARGET ${EXECUTABLE} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${EXECUTABLE}.elf ${EXECUTABLE}.bin
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/dfuse-pack.py -b 0x08000000:${EXECUTABLE}.bin -D 0x0483:0xdf11 -a 0 ${EXECUTABLE}.dfu
)
