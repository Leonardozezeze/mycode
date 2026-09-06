cmake_minimum_required(VERSION 3.22)

# ==============================================================================
# 交叉编译工具链设置(必须在 project() 之前)
# ==============================================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# 交叉编译:跳过链接器测试,避免 try_compile 失败
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 允许通过 -DTOOLCHAIN_PATH=... 指定工具链安装目录
if(DEFINED TOOLCHAIN_PATH)
    set(CMAKE_C_COMPILER   ${TOOLCHAIN_PATH}/${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/${CMAKE_CXX_COMPILER})
    set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}/${CMAKE_ASM_COMPILER})
    set(CMAKE_OBJCOPY      ${TOOLCHAIN_PATH}/${CMAKE_OBJCOPY})
    set(CMAKE_SIZE         ${TOOLCHAIN_PATH}/${CMAKE_SIZE})
endif()

project(MyStm32F407Project LANGUAGES C ASM)

# ==============================================================================
# 构建类型与 IDE 支持
# ==============================================================================
# 默认 Debug(-Og -g3),可用 -DCMAKE_BUILD_TYPE=Release 切换(-Os)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_C_FLAGS_DEBUG   "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_ASM_FLAGS_DEBUG   "-g3")
set(CMAKE_ASM_FLAGS_RELEASE "")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 为 clangd / LSP 生成 compile_commands.json

# ==============================================================================
# MCU 与链接脚本
# ==============================================================================
set(MCU_FAMILY    STM32F407xx)
set(MCU_FLAGS     -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/STM32F407XX_FLASH.ld)

# ==============================================================================
# 源文件收集(目录缺失时对应 GLOB 结果为空,自动跳过)
# ==============================================================================

# 用户应用源码 (Core/Src: main/bsp/app + 中断 + MSP + syscalls,含 OD.c)
file(GLOB CORE_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Core/Src/*.c)

# CMSIS 系统源码 + 启动文件(.s 与 .ld/.svd 同放在 CMSIS/Src)
file(GLOB SYSTEM_SOURCES   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/*.c)
set(STARTUP_SOURCE ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Src/startup_stm32f407xx.s)

# HAL 驱动库源码(排除 *_template.c;Legacy 目录不参与编译)
file(GLOB HAL_SOURCES      CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/HAL/Src/*.c)
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\\.c$")

# FreeRTOS 内核源码 (无中间件时为空)
file(GLOB FREERTOS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/FreeRTOS/Src/*.c
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/FreeRTOS/portable/*.c
)

# CANopenNode 协议栈源码 (无中间件时为空)
# 移植层 CANopenNode_STM32 用非递归 GLOB(顶层 .c);协议栈按 301/303/... 分目录 GLOB。
file(GLOB CANOPEN_301_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/301/*.c)
file(GLOB CANOPEN_303_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/303/*.c)
file(GLOB CANOPEN_304_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/304/*.c)
file(GLOB CANOPEN_305_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/305/*.c)
file(GLOB CANOPEN_309_SOURCES     CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/309/*.c)
file(GLOB CANOPEN_EXTRA_SOURCES   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/extra/*.c)
file(GLOB CANOPEN_STORAGE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/storage/*.c)
file(GLOB CANOPEN_STM32_SOURCES   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/CANopenNode_STM32/*.c)

file(GLOB CANOPEN_MAIN_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/CANopen.c)

# 用户自定义驱动 (Drivers/User,一个外设一对 .c/.h)
file(GLOB USER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Drivers/User/*.c)

# ==============================================================================
# 可执行文件
# ==============================================================================
add_executable(${PROJECT_NAME}.elf
    ${CORE_SOURCES}
    ${SYSTEM_SOURCES}
    ${HAL_SOURCES}
    ${FREERTOS_SOURCES}
    ${CANOPEN_301_SOURCES}
    ${CANOPEN_303_SOURCES}
    ${CANOPEN_304_SOURCES}
    ${CANOPEN_305_SOURCES}
    ${CANOPEN_309_SOURCES}
    ${CANOPEN_EXTRA_SOURCES}
    ${CANOPEN_STORAGE_SOURCES}
    ${CANOPEN_MAIN_SOURCES}
    ${CANOPEN_STM32_SOURCES}
    ${USER_SOURCES}
    ${STARTUP_SOURCE}
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/HAL/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/HAL/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/FreeRTOS/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/FreeRTOS/portable
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode
    ${CMAKE_SOURCE_DIR}/Drivers/Middleware/CANopenNode/CANopenNode_STM32
    ${CMAKE_SOURCE_DIR}/Drivers/User
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    ${MCU_FAMILY}
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
)

# ==============================================================================
# 链接选项
# ==============================================================================
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    --specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--print-memory-usage
)

# ==============================================================================
# 生成 .hex / .bin 并打印体积
# ==============================================================================
set(HEX_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating ${PROJECT_NAME}.hex / ${PROJECT_NAME}.bin and size report"
)
