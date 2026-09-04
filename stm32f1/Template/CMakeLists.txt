cmake_minimum_required(VERSION 3.22)

# ------------------------------------------------------------------
# 交叉编译工具链设置(必须在 project() 之前)
# ------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# 交叉编译时跳过链接器测试,避免 try_compile 失败
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 允许通过 -DTOOLCHAIN_PATH=... 指定工具链安装目录
if(DEFINED TOOLCHAIN_PATH)
    set(CMAKE_C_COMPILER   ${TOOLCHAIN_PATH}/${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/${CMAKE_CXX_COMPILER})
    set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}/${CMAKE_ASM_COMPILER})
    set(CMAKE_OBJCOPY      ${TOOLCHAIN_PATH}/${CMAKE_OBJCOPY})
    set(CMAKE_SIZE         ${TOOLCHAIN_PATH}/${CMAKE_SIZE})
endif()

project(MyStm32F103Project LANGUAGES C ASM)

# ------------------------------------------------------------------
# MCU 与链接脚本
# ------------------------------------------------------------------
set(MCU_FAMILY      STM32F103xB)
set(MCU_FLAGS       -mcpu=cortex-m3 -mthumb)
set(LINKER_SCRIPT   ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/STM32F103XB_FLASH.ld)

# 默认 Debug(-Og -g3),可用 -DCMAKE_BUILD_TYPE=Release 切换(-Os)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_C_FLAGS_DEBUG   "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_ASM_FLAGS_DEBUG   "-g3")
set(CMAKE_ASM_FLAGS_RELEASE "")

# ------------------------------------------------------------------
# 源文件
# ------------------------------------------------------------------
file(GLOB CORE_SOURCES ${CMAKE_SOURCE_DIR}/Core/Src/*.c)

# HAL 库源码(排除模板文件与 Legacy 目录)
file(GLOB HAL_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/HAL/Src/*.c)
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\\.c$")

set(STARTUP_SOURCE ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/startup_stm32f103xb.s)
set(SYSTEM_SOURCE ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/system_stm32f1xx.c)

add_executable(${PROJECT_NAME}.elf
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_SOURCE}
    ${SYSTEM_SOURCE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/HAL/Inc
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    ${MCU_FAMILY}
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    -fdata-sections
    -ffunction-sections
)

# ------------------------------------------------------------------
# 链接选项
# ------------------------------------------------------------------
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--print-memory-usage
)

# ------------------------------------------------------------------
# 生成 .hex / .bin 并打印体积
# ------------------------------------------------------------------
set(HEX_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.hex / ${PROJECT_NAME}.bin and size report"
)
